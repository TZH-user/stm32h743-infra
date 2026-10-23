# -*- coding: utf-8 -*-
"""translate_ui.py —— 仪器模板 UI 字符串翻译/重刷工具(配合 translation_map.md)

用途:上游中文版(signal_separation / H743_Instrument_CN)更新后,对英文工程
重刷界面翻译;或校验英文工程是否仍有引号内中文残留。

映射来源 = 目标工程(或 --map 指定)的 translation_map.md:
  主表(2 列,首列含中文)   -> 中->英 映射
  修订附录(3 列,首列英文) -> 英->英 短化映射(槽宽适配),在主映射之后应用
改译文/加新串:直接编辑 translation_map.md 后重跑本工具,勿手改 gen 文件。

替换规则(与首次翻译一致):
  * 只处理引号内文本("...";XML 属性值天然同形),代码注释/串口日志不动
  * 映射按长度降序应用,防止短串截胡长串
  * 范围:UI/Designer/**.xml、UI/Designer/**.c、UI/Adapter、UI/Custom、UI/pages
  * 完成后强校验:范围内引号中文必须清零,否则 exit 1 并列出(补映射后重跑)

用法(PowerShell):
  python translate_ui.py --project F:\\STM32\\Templates\\H743_Instrument_EN
  python translate_ui.py --project <工程> --check-only
  python translate_ui.py --project <工程> --map <别处的 translation_map.md>
"""
import argparse
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(line_buffering=True)

GLOBS = ["UI/Designer/**/*.xml", "UI/Designer/**/*.c",
         "UI/Adapter/*.c", "UI/Custom/*.c", "UI/pages/*.c"]
HAN = re.compile(r"[\u4e00-\u9fff]")
QUOTED = re.compile(r'"([^"\n]*)"')

def parse_map(md_path):
    """从 translation_map.md 解析映射:返回 (zh_en 列表, en_en 列表)。"""
    zh_en, en_en = [], []
    for line in md_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not (line.startswith("|") and line.endswith("|")):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if any(set(c) <= {"-", " ", ":"} and c for c in cells):
            continue                       # 分隔行
        if len(cells) == 2 and HAN.search(cells[0]):
            zh_en.append((cells[0], cells[1]))
        elif len(cells) == 3 and cells[0] and not HAN.search(cells[0]) \
                and cells[0] not in ("原英文", "English"):
            # 修订附录可能一格多条(用 / 并列),逐条拆开
            olds = [s.strip() for s in cells[0].split(" / ")]
            news = [s.strip() for s in cells[1].split(" / ")]
            if len(olds) == len(news):
                en_en += list(zip(olds, news))
    zh_en.sort(key=lambda kv: len(kv[0]), reverse=True)
    en_en.sort(key=lambda kv: len(kv[0]), reverse=True)
    return zh_en, en_en

def main():
    ap = argparse.ArgumentParser(description="仪器模板 UI 翻译重刷/校验")
    ap.add_argument("--project", required=True, help="目标工程根目录(英文版)")
    ap.add_argument("--map", help="translation_map.md 路径(默认取工程根下同名文件)")
    ap.add_argument("--check-only", action="store_true", help="只校验残留,不改文件")
    a = ap.parse_args()

    proj = Path(a.project)
    md = Path(a.map) if a.map else proj / "translation_map.md"
    if not proj.is_dir():
        sys.exit(f"工程不存在: {proj}")
    files = sorted({p for g in GLOBS for p in proj.glob(g) if p.is_file()})

    if not a.check_only:
        if not md.is_file():
            sys.exit(f"映射文件不存在: {md}")
        zh_en, en_en = parse_map(md)
        print(f"[映射] 中->英 {len(zh_en)} 条,英->英修订 {len(en_en)} 条(来自 {md.name})")
        hits = 0

        def sub(m):
            nonlocal hits
            inner = m.group(1)
            if HAN.search(inner):
                for zh, en in zh_en:
                    if zh in inner:
                        hits += inner.count(zh)
                        inner = inner.replace(zh, en)
            for old, new in en_en:               # 整串短化,防误伤长文本
                if inner == old:
                    hits += 1
                    inner = new
            return f'"{inner}"'

        for f in files:
            with open(f, "r", encoding="utf-8", newline="") as fh:
                text = fh.read()
            new = QUOTED.sub(sub, text)
            if new != text:
                with open(f, "w", encoding="utf-8", newline="") as fh:
                    fh.write(new)
        print(f"[翻译] 替换 {hits} 处,共 {len(files)} 个文件")

    residue = []
    for f in files:
        for i, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            for m in QUOTED.finditer(line):
                if HAN.search(m.group(1)):
                    residue.append(f"{f.relative_to(proj)}:{i}: {m.group(0)[:60]}")
    if residue:
        print(f"[残留] 引号内中文 {len(residue)} 处:")
        for r in residue[:20]:
            print("   ", r)
        sys.exit(1)
    print("[校验] 引号内中文清零,通过")

if __name__ == "__main__":
    main()
