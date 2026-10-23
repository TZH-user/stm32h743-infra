# ProjClone —— STM32 工程克隆工具 策划案

- 日期:2026-07-26
- 起草:Claude Code(基于当日两次成功实践:signal_separation → H743_LVGL_Baseline、Baseline → H743_UI_Instrument)
- 交付方式:本文件交给实施 AI(Codex / Claude 等)独立完成,**无需本次会话上下文**
- 状态:待实施

---

## 1. 背景与动机

F:\STM32\stm32project\VSCodeCMAKEProject 下十余个 CubeMX+CMake 工程的演进方式是"整目录复制旧工程再改",长期痛点:

1. 复制时把 build 缓存、备份目录(备份*、Backups_UTF-8、*_ai_backup_*)、.bak 文件、11 万级第三方库文件一起带走;
2. 工程名散布**三处**——目录名、CMakeLists 的 `set(CMAKE_PROJECT_NAME x)`、.ioc 文件名及其内部 `ProjectManager.ProjectName/ProjectFileName` 两行——手工漏改任何一处,构建产物或 CubeMX 重生成就错位;
3. 无校验环节,克隆完是否完整全凭肉眼。

2026-07-26 已用一次性 Python 脚本(shutil 字节级复制 + 排除规则 + 文本替换 + 清单校验)完成两次模板制作,全部一次通过编译。本策划案把该模式固化为通用工具。

## 2. 产品形态(两阶段)

- **阶段 A(必做)**:单文件 Python CLI —— `projclone.py`,仅用标准库(pathlib/shutil/re/argparse/fnmatch),放 `F:\STM32\Templates\_tools\projclone.py`。
- **阶段 B(可选)**:MCP 服务器化 —— 用户主力工作流是 AI + MCP(参照 F:\MCP 下 ai-everything-mcp-* 的 Python MCP 结构),暴露 `clone_project / list_profiles / verify_clone` 三个工具,让任意 AI 会话一句话完成克隆。PyInstaller 打 exe 优先级低于 MCP 化。

## 3. 功能规格

### F1 克隆(核心)
`--src <源工程> --dst <目标路径>`:递归字节级复制(shutil.copy2 保时间戳),应用排除规则。**绝不修改源工程**。目标已存在默认拒绝,`--force` 才允许合并覆盖。

### F2 改名(三处同步)
`--new-name <新名>` 时:
1. `<旧名>.ioc` → `<新名>.ioc`,文件内全文 replace 旧名(实践验证安全:.ioc 内旧名只出现在 ProjectName/ProjectFileName 两行);
2. CMakeLists.txt 中 `set(CMAKE_PROJECT_NAME 旧名)` → 新名;
3. 目标目录名即 `--dst` 末段,由调用方给出。
其余文件**默认不做全文替换**,只扫描并报告旧名残留位置(见 F3),由人决定是否处理——因为存在**豁免场景**:LVGL Editor 生成库(如 signal_separation_ui_*)改名必须走 Editor 重新 Generate,文本替换会破坏工具工程一致性。默认豁免模式 `--keep-name-in "UI/Designer/**"`,可追加。

### F3 校验与报告
克隆完成后自动执行,输出到终端与 `--report <file>`(默认 dst 下 `克隆报告.md`):
1. 文件数对账:源(应用排除规则后)与目标逐顶级目录计数一致;
2. 关键文件存在性:profile 定义的清单(如 .ioc、CMakeLists.txt、CMakePresets.json、*.ld、startup_*.s、cmake/stm32cubemx/CMakeLists.txt);
3. 旧名残留扫描:目标内所有**文本文件**(白名单后缀:.c/.h/.txt/.md/.cmake/.ioc/.json/.ld/.s/.xml/.yml)grep 旧名,豁免路径单列,非豁免残留列出文件:行号;
4. exit code:0 全过 / 1 有警告(残留在豁免区)/ 2 失败(缺关键文件或计数不符)。

### F4 预设 profile(--profile,默认 cubemx-cmake)
| profile | 用途 | 排除规则 | 关键文件清单 |
|---|---|---|---|
| cubemx-cmake | CubeMX+CMake 工程克隆(默认) | 见 §4 默认集 | .ioc、CMakeLists.txt、CMakePresets.json、启动文件、链接脚本、cmake/ |
| template-derive | 从 Templates 派生新项目 | 默认集(build 必除) | 同上 + README* |
| raw | 纯净复制,只排缓存 | build/、.cache/、.codegraph/ | 无强制清单 |

