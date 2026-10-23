# H743_Instrument_CN —— 全功能仪器模板(中文界面)

## 版本与来源

- 克隆自 `F:\STM32\stm32project\VSCodeCMAKEProject\signal_separation` @ 2026-07-26(projclone,cubemx-cmake profile,已去 build/备份/.codegraph 并三处改名)
- 修订记录:无(中文版保持上游原样)
- 姊妹模板:H743_Instrument_EN(仅界面语言不同);轻量底座见 H743_LVGL_Baseline
- 上游仍在演进,同步流程见《模板索引.md》"上游同步"一节

## 这是什么

signal_separation 信号分离仪的**全功能克隆**:AD9226 采集 → CMSIS-DSP/SignalCore 分析管线 → LVGL Editor 六屏仪器 UI(监测/频谱/控制/系统/测试/校准)→ 双 DAC 输出 + QSPI 校准存储 + 串口遥控。上电行为与源工程完全一致,是"从完整仪器做减法"的起点;只要最小点亮请改用 H743_LVGL_Baseline。

## 开工流程

1. 用 proj-clone 技能(或 `python F:\STM32\Templates\_tools\projclone.py`)派生,profile 用 `template-derive`,目标放 `F:\STM32\stm32project\<新名>`(勿带空格)。
2. 进工程 `cmake --preset Debug && cmake --build build/Debug -j8`,烧录确认与源机行为一致。
3. `git init` 提交首个基线,再开始改。

## 结构速览

| 目录 | 内容 |
|---|---|
| Core\ | CubeMX 生成(.ioc 同名)+ lv_conf.h(LV_FONT_DEFAULT = font_cn_16) |
| BSP\ | LTDC / SDRAM / Touch(GT9xx)/ ADC 转发壳 |
| APP\ | RTOS 任务、Calibration、BoardFlash(W25Q64)、SerialControl、SignalDetect、Config、Debug |
| UI\Designer\ | LVGL Editor 工程(XML 源 + 生成代码,库名 signal_separation_ui / lib-ui) |
| UI\Custom + Adapter + pages | 手写 monitor 仪器屏 + 数据↔UI 适配层 + 组装入口 |
| UI\Fonts\ | font_cn_16(Noto Sans SC)+ 生成工具(tools\gen_font_cn.ps1) |
| SignalCore / CalibrationCore / Stm32SignalAdapters | 算法库三件套(独立 CMake 库) |

## 常见裁剪(做减法指引)

- **去校准业务**:CMakeLists 移除 APP/Calibration 三源 + UI/Adapter/app_calibration_ui_adapter.c + APP/RTOS/app_calibration_worker_task.c;freertos.c 无需动(任务 weak 空转)。
- **去串口遥控**:移除 APP/SerialControl 两源,并清 Core/Src/freertos.c USER CODE 区的 AppSerialControlTask_Init/AppUiRemoteControl_Init 与两个 include(参考 H743_LVGL_Baseline 的 freertos.c,该版已清好)。
- **去锁相/输出**:移除 APP/RTOS/app_output_task.c、app_phase_lock 相关;instrument 屏相位显示会保持 "--"。
- 更彻底的骨架化将由 ProjNew 工具提供(见 Templates\ProjNew工具_策划案.md)。

## UI 迭代

界面改动走 LVGL Editor:打开 `UI\Designer\project.xml` → 改屏/加屏 → Generate → 重编译(文件清单自动同步)。动态数据页参考 UI\pages\app_lvgl_ui.c 的组装模式(ui_init → 删除 Designer monitor 屏 → 挂手写 AppInstrumentScreen → AppUiAdapter 周期刷新)。**不要**在运行时对 Designer 屏二次 screen_*_create()(首版 UI 模板的卡死教训)。

## 历史文档说明

根目录《VCP与VOFA输出调用维护手册》《方案改动记录》《文件改动记录》来自上游,克隆时工程名字符串已被替换,溯源请以 signal_separation 原版为准;《克隆报告.md》为 projclone 生成的校验记录。
