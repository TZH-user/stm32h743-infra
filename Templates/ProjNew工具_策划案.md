# ProjNew —— 新项目一条龙工具 策划案

- 日期:2026-07-27
- 起草:Claude Code;交付给实施 AI 独立完成,无需起草会话上下文
- 前置依赖:`F:\STM32\Templates\_tools\projclone.py`(已验收)、Templates 三模板(已验收)、`F:\STM32\stm32project\Function\` 模块库
- 状态:待实施

---

## 1. 动机

projclone 解决了"复制得对",但从模板到**可开工工程**还剩三步手工活:
1. 从 Function\Hardware 复制所需模块目录到工程约定路径;
2. 在 CMakeLists.txt 解注释对应模块的 target_sources / include 行;
3. Instrument 模板派生后常要裁掉校准/串口遥控等业务("骨架化")。

三步全部机械可判定——模板 CMakeLists 的模块注释是**成组机器可解析格式**(`# --- <模块名>(...;拷到 <目标路径>/) ---` 起始,组内为注释掉的源文件行),裁剪对象也是有限清单。固化为 `projnew.py`。

## 2. 形态

单文件 CLI `F:\STM32\Templates\_tools\projnew.py`,纯标准库,内部复用 projclone.py(以 `import` 或 subprocess 方式调用,实施者自选,倾向 import:`from projclone import ...` 需 projclone 结构允许,否则 subprocess)。

```
python projnew.py --from <模板名或路径> --name <新工程名>
                  [--dst-root F:\STM32\stm32project]      # 目标父目录,默认此值
                  [--modules DDS_AD9959,CLK_SI5351,...]   # Function\Hardware 模块选装
                  [--trim ui-core|none]                   # 裁剪档位,默认 none
                  [--list-modules] [--list-trims]         # 枚举可选项
                  [--dry-run] [--report FILE]
```

## 3. 功能规格

### F1 派生
调 projclone(profile=template-derive)完成克隆与三处改名;projclone 的护栏(白名单、dry-run、豁免)全部继承,不重复实现。

### F2 模块选装(--modules)
每个模块执行两个动作:
1. **复制**:`Function\Hardware\<模块>\` → 工程内约定路径。路径映射表内置(与模板 CMakeLists 注释一致):

| 模块 | 工程内路径 |
|---|---|
| ADC_AD9226 | BSP\ADC\(仅 ad9226.c/h;ad9226_config.h → APP\Config\) |
| DDS_AD9959 | APP\DDS_9959\(保留 Hardware/App_9959 子结构) |
| DDS_AD9910 | APP\DDS_9910\ |
| CLK_SI5351 | APP\SI5351\(保留 Config/Port/Wrapper/Legacy 子结构;.md 文档一并带) |
| Flash_W25Q64_QSPI | APP\BoardFlash\ |
| DAC_WaveGen | APP\DAC\ |
| Buzzer | APP\Buzzer\ |
| OCXO_Cal | APP\OCXO\ |
| LCD_LTDC_Display / SDRAM_Test / Touch_GT9xx / LVGL_Port / Debug_IO | 不提供选装(模板已预装),--modules 给出时报错提示 |

2. **解注释**:在工程 CMakeLists.txt 中定位该模块的注释组(以 `# --- ` + 模块关键词开头,至下一空行/下一组止),把组内 `# <path>.c` 行解为 `<path>.c`;target_include_directories 里对应 `# <dir>` 行同样解开。**只处理精确匹配的行**,匹配数与内置预期数核对,不符即中止(参照 projclone 的 re.subn 强校验模式)。
3. 依赖提示:OCXO_Cal 依赖 Flash_W25Q64_QSPI 与 DDS 控制头——若 --modules 含 OCXO_Cal 而缺依赖,警告并列出(不自动加装,人决定)。

### F3 裁剪档位(--trim,仅对 Instrument 模板有效)
`ui-core`:从全功能裁到"UI+采集+FFT 骨架"。动作清单(实施时以 CN 模板实际文件为准核对):
- 删源+CMake 行:APP\Calibration\*、APP\SerialControl\*、APP\RTOS\app_calibration_worker_task.c、APP\RTOS\app_output_task.c、APP\RTOS\app_serial_control_task.c、UI\Adapter\app_calibration_ui_adapter.*、CalibrationCore 子目录及其 add_subdirectory/link 项、Stm32SignalAdapters 中仅服务校准/输出的源(实施时逐文件核对引用,拿不准就保留该文件);
- 清 Core\Src\freertos.c USER CODE 区两处业务引用(AppSerialControlTask_Init/AppUiRemoteControl_Init 与对应 include)——精确正则 + subn 计数校验,模式照抄 H743_LVGL_Baseline 制作脚本;
- 裁后必须:configure+build 通过、上电 UI 六屏可浏览(校准/测试页按钮无功能属预期,标注 TBD 即可)。
- Baseline 模板 --trim 无意义,给出时报错。

### F4 校验与报告
projclone 报告之外追加:模块复制清单与文件数、CMake 解注释行数(预期 vs 实际)、trim 删除清单、末尾自动跑 `cmake --preset Debug`(configure 必须通过,--dry-run 除外)。报告写入 `<新工程>\projnew报告.md`。

### F5 dry-run
全链路动作清单(含将解开的 CMake 行原文)打印,不落盘。

## 4. 实现要点(经验教训,必读)

1. 子进程/文本处理规矩全部沿用 projclone 策划案 §5(CRLF 保留、subn 计数强校验、UTF-8、动态对账)。
2. **控制台实时输出**:print 加 flush(或 line_buffering),多分钟步骤(克隆/编译)必须有阶段性进度行——2026-07-26 双语模板脚本因 capture_output 全程静默被用户当成卡死,此为硬性要求。
3. 调 projclone 若用 subprocess:不要包 uv run(uv cache 锁曾造成第二进程静默等待);直接 `sys.executable`。
4. CMakeLists 注释组解析:以行首 `    # --- ` 定位组头,组员行形如 `    # APP/DDS_9959/Hardware/ad9959.c`——解开=去掉 `# ` 保留缩进;include 区同理。CN/EN 两模板的注释组文本一致(EN 未翻译 CMake 注释),解析器写一份即可;Baseline 的组文本同源。
5. 模块目标路径若与模板既有目录冲突(如再次选装已存在模块):默认拒绝,--force 才覆盖。

## 5. 验收测试(必须实际执行并贴输出)

- T1 `--list-modules` / `--list-trims` 输出与本案 §3 一致;
- T2 从 Baseline 派生 + `--modules DDS_AD9959,CLK_SI5351`:模块文件落位、CMake 解注释行数符合预期、configure 通过;
- T3 从 H743_Instrument_CN 派生 + `--trim ui-core`:删除清单符合 §F3、configure+build 通过;
- T4 `--modules OCXO_Cal`(缺依赖):出现依赖警告;
- T5 `--dry-run` 零落盘;
- T6 对已存在目标无 --force:拒绝。
- 测试工程放 `F:\STM32\_projnew_test\`,测毕清理。

完成后在本文件末尾追加"实施记录"(日期/路径/六项结果),并把 projnew 用法一节合入 proj-clone 技能文档(或新建 proj-new 技能,实施者判断后向用户提议,二选一)。

## 6. 不要做的事

- 不改 projclone.py 既有行为(需要新钩子时先在实施记录里说明);不动 Templates 三模板与 Function 库内容(F3 的删除只发生在派生出的新工程);不引第三方依赖;MCP 化不做(用户已否决)。
