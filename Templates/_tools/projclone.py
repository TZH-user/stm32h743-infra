#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""projclone.py —— STM32 工程克隆工具(阶段 A,单文件、纯标准库、Python 3.10+)

用途:把一个 CubeMX+CMake 工程克隆为新工程:
  1) 递归字节级复制(shutil.copy2 保时间戳),应用排除规则(build 缓存/备份/.bak 等),
     绝不修改源工程;
  2) --new-name 时同步三处工程名:<旧名>.ioc 文件名与内容(ProjectManager.ProjectName /
     ProjectFileName)、根 CMakeLists.txt 的 set(CMAKE_PROJECT_NAME ...)、目标目录名
     (由 --dst 末段决定);根目录 README* 属"工程身份文件",一并全文替换;
     其余文件一律不改,只扫描并报告旧名残留,由人决定是否处理;
  3) 自动校验:逐顶级目录文件对账(动态,源过滤后 vs 目标)、关键文件存在性(profile
     清单)、残留扫描(豁免区单列),输出中文报告(默认 <dst>\\克隆报告.md)。

exit code:0 = 校验全过且零残留
          1 = 有残留(行内容或文件名命中旧名,豁免区/非豁免区分段列出)
          2 = 失败(缺关键文件 / 对账不符 / 替换计数校验失败 / 源遍历错误 /
              未预期异常 / 拒绝类:目标已存在无 --force、src 与 dst 互相嵌套、
              新名非法或为 Windows 保留名、多级排除模式等)

已知行为边界:残留扫描的行内容匹配区分大小写(grep 语义),文件名匹配不区分
(Windows 语义);排除模式 `*.bak*` 理论上会命中 `x.baker.md` 这类罕见名(被排除
的文件都列在报告中,可人工复核);旧名短于 5 字符时注释/README 不做替换只报残留。

