# signal_separation 工程入口

> 新成员、AI 或比赛现场排障时，先读本文件，再进入源码。

## 1. 当前主线

- MCU：STM32H743XIH6
- 分支：`feature/dual-phase-lock`
- 当前整理基线：`7b3ac78`（既有真机功能验收基线：`1b68802`）
- 工程生成源：`signal_separation.ioc`
- 构建入口：`CMakeLists.txt`，预设见 `CMakePresets.json`
- 固件入口：`Core/Src/main.c`
- 当前业务入口：`APP/RTOS/app_main_task.c`
- 持续跟踪入口：`Stm32SignalAdapters/src/cmsis_rtos_tracking_task.c`
- 串口输出入口：`APP/RTOS/app_output_task.c`
- 详细主线索引：`Docs/当前主线索引_UTF-8.md`

## 2. 目录边界

- `SignalCore/`：不直接包含 AD9226、HAL 或 CMSIS-RTOS 头文件的信号算法库。
- `Stm32SignalAdapters/`：AD9226、双 DAC DMA、UART DMA、CMSIS-RTOS 跟踪任务等 STM32 适配层。
- `APP/`：本题业务编排、输出协议、UI 适配和工程配置。
- `BSP/`：显示、触摸、SDRAM 等板级功能；AD9226 实现已抽到适配层，原路径仅保留兼容头/空翻译单元。
- `Core/`、`Drivers/`、`Middlewares/`：CubeMX/HAL/中间件生成或第三方内容。
- 历史实现与阶段方案统一通过 Git 提交记录查看，不在当前工程目录保留复制归档。

## 3. 主流程

1. `StartMainTask()` 初始化 STM32→SignalCore 端口，采集并完成一次多帧识别。
2. 有效 E7 结果通过 `AppPhaseLockTask_RequestStart()` 交给跟踪任务。
3. 跟踪任务周期采集短帧，调用 `SignalJoint_TrackKnown()` 和 `AppPhaseLock_Update()`。
4. `AppDacOutput_*()` 通过注入的 DAC1/TIM6/DMA2/RAM_D2 资源持续输出双通道波形。
5. `StartPrintfTask()` 独占 VCP；日志格式、波形流和队列核心已分文件。
6. 双正弦整数倍模式下，`PhaseLockTask` 以 A' 定义公共时间原点，并按 UI/API 设置的 `0..180 deg`、`5 deg` 步进生成 B' 初相位。

## 4. 修改门禁

- 不改变任务数量、任务所有权、DMA 控制器分配和大型缓冲内存段。
- 修改采样率、FFT 长度、DAC 采样率或频率边界时，先检查 `APP/Config/app_config_contracts.h`。
- 修改算法时优先改 `SignalCore/`；硬件差异放入 `Stm32SignalAdapters/`。
- 历史方案只允许从 Git 新建独立分支验证，不直接覆盖当前主线。
- 相位设置入口为 `AppPhaseLockTask_SetOutputPhaseDifference()`；UI 不直接启停或更新 DAC。