### F5 dry-run
`--dry-run`:完整打印将复制/排除/改名/校验的动作清单,不落盘。

### F6 交互护栏
- 目标路径含空格 → 警告(CMake/脚本生态易踩坑,用户工程史上"Simple wireless automatic transceiver device"目录反复添乱);
- 路径长度接近 260 → 警告(Windows 长路径);
- 源在目标内部或反之 → 拒绝(防递归自我复制)。

## 4. 排除规则默认集(实践总结,fnmatch 语法)

```
build/            */build/          .cache/           .codegraph/
Backups_UTF-8/    备份*/            *_backup_*/       _ai_backup_*/
_codex_flash_build_*/               *.bak*            mx.scratch
preview-build/    preview-bin/      .git/(--keep-git 可保留)
```
规则按目录名与文件名两级匹配;CLI `--exclude PATTERN` 可追加。

## 5. 实现要点与经验教训(必读,均为当日实战踩过的点)

1. **文本替换必须保留行尾**:CubeMX 文件多为 CRLF。Python 打开用 `open(path, encoding="utf-8", newline="")` 读写,正则里行尾写 `\r?\n`,否则整文件行尾被暗改。
2. **替换必须计数校验**:用 `re.subn`,预期替换次数不符即中止(当日 freertos.c 清理用 `n1!=1 or n2!=1 → sys.exit` 模式,避免静默失败)。
3. **只对文本白名单后缀做替换**,其余一律 shutil 字节复制——防止对 .png/.a/.wasm 等二进制做字符串操作。
4. **根层排除闭包技巧**:`shutil.copytree` 的 ignore 回调里判断 `Path(d) == 目标根` 再排除,可实现"只排顶层的 demos/,不误伤深层同名目录"(LVGL 裁剪的做法)。
5. **Windows 大小写陷阱**:同一工程内出现过 `Core/App` 与 `Core/APP` 并存(文件系统不敏感、被跨平台工具视为不同)。克隆器不修此问题,但残留扫描时按实际磁盘名报告。
6. **编码**:工程内中文注释均为 UTF-8;子进程输出捕获(若有)需 `encoding="utf-8", errors="replace"`,防 GBK 控制台乱码中断。
7. **校验优先动态对账**("目标数 == 源过滤后数"),硬编码预期数只用于关键文件清单——当日曾因源目录里多出 README.md/转义名文件与硬编码数字不符而误报。

## 6. CLI 设计与示例

```
python projclone.py --src SRC --dst DST [--new-name NAME]
                    [--profile cubemx-cmake|template-derive|raw]
                    [--exclude PAT]... [--keep-name-in PAT]...
                    [--keep-git] [--dry-run] [--force] [--report FILE]
```

```powershell
# 例1:从模板起新项目(最常用)
python projclone.py --src F:\STM32\Templates\H743_UI_Instrument `
    --dst F:\STM32\stm32project\MyExperiment --new-name MyExperiment `
    --profile template-derive

# 例2:旧工程做纯净归档副本(不带缓存/备份)
python projclone.py --src ...\signal_separation --dst F:\STM32\_archive\signal_separation_clean --profile raw

# 例3:先看会发生什么
python projclone.py --src ... --dst ... --new-name X --dry-run
```

## 7. 给实施 AI 的指令

