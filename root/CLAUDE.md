# F:\STM32 —— STM32 工程区约定(AI 会话必读)

平台:STM32H743(HAL + FreeRTOS/CMSIS-RTOS2 + LVGL 9.5 + LVGL Editor),工具链 Arm GNU + CMake/Ninja + STM32_Programmer_CLI(SWD)。

踩坑档案:**避坑须知_UTF-8.md**(git 嵌套仓库、克隆工具语义、PowerShell 管道坑、多会话并行规则——动手前过一眼对应小节)。

## 目录地图

- `Templates\` —— 模板区(**只作克隆源,严禁直接在里面开发**;对照表见 Templates\模板索引.md)
  - H743_LVGL_Baseline(最小点亮)/ H743_Instrument_CN / _EN(全功能仪器,双语)
  - `_tools\projclone.py`(克隆)、`_tools\template_selftest.py`(模板冒烟)
- `stm32project\Function\` —— 模块库:Hardware\ 13 个硬件驱动 + Algorithm\ 9 组算法(索引见其根 README)
- `stm32project\VSCodeCMAKEProject\` —— 存量工程区;signal_separation 为最新活跃工程,是 Instrument 模板与 Function\Algorithm 的上游
- `stm32project\<新项目>` —— 新项目落位处(短路径、无空格)

## 硬约定

1. **新项目 = 从模板派生**,一律走 proj-clone 技能(projclone.py),dry-run 先行;不要手写 Copy-Item/robocopy 复制工程。模板目录绝不可作克隆目标。
2. **硬件驱动不重写**:先查 Function\Hardware 有没有;接入=复制模块目录+解开工程 CMakeLists 对应注释组(各模板已埋好成组注释)。结项把有价值的驱动改动 diff 回流 Function。
3. **模板改动纪律**:改完必跑 `_tools\template_selftest.py`;CN/EN 成对同步(EN 翻译映射在其 translation_map.md);更新 Templates\模板索引.md 版本列;git commit。
4. **CubeMX 边界**:用户代码只进 BSP\/APP\/UI\,不写 Core\Src(USER CODE 区除外);RTOS 任务实现用"覆盖 freertos.c __weak 符号"模式放用户文件。
5. **LVGL Editor 工程**(UI\Designer):界面改动改 XML 后 Generate,勿手改 *_gen.* 文件;库名(signal_separation_ui)改名必须走 Editor;运行时不要对 Designer 屏做二次 screen_*_create()(重复实例化会卡死——2026-07-26 实测教训)。
6. **build\ 与备份不入库**:git 已配白名单 .gitignore;历史目录级备份习惯以 git 提交替代,确需留档放 `_archive\`。
7. 烧录:`STM32_Programmer_CLI -c port=SWD mode=UR -d <elf> -v -rst`(路径 C:\Users/<user>\AppData\Local\stm32cube\bundles\programmer\2.22.0+st.1\bin\);烧录/RF 输出等硬件操作先向用户确认再执行。

## 脚本编写规矩(在本区写自动化脚本时)

- 文本替换:UTF-8、`newline=""` 保留 CRLF、re.subn 计数强校验、只动白名单后缀;二进制一律字节复制。
- 长任务必须实时输出进度(勿 capture_output 全程静默);子进程输出捕获加 `encoding="utf-8", errors="replace"`。
- 大规模文件操作优先复用 projclone/(待实施的)projnew,不要临时手搓。

## 待实施

- `Templates\ProjNew工具_策划案.md`:派生+模块选装+裁剪一条龙(交任意实施 AI,验收标准在案内)。