示例(PowerShell):
  # 从模板派生新工程(最常用)
  python projclone.py --src F:\\STM32\\Templates\\H743_UI_Instrument `
      --dst F:\\STM32\\stm32project\\MyExp --new-name MyExp --profile template-derive
  # 纯净归档副本
  python projclone.py --src <旧工程> --dst <归档路径> --profile raw
  # 先看会发生什么
  python projclone.py --src ... --dst ... --new-name X --dry-run

详见策划案:F:\\STM32\\Templates\\工程克隆工具ProjClone_策划案.md
"""

from __future__ import annotations

import argparse
import fnmatch
import os
import re
import shutil
import sys
from datetime import datetime
from pathlib import Path

VERSION = "1.1.0"

# ---------------------------------------------------------------------------
# PROFILES 常量区
# ---------------------------------------------------------------------------

# 默认排除集(实践总结):尾部带 / 的按"目录名"匹配(任意层级),否则按"文件名"匹配
DEFAULT_EXCLUDES = [
    "build/", "*/build/", ".cache/", ".codegraph/",
    "Backups_UTF-8/", "备份*/", "*_backup_*/", "_ai_backup_*/",
    "_codex_flash_build_*/", "*.bak*", "mx.scratch",
    "preview-build/", "preview-bin/", ".git/",
]

# raw:纯净复制,只排缓存
RAW_EXCLUDES = ["build/", ".cache/", ".codegraph/"]

# CubeMX+CMake 工程的关键文件清单(相对目标根的 glob 模式)
REQUIRED_CUBEMX = [
    "*.ioc",
    "CMakeLists.txt",
    "CMakePresets.json",
    "startup_*.s",
    "*.ld",
    "cmake/stm32cubemx/CMakeLists.txt",
]

PROFILES = {
    "cubemx-cmake": {
        "desc": "CubeMX+CMake 工程克隆(默认)",
        "excludes": DEFAULT_EXCLUDES,
        "required": REQUIRED_CUBEMX,
    },
    "template-derive": {
        "desc": "从 Templates 派生新项目",
        "excludes": DEFAULT_EXCLUDES,
        "required": REQUIRED_CUBEMX + ["README*"],
    },
    "raw": {
        "desc": "纯净复制,只排缓存",
        "excludes": RAW_EXCLUDES,
        "required": [],
    },
}

# 残留扫描的文本文件白名单后缀(其余文件视为二进制,只复制不扫描)
TEXT_SUFFIXES = {".c", ".h", ".txt", ".md", ".cmake", ".ioc", ".json",
                 ".ld", ".s", ".xml", ".yml", ".yaml"}

# 默认豁免路径(LVGL Editor 生成库改名必须走 Editor 重新 Generate,不做文本替换)
# 两条模式分别覆盖"工程根下"与"任意子目录下"的 UI/Designer(fnmatch 模式锚定根,* 可跨 /)
DEFAULT_KEEP_NAME_IN = ["UI/Designer/**", "*/UI/Designer/**"]

# 路径长度警告阈值(Windows MAX_PATH=260)
PATH_WARN_LIMIT = 259


# ---------------------------------------------------------------------------
# 排除规则
# ---------------------------------------------------------------------------

def normalize_excludes(patterns: list[str], keep_git: bool) -> tuple[list[str], list[str]]:
    """把排除模式归一化为 (目录名模式, 文件名模式) 两组,全部小写。

    尾部带 / 的是目录模式;"*/x/" 与 "x/" 等价(都按目录名匹配,天然覆盖任意层级)。
    """
    dir_pats: list[str] = []
    file_pats: list[str] = []
    for raw in patterns:
        p = raw.strip().replace("\\", "/")
        if not p:
            continue
        is_dir = p.endswith("/")
        p = p.rstrip("/")
        if p.startswith("*/"):          # */build 与 build 同义
            p = p[2:]
        if "/" in p:
            # 匹配语义是"单个目录名/文件名对任意层级生效",路径级模式无法表达且
            # 会静默不生效——显式拒绝,防止用户以为排除了实际没排除
            fail(f"排除模式仅支持单级目录名/文件名(fnmatch),不支持路径级模式:{raw}\n"
                 f"提示:'build/' 已可匹配任意层级的 build 目录,无需写路径前缀")
        p = p.lower()
        if keep_git and p == ".git":
            continue
        if is_dir:
            if p not in dir_pats:
                dir_pats.append(p)
        else:
            if p not in file_pats:
                file_pats.append(p)
    return dir_pats, file_pats


def name_matches(name: str, pats: list[str]) -> bool:
    """单个目录名/文件名与模式组做 fnmatch(统一小写,Windows 大小写不敏感)。"""
    n = name.lower()
    return any(fnmatch.fnmatchcase(n, p) for p in pats)


def rel_matches(rel_posix: str, pats: list[str]) -> bool:
    """相对路径(正斜杠)与豁免模式匹配,如 "UI/Designer/**"。"""
    r = rel_posix.replace("\\", "/").lower()
    return any(fnmatch.fnmatchcase(r, p.replace("\\", "/").lower()) for p in pats)


# ---------------------------------------------------------------------------
# 克隆
# ---------------------------------------------------------------------------

# os.path.isjunction 是 Python 3.12 新增;更早版本退化为不检测(junction 会被深入)
_isjunction = getattr(os.path, "isjunction", lambda _p: False)


def apply_excludes(src: Path, dir_pats: list[str], file_pats: list[str]):
    """遍历目录树并应用排除规则,产出文件清单(克隆 / 对账 / 残留扫描共用)。

    junction 与符号链接一律不深入、不复制——防止把共享库物化、防链接环无限膨胀。
    返回 (文件相对路径列表, 排除目录, 排除文件, 跳过的链接, 遍历错误)。
    """
    copy_rels: list[Path] = []
    excluded_dirs: list[str] = []
    excluded_files: list[str] = []
    link_skips: list[str] = []
    walk_errors: list[str] = []

    def _onerr(err: OSError) -> None:
        # os.walk 默认 onerror=None 会静默丢掉整个无权限子目录,克隆不完整却
        # "校验全过"——必须收集并让上层判失败
        walk_errors.append(f"{getattr(err, 'filename', '?')} — {err}")

    for root, dirs, files in os.walk(src, onerror=_onerr):
        root_p = Path(root)
        kept = []
        for d in sorted(dirs):
            dp = root_p / d
            rel = str(dp.relative_to(src))
            if dp.is_symlink() or _isjunction(dp):
                link_skips.append(rel + os.sep)
            elif name_matches(d, dir_pats):
                excluded_dirs.append(rel)
            else:
                kept.append(d)
        dirs[:] = kept                      # 剪枝:被排除/链接目录不再深入
        for f in sorted(files):
            fp = root_p / f
            rel = fp.relative_to(src)
            if fp.is_symlink():
                link_skips.append(str(rel))
            elif name_matches(f, file_pats):
                excluded_files.append(str(rel))
            else:
                copy_rels.append(rel)
    return copy_rels, excluded_dirs, excluded_files, link_skips, walk_errors


def clone(src: Path, dst: Path, copy_rels: list[Path], dry_run: bool):
    """按清单逐文件 shutil.copy2(保时间戳)。返回 (复制数, 失败列表, 超长路径警告)。"""
    long_paths: list[str] = []
    failures: list[str] = []
    copied = 0
    for rel in copy_rels:
        target = dst / rel
        if len(str(target)) > PATH_WARN_LIMIT:
            long_paths.append(str(target))
        if dry_run:
            copied += 1
            continue
        try:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src / rel, target)
            copied += 1
        except OSError as e:
            failures.append(f"{rel} — {e}")
    return copied, failures, long_paths


def count_by_top(rels: list[Path]) -> dict[str, int]:
    """按顶级条目分组计数;根目录直属文件计入 "(根)"。"""
    counts: dict[str, int] = {}
    for rel in rels:
        top = rel.parts[0] if len(rel.parts) > 1 else "(根)"
        counts[top] = counts.get(top, 0) + 1
    return counts


# ---------------------------------------------------------------------------
# 改名(三处同步)
# ---------------------------------------------------------------------------

# 短于此长度的旧名(UI/APP/CAN/H743 这类)极易撞上无关标识符(如 Mcu.CPN=STM32H743XIH6、
# UI/ 路径),此时注释/文档区不做替换,只改锚定行,其余出现留给残留扫描、由人决定
SHORT_NAME_LIMIT = 5


def _sub_word(old: str, new: str, text: str) -> tuple[str, int]:
    """带词边界的替换:old 前后不能是 [A-Za-z0-9_],防止子串误伤(H743 撞 STM32H743XIH6)。"""
    pat = re.compile(rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])")
    return pat.subn(new, text)


def _read_text(path: Path) -> str:
    # newline="" 关闭行尾翻译,CRLF 原样保留(CubeMX 文件多为 CRLF)
    with open(path, "r", encoding="utf-8", newline="") as fh:
        return fh.read()


def _write_text(path: Path, text: str) -> None:
    with open(path, "w", encoding="utf-8", newline="") as fh:
        fh.write(text)


def rename_project(dst: Path, old_name: str, new_name: str, dry_run: bool,
                   src_for_dry: Path | None = None):
    """在目标内同步工程名(绝不碰源工程):

    1) <旧名>.ioc → <新名>.ioc:内容只改 ProjectManager.ProjectFileName / ProjectName
       两行(锚定行,永不误伤),其余出现留给残留扫描;
    2) 根 CMakeLists.txt:锚定替换 set(CMAKE_PROJECT_NAME ...) 行;注释等其余出现用
       词边界替换,旧名短于 SHORT_NAME_LIMIT 时跳过;
    3) 根 README*:词边界替换 + 短名跳过(文档,替换后由人复核语义)。

    任何读取失败(非 UTF-8 等)都不落写盘副作用。
    返回 (改名动作列表, 失败原因列表);动作 = (相对路径, 替换次数, 说明)。
    """
    actions: list[tuple[str, int, str]] = []
    failures: list[str] = []
    short_name = len(old_name) < SHORT_NAME_LIMIT

    # --- 1) .ioc 文件:先读(失败零副作用)→ 改名 → 写锚定两行 --------------
    ioc_old = dst / f"{old_name}.ioc"
    ioc_new = dst / f"{new_name}.ioc"
    if dry_run:
        ioc_src = (src_for_dry / f"{old_name}.ioc") if src_for_dry else ioc_old
        if ioc_src.is_file():
            actions.append((ioc_old.name, -1, f"将改名为 {ioc_new.name} 并替换锚定两行(dry-run)"))
        else:
            failures.append(f"未找到 {ioc_old.name},真跑时三处改名将失败(dry-run 预检)")
    elif not ioc_old.is_file():
        failures.append(f"未找到 {ioc_old.name},无法完成三处改名(源工程名判定为 {old_name})")
    else:
        try:
            text = _read_text(ioc_old)
        except (UnicodeDecodeError, OSError) as e:
            failures.append(f"{ioc_old.name}: 读取失败({e}),未做任何修改"
                            f"(若为编码问题请先转 UTF-8)")
            text = None
        if text is not None:
            # 锚定行替换:只动这两行的值,子串旧名(如 Mcu.CPN=STM32H743XIH6)绝不误伤
            text, n1 = re.subn(r"(?m)^(ProjectManager\.ProjectFileName=).*?(\r?)$",
                               lambda m: f"{m.group(1)}{new_name}.ioc{m.group(2)}", text)
            text, n2 = re.subn(r"(?m)^(ProjectManager\.ProjectName=).*?(\r?)$",
                               lambda m: f"{m.group(1)}{new_name}{m.group(2)}", text)
            if n1 == 1 and n2 == 1:
                os.replace(ioc_old, ioc_new)   # --force 重跑时新名.ioc 可能已存在,原子覆盖
                _write_text(ioc_new, text)
                actions.append((ioc_new.name, n1 + n2,
                                f"由 {ioc_old.name} 改名而来,锚定两行替换成功"))
            else:
                failures.append(f"{ioc_old.name}: ProjectFileName/ProjectName 行替换计数异常"
                                f"(n1={n1}、n2={n2},预期各 1),已中止且未改名未写盘")

    # --- 2) 根 CMakeLists.txt:锚定 set 行 + 词边界替换其余 -----------------
    cml = dst / "CMakeLists.txt"
    if dry_run:
        actions.append(("CMakeLists.txt", -1, "将锚定替换 CMAKE_PROJECT_NAME 行(dry-run)"))
    elif not cml.is_file():
        failures.append("未找到根 CMakeLists.txt,无法改 CMAKE_PROJECT_NAME")
    else:
        try:
            text = _read_text(cml)
        except (UnicodeDecodeError, OSError) as e:
            failures.append(f"CMakeLists.txt: 读取失败({e}),未做任何修改"
                            f"(若为编码问题请先转 UTF-8)")
            text = None
        if text is not None:
            # 锚定行:CMake 命令大小写不敏感,允许引号
            text, n_set = re.subn(
                rf"(?i)(set\(\s*CMAKE_PROJECT_NAME\s+[\"']?){re.escape(old_name)}([\"']?\s*\))",
                lambda m: f"{m.group(1)}{new_name}{m.group(2)}", text)
            n_rest = 0
            if not short_name:
                text, n_rest = _sub_word(old_name, new_name, text)
            if n_set == 1:
                _write_text(cml, text)
                note = f"锚定行 1 处 + 其余 {n_rest} 处"
                if short_name:
                    note += f"(旧名短于 {SHORT_NAME_LIMIT} 字符,注释区不替换,残留见扫描)"
                actions.append(("CMakeLists.txt", n_set + n_rest, note))
            else:
                failures.append(f"CMakeLists.txt: set(CMAKE_PROJECT_NAME {old_name}) 匹配"
                                f" {n_set} 处(预期 1),已中止未写盘")

    # --- 3) 根 README*:词边界替换 + 短名跳过 -------------------------------
    # dry-run 时目标尚不存在,从源侧枚举 README 以完整预告动作
    readme_root = src_for_dry if (dry_run and src_for_dry) else dst
    for readme in sorted(readme_root.glob("README*")):
        if not readme.is_file():
            continue
        if dry_run:
            actions.append((readme.name, -1, "将做词边界替换(dry-run)"))
            continue
        try:
            text = _read_text(readme)
        except (UnicodeDecodeError, OSError):
            actions.append((readme.name, 0, "读取失败(非 UTF-8?),跳过"))
            continue
        if short_name:
            actions.append((readme.name, 0,
                            f"旧名短于 {SHORT_NAME_LIMIT} 字符,不替换,残留见扫描"))
            continue
        text, n = _sub_word(old_name, new_name, text)
        if n:
            _write_text(readme, text)
        actions.append((readme.name, n, "词边界替换,语义请人工复核" if n else "无旧名"))

    return actions, failures


# ---------------------------------------------------------------------------
# 残留扫描
# ---------------------------------------------------------------------------

def scan_residue(dst: Path, names: list[str], keep_pats: list[str], skip: set[Path],
                 dir_pats: list[str], file_pats: list[str]):
    """在目标内扫描名字残留(逐行),按豁免路径分段。

    返回 (豁免命中, 非豁免命中, 文件名命中);
    行命中 = (相对路径, 行号, 名字, 行内容摘要);文件名命中 = (相对路径, 名字, 豁免?)。
    """
    exempt: list[tuple[str, int, str, str]] = []
    normal: list[tuple[str, int, str, str]] = []
    fname_hits: list[tuple[str, str, bool]] = []
    names = [n for n in names if n and n.strip()]   # 空串会让每一行都命中,必须过滤
    if not names:
        return exempt, normal, fname_hits

    # 与克隆同一套排除剪枝:--force 合并进带 build/ 的目标时,缓存不参与扫描
    files, *_ = apply_excludes(dst, dir_pats, file_pats)
    for rel_p in files:
        path = dst / rel_p
        if path.resolve() in skip:
            continue
        rel = rel_p.as_posix()
        is_exempt = rel_matches(rel, keep_pats)
        for nm in names:                       # 文件名本身含旧名也要报告
            if nm.lower() in path.name.lower():
                fname_hits.append((rel, nm, is_exempt))
        if path.suffix.lower() not in TEXT_SUFFIXES:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for lineno, line in enumerate(text.splitlines(), 1):
            for nm in names:
                # 行内容按精确大小写匹配(grep 语义);文件名匹配不区分(Windows 语义)
                if nm in line:
                    snippet = line.strip()[:80]
                    hit = (rel, lineno, nm, snippet)
                    (exempt if is_exempt else normal).append(hit)
    return exempt, normal, fname_hits


# ---------------------------------------------------------------------------
# 校验
# ---------------------------------------------------------------------------

def verify(dst: Path, copy_rels: list[Path], required: list[str],
           renames: dict[str, str], skip: set[Path], dry_run: bool,
           src: Path | None = None,
           dir_pats: list[str] | None = None, file_pats: list[str] | None = None):
    """动态对账 + 关键文件存在性检查。

    返回 (对账表, 缺失文件列表, 目标独有文件数, 关键文件结果, 失败原因列表)。
    对账表行 = (顶级目录, 源过滤后数, 目标实际数, 是否一致)。
    renames: 克隆后被改名的文件 {旧相对路径: 新相对路径}(.ioc),对账时按新名核对。
    """
    failures: list[str] = []

    # 每个"应复制"文件在目标的期望路径(考虑 .ioc 改名)
    expected: list[tuple[Path, Path]] = []          # (归组用的源相对路径, 目标期望相对路径)
    for rel in copy_rels:
        rel_s = str(rel)
        expected.append((rel, Path(renames.get(rel_s, rel_s))))

    if dry_run:
        # 预演:对账按"应复制清单"自证一致;关键文件在源侧预检,提前暴露缺失
        src_counts = count_by_top(copy_rels)
        table = [(top, n, n, True) for top, n in sorted(src_counts.items())]
        req_results = []
        for pat_s in required:
            found = src is not None and any(p.is_file() for p in src.glob(pat_s))
            req_results.append((pat_s, found))
            if not found:
                failures.append(f"缺关键文件(源侧预检):{pat_s}")
        return table, [], 0, req_results, failures

    missing = [str(er) for _, er in expected if not (dst / er).is_file()]
    ok_rels = [rel for rel, er in expected if (dst / er).is_file()]

    src_counts = count_by_top(copy_rels)
    dst_counts = count_by_top(ok_rels)
    table = []
    for top in sorted(src_counts):
        s, d = src_counts[top], dst_counts.get(top, 0)
        table.append((top, s, d, s == d))
        if s != d:
            failures.append(f"顶级目录 {top} 对账不符:源(过滤后){s} vs 目标 {d}")

    # 目标独有文件(--force 合并到已有目录时提示;与克隆同一套排除剪枝)
    expected_set = {str(er) for _, er in expected}
    dst_files, *_ = apply_excludes(dst, dir_pats or [], file_pats or [])
    extra = sum(1 for r in dst_files
                if (dst / r).resolve() not in skip and str(r) not in expected_set)

    # 关键文件存在性(glob 相对目标根)
    req_results: list[tuple[str, bool]] = []
    for pat_s in required:
        found = any(p.is_file() for p in dst.glob(pat_s))
        req_results.append((pat_s, found))
        if not found:
            failures.append(f"缺关键文件:{pat_s}")

    return table, missing, extra, req_results, failures


# ---------------------------------------------------------------------------
# 报告
# ---------------------------------------------------------------------------

def build_report(ctx: dict) -> str:
    """汇总所有结果,生成中文 markdown 报告文本。ctx 见 main() 里的组装。"""
    L: list[str] = []
    L.append(f"# 克隆报告 —— {ctx['title']}")
    L.append("")
    L.append(f"- 生成时间:{ctx['now']}")
    L.append(f"- 工具:projclone.py v{VERSION}(profile = {ctx['profile']})"
             f"{' 【dry-run,未落盘】' if ctx['dry_run'] else ''}")
    L.append(f"- 源:`{ctx['src']}`")
    L.append(f"- 目标:`{ctx['dst']}`")
    if ctx["new_name"]:
        L.append(f"- 改名:`{ctx['old_name']}` → `{ctx['new_name']}`")
    L.append("")

    L.append("## 1. 复制与排除")
    L.append("")
    L.append(f"- 应复制文件:{len(ctx['copy_rels'])} 个,实际复制:{ctx['copied']} 个")
    L.append(f"- 排除目录:{len(ctx['excluded_dirs'])} 个;排除文件:{len(ctx['excluded_files'])} 个")
    for d in ctx["excluded_dirs"][:20]:
        L.append(f"  - [目录] {d}")
    if len(ctx["excluded_dirs"]) > 20:
        L.append(f"  - ……共 {len(ctx['excluded_dirs'])} 个目录,余略")
    for f in ctx["excluded_files"][:20]:
        L.append(f"  - [文件] {f}")
    if len(ctx["excluded_files"]) > 20:
        L.append(f"  - ……共 {len(ctx['excluded_files'])} 个文件,余略")
    if ctx["link_skips"]:
        L.append(f"- 跳过链接(junction/symlink,不复制):{len(ctx['link_skips'])} 个")
        for p in ctx["link_skips"][:10]:
            L.append(f"  - {p}")
    if ctx["copy_failures"]:
        L.append(f"- **复制失败 {len(ctx['copy_failures'])} 个:**")
        for f in ctx["copy_failures"]:
            L.append(f"  - {f}")
    if ctx["long_paths"]:
        L.append(f"- 超长路径警告(>{PATH_WARN_LIMIT} 字符,共 {len(ctx['long_paths'])} 个),"
                 f"如失败请缩短目标路径或启用系统长路径支持:")
        for p in ctx["long_paths"][:10]:
            L.append(f"  - {p}")
    L.append("")

    L.append("## 2. 顶级目录对账(源过滤后 vs 目标)")
    L.append("")
    L.append("| 顶级条目 | 源(过滤后) | 目标 | 结果 |")
    L.append("|---|---|---|---|")
    for top, s, d, ok in ctx["table"]:
        L.append(f"| {top} | {s} | {d} | {'一致' if ok else '**不符**'} |")
    if ctx["missing"]:
        L.append("")
        L.append(f"**目标缺失 {len(ctx['missing'])} 个应复制文件:**")
        for m in ctx["missing"][:20]:
            L.append(f"- {m}")
    if ctx["extra"]:
        L.append("")
        L.append(f"- 目标中另有 {ctx['extra']} 个非本次复制的文件(--force 合并或报告自身)")
    L.append("")

    if ctx["new_name"]:
        L.append("## 3. 改名详情(三处同步 + 工程身份文件)")
        L.append("")
        L.append(f"- 目录名:由 --dst 末段决定 = `{ctx['dst'].name}`")
        for name, n, note in ctx["rename_actions"]:
            times = "?" if n < 0 else str(n)
            L.append(f"- `{name}`:替换 {times} 处 —— {note}")
        L.append("")

    L.append("## 4. 关键文件清单")
    L.append("")
    if ctx["req_results"]:
        for pat_s, found in ctx["req_results"]:
            L.append(f"- [{'x' if found else ' '}] `{pat_s}`"
                     f"{'' if found else ' **缺失**'}")
    else:
        L.append("- (raw profile 无强制清单)")
    L.append("")

    L.append("## 5. 残留扫描")
    L.append("")
    if not ctx["scan_names"]:
        L.append("- 未指定 --new-name / --scan-name,跳过残留扫描。")
    else:
        L.append(f"- 扫描名字:{'、'.join(ctx['scan_names'])};"
                 f"豁免模式:{'、'.join(ctx['keep_pats'])}")
        L.append("")
        L.append(f"### 5.1 豁免区残留({len(ctx['exempt'])} 处)——设计决策,不算失败")
        L.append("")
        if ctx["exempt"]:
            L.append("改名需走 LVGL Editor 重新 Generate 等专用流程,文本替换会破坏一致性:")
            for rel, lineno, nm, snip in ctx["exempt"]:
                L.append(f"- `{rel}:{lineno}` [{nm}] {snip}")
        else:
            L.append("- 无")
        L.append("")
        L.append(f"### 5.2 非豁免残留({len(ctx['normal'])} 处)——请逐条人工确认")
        L.append("")
        if ctx["normal"]:
            for rel, lineno, nm, snip in ctx["normal"]:
                L.append(f"- `{rel}:{lineno}` [{nm}] {snip}")
        else:
            L.append("- 无")
        if ctx["fname_hits"]:
            L.append("")
            L.append("### 5.3 文件名含扫描名")
            L.append("")
            for rel, nm, is_ex in ctx["fname_hits"]:
                L.append(f"- `{rel}` [{nm}]{'(豁免区)' if is_ex else ''}")
    L.append("")

    L.append("## 6. 结论")
    L.append("")
    code = ctx["exit_code"]
    verdict = {0: "0 —— 校验全过,零残留",
               1: "1 —— 有警告:存在残留(明细见第 5 节)",
               2: "2 —— 失败"}[code]
    L.append(f"- exit code = {verdict}")
    for r in ctx["fail_reasons"]:
        L.append(f"  - [失败] {r}")
    L.append("")
    return "\n".join(L)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def build_argparser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(
        prog="projclone.py",
        description="STM32 工程克隆工具:复制(应用排除规则)+ 三处改名 + 校验报告。",
        epilog="详见 F:\\STM32\\Templates\\工程克隆工具ProjClone_策划案.md")
    ap.add_argument("--src", required=True, help="源工程目录(绝不会被修改)")
    ap.add_argument("--dst", required=True, help="目标目录(末段即新目录名)")
    ap.add_argument("--new-name", help="新工程名;同步 .ioc / CMakeLists / 目录名三处")
    ap.add_argument("--profile", choices=sorted(PROFILES), default="cubemx-cmake",
                    help="预设(默认 cubemx-cmake)")
    ap.add_argument("--exclude", action="append", default=[], metavar="PAT",
                    help="追加排除模式(fnmatch;尾部 / 表示目录),可多次")
    ap.add_argument("--keep-name-in", action="append", default=[], metavar="PAT",
                    help=f"追加残留豁免路径模式(默认已含 {DEFAULT_KEEP_NAME_IN[0]}),可多次")
    ap.add_argument("--scan-name", action="append", default=[], metavar="NAME",
                    help="追加残留扫描的名字(默认只扫旧工程名),可多次")
    ap.add_argument("--keep-git", action="store_true", help="保留 .git 目录")
    ap.add_argument("--dry-run", action="store_true",
                    help="只打印将执行的动作清单,不落盘")
    ap.add_argument("--force", action="store_true",
                    help="目标已存在时允许合并覆盖(默认拒绝)")
    ap.add_argument("--report", metavar="FILE",
                    help="报告输出路径(默认 <dst>\\克隆报告.md)")
    return ap


def fail(msg: str) -> "NoReturn":
    print(f"[失败] {msg}", file=sys.stderr)
    sys.exit(2)


def _run(argv: list[str] | None = None) -> int:
    # 防 GBK 控制台打印中文/特殊字符时崩溃
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass

    args = build_argparser().parse_args(argv)
    src = Path(args.src).resolve()
    dst = Path(args.dst).resolve()
    profile = PROFILES[args.profile]

    # ---- 护栏(F6) -------------------------------------------------------
    if not src.is_dir():
        fail(f"源工程不存在或不是目录:{src}")
    if src == dst:
        fail("源与目标相同")
    if dst.is_relative_to(src) or src.is_relative_to(dst):
        fail(f"源与目标互相嵌套,拒绝(防递归自我复制):\n  源 {src}\n  目标 {dst}")
    if " " in str(dst):
        print(f"[警告] 目标路径含空格,CMake/脚本生态易踩坑:{dst}")
    if len(str(dst)) >= 200:
        print(f"[警告] 目标路径较长({len(str(dst))} 字符),深层文件可能逼近 Windows 260 限制")
    # 目标安全护栏:系统敏感路径拒绝;F:\STM32 工作区外仅警告(保留跨机通用性)
    _dst_posix = str(dst).replace("\\", "/").lower()
    if _dst_posix.startswith(("c:/windows", "c:/program files", "c:/programdata")):
        fail(f"目标位于系统敏感路径,拒绝:{dst}")
    if not _dst_posix.startswith("f:/stm32"):
        print(f"[警告] 目标不在 F:\\STM32 工作区内(本工作区约定克隆目标应在其下):{dst}")
    _WIN_RESERVED = ({"CON", "PRN", "AUX", "NUL"}
                     | {f"COM{i}" for i in range(1, 10)}
                     | {f"LPT{i}" for i in range(1, 10)})
    if args.new_name:
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_-]*", args.new_name):
            fail(f"新工程名含非法字符(建议字母/数字/下划线/连字符):{args.new_name}")
        if args.new_name.upper() in _WIN_RESERVED:
            fail(f"新工程名是 Windows 保留设备名({args.new_name}),生成 {args.new_name}.ioc 会失败")
        if dst.name != args.new_name:
            print(f"[警告] 目标目录名({dst.name})与 --new-name({args.new_name})不一致,"
                  f"目录名以 --dst 为准")

    # 目标已存在检查(拒绝时不落任何盘;dry-run 也要预告真跑会被拒,保证预演一致性)
    preexist_warn = False
    if dst.exists() and not dst.is_dir():
        fail(f"目标路径已存在且是一个文件(应为目录):{dst}")
    try:
        dst_nonempty = dst.is_dir() and any(dst.iterdir())
    except OSError as e:
        fail(f"无法访问目标目录:{dst} — {e}")
    if dst_nonempty and not args.force:
        if args.dry_run:
            print(f"[警告] 目标已存在且非空,真跑将被拒绝(需 --force):{dst}")
            preexist_warn = True
        else:
            fail(f"目标已存在且非空:{dst}\n如确要合并覆盖,请加 --force(目标独有文件会保留)")

    # 报告路径护栏必须在克隆之前:指向源内直接拒绝,不浪费一次克隆
    report_path = (Path(args.report).resolve() if args.report
                   else (dst / "克隆报告.md").resolve())
    if report_path.is_relative_to(src):
        fail(f"--report 不允许指向源工程内部(绝不改源):{report_path}")

    # ---- 确定旧工程名:源根唯一 .ioc 的主名优先,否则源目录名 -------------
    iocs = sorted(src.glob("*.ioc"))
    if len(iocs) == 1:
        old_name = iocs[0].stem
        if old_name != src.name:
            print(f"[警告] 源目录名({src.name})与 .ioc 主名({old_name})不一致,"
                  f"以 .ioc 为准判定旧工程名")
    else:
        old_name = src.name
        if args.new_name and args.profile != "raw":
            print(f"[警告] 源根目录 .ioc 数量为 {len(iocs)}(预期 1),旧名按目录名 {old_name} 判定")

    if args.new_name == old_name:
        print(f"[提示] 新旧工程名相同({old_name}),跳过改名步骤")
        args.new_name = None

    # ---- 组装排除与豁免 ---------------------------------------------------
    dir_pats, file_pats = normalize_excludes(
        list(profile["excludes"]) + args.exclude, keep_git=args.keep_git)
    keep_pats = DEFAULT_KEEP_NAME_IN + args.keep_name_in

    mode = "dry-run(不落盘)" if args.dry_run else "执行"
    print(f"projclone v{VERSION} [{mode}] profile={args.profile}")
    print(f"  源:{src}")
    print(f"  目标:{dst}")
    print(f"  排除:目录模式 {dir_pats} + 文件模式 {file_pats}")

    # ---- 阶段 1:应用排除规则,得到待复制清单 -----------------------------
    copy_rels, excluded_dirs, excluded_files, link_skips, walk_errors = apply_excludes(
        src, dir_pats, file_pats)
    print(f"[1/4] 扫描源:{len(copy_rels)} 个文件待复制,"
          f"排除 {len(excluded_dirs)} 个目录、{len(excluded_files)} 个文件")
    for ls in link_skips:
        print(f"[警告] 跳过链接(junction/symlink 不复制):{ls}")
    for we in walk_errors:
        print(f"[失败] 源遍历错误(克隆将不完整):{we}")
    if args.dry_run:
        for d in excluded_dirs:
            print(f"      [排除目录] {d}")
        for f in excluded_files:
            print(f"      [排除文件] {f}")

    # ---- 阶段 2:复制 ------------------------------------------------------
    copied, copy_failures, long_paths = clone(src, dst, copy_rels, args.dry_run)
    print(f"[2/4] {'将复制' if args.dry_run else '已复制'} {copied} 个文件"
          + (f"(失败 {len(copy_failures)})" if copy_failures else ""))
    for lp in long_paths[:5]:
        print(f"[警告] 目标路径超长:{lp}")

    # ---- 阶段 3:改名 ------------------------------------------------------
    rename_actions: list[tuple[str, int, str]] = []
    rename_failures: list[str] = []
    renames: dict[str, str] = {}
    if args.new_name:
        rename_actions, rename_failures = rename_project(
            dst, old_name, args.new_name, args.dry_run, src_for_dry=src)
        renames[f"{old_name}.ioc"] = f"{args.new_name}.ioc"
        print(f"[3/4] 改名 {old_name} → {args.new_name}:")
        for name, n, note in rename_actions:
            times = "?" if n < 0 else str(n)
            print(f"      {name}: 替换 {times} 处({note})")
        for r in rename_failures:
            print(f"[失败] {r}")
    else:
        print("[3/4] 未指定 --new-name,跳过改名")

    # ---- 阶段 4:校验(对账 + 关键文件 + 残留扫描) -----------------------
    # 默认报告名无条件跳过:--force 重跑时目标里可能残存上次报告(满是旧名),不当残留
    skip = {report_path, (dst / "克隆报告.md").resolve()}

    table, missing, extra, req_results, verify_failures = verify(
        dst, copy_rels, profile["required"], renames, skip, args.dry_run, src=src,
        dir_pats=dir_pats, file_pats=file_pats)

    scan_names = ([old_name] if args.new_name else []) + args.scan_name
    scan_names = list(dict.fromkeys(scan_names))            # 去重保序
    if args.dry_run:
        exempt, normal, fname_hits = [], [], []
        if scan_names:
            print(f"[4/4] dry-run:将扫描名字 {scan_names},豁免 {keep_pats}")
        else:
            print("[4/4] dry-run:无扫描名字,将只做对账与关键文件检查")
    else:
        exempt, normal, fname_hits = scan_residue(dst, scan_names, keep_pats, skip,
                                                  dir_pats, file_pats)
        print(f"[4/4] 校验:对账{'通过' if not any(not ok for *_, ok in table) else '不符'};"
              f"残留 豁免 {len(exempt)} 处 / 非豁免 {len(normal)} 处")

    # ---- 汇总 exit code ----------------------------------------------------
    fail_reasons = [f"复制失败 {len(copy_failures)} 个文件"] if copy_failures else []
    fail_reasons += [f"源遍历错误:{we}" for we in walk_errors]
    fail_reasons += rename_failures + verify_failures
    if missing:
        fail_reasons.append(f"目标缺失 {len(missing)} 个应复制文件")
    if preexist_warn:
        fail_reasons.append("目标已存在且非空(真跑将被拒绝,需 --force;本次为 dry-run 预告)")
    if fail_reasons:
        exit_code = 2
    elif exempt or normal or fname_hits:    # 文件名含旧名同样算残留,守住"0 = 零残留"
        exit_code = 1
    else:
        exit_code = 0

    # ---- 报告 --------------------------------------------------------------
    ctx = dict(
        title=args.new_name or dst.name, now=datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        profile=args.profile, dry_run=args.dry_run, src=src, dst=dst,
        old_name=old_name, new_name=args.new_name,
        copy_rels=copy_rels, copied=copied, excluded_dirs=excluded_dirs,
        excluded_files=excluded_files, link_skips=link_skips,
        copy_failures=copy_failures,
        long_paths=long_paths, table=table, missing=missing, extra=extra,
        rename_actions=rename_actions, req_results=req_results,
        scan_names=scan_names, keep_pats=keep_pats,
        exempt=exempt, normal=normal, fname_hits=fname_hits,
        fail_reasons=fail_reasons, exit_code=exit_code,
    )
    report_text = build_report(ctx)
    if args.dry_run:
        print("\n" + "=" * 30 + " dry-run 报告(未写盘) " + "=" * 30)
        print(report_text)
    else:
        try:
            report_path.parent.mkdir(parents=True, exist_ok=True)
            report_path.write_text(report_text, encoding="utf-8")
            print(f"报告已写入:{report_path}")
        except OSError as e:
            print(f"[警告] 报告写入失败({e}),以下为报告全文:")
            print(report_text)

    print(f"完成,exit code = {exit_code}")
    return exit_code


def main(argv: list[str] | None = None) -> int:
    """顶层兜底:未预期异常一律归为 exit 2(失败)。

    Python 未捕获 traceback 的进程退出码恰好是 1,与"exit 1 = 有残留(警告)"
    撞车,会让上层(skill/自动化)把崩溃误读成最温和的档位——必须显式归 2。
    (fail() 的 SystemExit 不受影响,原样传播。)
    """
    try:
        return _run(argv)
    except Exception as e:
        import traceback
        print(f"[失败] 未预期异常:{e}", file=sys.stderr)
        traceback.print_exc()
        return 2


if __name__ == "__main__":
    sys.exit(main())