**你(实施 AI)要做的事**:
1. 在 `F:\STM32\Templates\_tools\` 创建 `projclone.py`,按 §3-§6 实现;单文件、纯标准库、Python 3.10+;代码结构:`PROFILES` 常量区 → `apply_excludes()` → `clone()` → `rename_project()` → `scan_residue()` → `verify()` → `main()`。
2. 中文注释与中文报告输出;所有路径 pathlib,不拼字符串。
3. **验收测试(必须实际执行并贴输出)**:
   - T1 `--dry-run` 克隆 `F:\STM32\Templates\H743_LVGL_Baseline` → 不产生任何文件;
   - T2 真克隆 Baseline 到临时目录、`--new-name CloneSmokeTest`:三处改名生效、零非豁免残留、报告生成、exit 0;随后 `cmake --preset Debug` 在克隆出的工程上 **configure 成功**;
   - T3 克隆 `H743_UI_Instrument` 同上:预期 exit 1(signal_separation_ui 残留在豁免区 UI/Designer/**,属设计决策,报告应将其列入豁免段而非失败);configure 同样必须成功;
   - T4 目标已存在且无 `--force` → 拒绝且不动目标。
   - 测试产物放临时目录,测试后清理。
4. 完成后在本文件末尾追加"实施记录"段:完成日期、脚本路径、四项测试结果。
5. **不要做的事**:不改动 Templates 下两个模板与 VSCodeCMAKEProject 任何源工程;不引第三方依赖;不做 GUI;阶段 B(MCP 化)未经用户确认不启动。

**环境注意**:若在 Claude Code 旧会话中遇到权限分类器报 `deepseek-v4-pro[1m] is temporarily unavailable`,属已知路由故障残留——只读工具不受影响,写盘/执行可请用户在终端跑命令(用户 shell 为 PowerShell 7)。新会话无此问题。

## 8. 阶段 B 提要(MCP 化,待用户确认后另行实施)

- server 名 `proj-clone`,Python MCP(参照 F:\MCP\ai-everything-mcp-* 结构);
- tools:`clone_project(src, dst, new_name?, profile?, dry_run?)`、`list_profiles()`、`verify_clone(dst, old_name)`,返回结构化报告;
- 注册进用户 MCP 配置后,任意 AI 会话可一句话克隆工程;
- 风险控制:dst 限白名单根(F:\STM32\**),拒绝系统路径;默认 dry_run=true。

## 9. 里程碑

| 里程碑 | 内容 | 预估 |
|---|---|---|
| M1 | projclone.py + 四项验收测试通过 | 半天 |
| M2 | 排除/豁免规则按使用反馈微调,补 raw/template-derive 细节 | 半天 |
| M3(可选) | MCP 服务器化 + 注册 | 1 天 |

---

## 实施记录

- **完成日期**:2026-07-26
- **实施者**:Claude Code(Fable 5,新会话独立按本策划案实施)
- **脚本路径**:`F:\STM32\Templates\_tools\projclone.py`(单文件 ~640 行,纯标准库,Python 3.10+,已在 Python 3.12 验证)
- **状态**:阶段 A 完成,M1 达成。阶段 B(MCP 化)未启动(待用户确认)。

### 四项验收测试结果(均实际执行)

| 测试 | 命令要点 | 结果 |
|---|---|---|
| T1 | `--dry-run` 克隆 H743_LVGL_Baseline | **通过**:完整打印动作清单(2851 文件、排除 build、三处改名预告、源侧关键文件预检),目标目录零落盘,exit 0 |
| T2 | 真克隆 Baseline → CloneSmokeTest(template-derive) | **通过**:exit 0;三处改名生效(`CloneSmokeTest.ioc` 787/788 行、`set(CMAKE_PROJECT_NAME CloneSmokeTest)`);.ioc 替换 2 处、CMakeLists 2 处、README 5 处,强校验全过;CRLF 保留;零残留;`克隆报告.md` 生成;`cmake --preset Debug` configure 成功;源工程逐项验证未被改动 |
| T3 | 克隆 H743_UI_Instrument + `--scan-name signal_separation` | **通过**:exit 1;豁免段(UI/Designer/**)列 45 处 signal_separation 残留并注明"设计决策,不算失败";非豁免段如实列 11 处(app_lvgl_ui.c、README、CMakeLists 注释等,属已知设计决策,见 README 改名说明);configure 成功 |
| T4 | 目标已存在且无 `--force` | **通过**:exit 2,拒绝信息清晰,目标 2877 文件数与时间戳逐项比对未变 |

测试产物置于 `F:\STM32\_projclone_test\`(短路径,避开会话 scratchpad 前缀 129 字符 + 最深相对路径 106 字符 = 251 逼近 260 上限的风险),测毕已整目录删除。

### 实施裁量(对策划案模糊点的解释,已按此实现)

1. **替换白名单 = 工程身份文件**:根 `.ioc` + 根 `CMakeLists.txt` + 根 `README*` 三类做全文替换,其余文件一律只扫描不修改。原因:实测 Baseline 旧名还出现在 CMakeLists 第 4 行注释与 README(5 处),仅改"三处"无法达成 T2 要求的"零非豁免残留 + exit 0"。强校验点(.ioc 两行、`set(CMAKE_PROJECT_NAME)`)不符即 exit 2;注释/README 替换仅记录次数。副作用:README 内旧绝对路径会被替换为新名路径,报告中提示人工复核语义。
2. **新增 `--scan-name NAME`(可多次)**:残留扫描默认只扫旧工程名;T3 中 signal_separation 是祖先库名而非本次旧名(扫旧名 H743_UI_Instrument 为零命中),故通过 `--scan-name signal_separation` 显式纳入扫描以验证豁免机制。
3. **exit code 细化**:0 = 零残留且校验全过;1 = 有残留(豁免与非豁免均为警告级,分段列出);2 = 失败(缺关键文件、对账不符、替换强校验失败、拒绝类:目标已存在无 `--force`、src/dst 嵌套、新名非法字符)。
4. 附加实现:残留报告含"文件名含扫描名"小节(如 `signal_separation_ui.c` 等 4 个文件名命中);对账采用"逐顶级目录、源过滤后 vs 目标实际存在"动态核对,并考虑 .ioc 改名映射;报告文件自身不计入对账与扫描。

### 阶段 B 处置(2026-07-26,用户决定)

**不做 MCP 服务器化,改用 Claude Code skill 约束 AI 会话。** §8 的风险控制思路(dst 白名单 F:\STM32\**、默认 dry-run)已移植进 skill 硬性约束:

- 生效副本:`C:\Users/<user>\.claude\skills\proj-clone\SKILL.md`(Claude Code 用户级 skill,自动触发);
- 交付归档:`F:\MCP\proj-clone-skill\`(SKILL.md 明文 + proj-clone.skill 打包件 + 交付说明);
- 行为实测(子代理两场景):正例(模板派生+"先看看")自主触发 skill、dry-run 先行、零落盘;负例(目标 C:\Projects+"直接干")按白名单拒绝并给出 F 盘替代路径,"直接干"被正确理解为不豁免白名单。两场景均通过,skill 未需修改。

### 审计修复记录(2026-07-27,v1.0.0 → v1.1.0)

对工具做对抗性审计(自审 + 独立子代理交叉,发现 19 项),按高/中/低三级全部修复:

- **高危 4 项**:①改名替换改为"锚定行 + 词边界"策略——.ioc 只改 ProjectManager 两行、CMakeLists 锚定 set(CMAKE_PROJECT_NAME) 行(大小写不敏感/允许引号),注释与 README 用词边界替换且旧名短于 5 字符时只报残留不替换(根治 `H743` 撞 `STM32H743XIH6`、`UI` 撞路径的静默损坏);②.ioc 改名用 os.replace 防 --force 重跑 FileExistsError,且先读后写、读取失败零副作用;③main 顶层异常兜底归 exit 2(Python 裸 traceback 退出码恰为 1,会被误读成"仅警告");④多级路径排除模式(如 docs/build/)显式拒绝,不再静默失效。
- **中危 7 项**:遍历统一收口到 apply_excludes(junction/符号链接跳过防物化与链接环、onerror 收集防无权限目录静默丢失并判失败);对账 extra 与残留扫描复用同一排除剪枝(--force 合并不再被 build/ 噪声淹没),历史克隆报告.md 无条件跳过;--report 护栏前移到克隆前且禁指源内;dry-run 与真跑一致性(目标已存在时预告 exit 2、源侧预检 .ioc);.ioc/CMakeLists 编码错误优雅失败;文件名含旧名计入 exit 1。
- **低危 6 项**:Windows 保留设备名(CON/NUL/COM1-9 等)拦截;空 --scan-name 过滤;默认豁免补 `*/UI/Designer/**`(覆盖 UI/Designer 不在工程根的布局);系统敏感路径(C:\Windows 等)拒绝 + F:\STM32 之外软警告;大小写匹配语义与 `*.bak*` 边界文档化。
- **回归**:原 T1-T4 全过(T3 改用 H743_Instrument_CN——模板区已演进,H743_UI_Instrument 已弃用为 `_deprecated_*`,新模板豁免 46/非豁免 64,configure 成功);新增 T5 --force 重跑、T6 短名 H743 mini 工程(Mcu.CPN 原样保留)、T7 多级模式拒绝、T8 report 护栏零落盘、T9 dry-run 预告、T10 raw 归档、T11 GBK 优雅失败,共 11 项全过。测试产物已清理。
- skill 同步:模板名引用更新为 H743_Instrument_CN/_EN(以 模板索引.md 为准),exit 1 语义补文件名命中。
