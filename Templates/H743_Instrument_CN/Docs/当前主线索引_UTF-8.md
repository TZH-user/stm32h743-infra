# 当前主线索引

## 基线与用途

- 分支：`feature/dual-phase-lock`
- 当前架构提交：`2892434`
- 已有真机功能验收基线：`1b68802`
- 用途：落实可复用代码抽离、工程适配、输出任务拆分、旧实现归档、配置一致性门禁和文档入口收敛。

本次修改不改变题目主线：混合信号经 AD9226 采集，一次识别后持续跟踪，最后通过片内双 DAC 重构两路信号。

## 入口文件

| 目的 | 文件 |
| --- | --- |
| 工程总入口 | `README_UTF-8.md` |
| CubeMX 配置源 | `signal_separation.ioc` |
| 构建清单 | `CMakeLists.txt`、`SignalCore/CMakeLists.txt`、`Stm32SignalAdapters/CMakeLists.txt` |
| MCU 启动 | `Core/Src/main.c` |
| 一次识别 | `APP/RTOS/app_main_task.c` |
| 持续跟踪 | `Stm32SignalAdapters/src/cmsis_rtos_tracking_task.c` |
| 输出任务核心 | `APP/RTOS/app_output_task.c` |
| 日志格式器 | `APP/RTOS/app_output_log_formatter.c` |
| ADC/FFT 波形流 | `APP/RTOS/app_output_wave_stream.c` |
| UI 数据边界 | `UI/Adapter/app_ui_adapter.c` |
| 配置一致性门禁 | `APP/Config/app_config_contracts.h` |

## 当前任务与所有权

| 任务 | 所有权 |
| --- | --- |
| `MainTask` | 一次会话、AD9226 逐帧采集、FFT/分类/投票、启动跟踪 |
| `PhaseLockTask` | 持续 AD9226 短帧、双频 I/Q、PI-DPLL、DAC DMA 补数 |
| `PrintfTask` | VCP 队列、模式切换和实际 UART 发送 |
| `DisplayTask` | LVGL 循环、页面控制与串口 UI 命令消费 |
| `TouchTask` | 触摸中断与输入状态 |
| `SerialControlTask` | USART1 行协议接收与命令解析 |
| `CalWorkerTask` | 校准服务状态机、板级回环/自检与 QSPI 存储 |

## 当前输入与输出接口

| 方向 | 接口 | 说明 |
| --- | --- | --- |
| 模拟输入 | AD9226 + TIM2/DMA | 输入混合信号 C，采样帧由 `AD9226_Start/Stop/GetFrame` 管理 |
| 算法输入 | `SignalCorePort` | 注入原始码转换和单调高分辨率计时，不让核心算法直接依赖 BSP/RTOS |
| 阶段交接 | `SignalVoteResult` | E7 有效结果作为持续跟踪的频率和波形种子 |
| 模拟输出 | DAC1 OUT1/OUT2 + TIM6/DMA2 | 通过 `AppDacOutputPort` 注入 HAL 句柄、DMA 状态寄存器和 RAM_D2 缓冲 |
| 诊断输出 | UART/VCP | `PrintfTask` 单一所有者；支持日志、ADC 波形、FFT 波形模式 |
| UI 输出 | 快照 + 控制 | UI Adapter 读取结果快照；Monitor RUN/STOP、相位键与 Test/Calibration 页按钮直接向 Main/PhaseLock/Calibration 服务发请求（2026-07-26 起） |

## 可复用边界

### SignalCore

公共头位于 `SignalCore/include/`，实现位于 `SignalCore/src/`。核心通过 `signal_core_port.h` 注入原始采样码转换和高分辨率计时，因此不直接依赖 AD9226、HAL 或 CMSIS-RTOS。

当前包含：预处理、FFT、双正弦估计、谐波分类、联合模型、投票、三角函数查表和 PI-DPLL。

### Stm32SignalAdapters

当前包含：

- `ad9226_hal_adapter.c`
- `dual_dac_dma_adapter.c`
- `signal_separation_dac_port.c`
- `cmsis_rtos_tracking_task.c`
- `uart_dma_output_service.c`
- `signal_core_stm32_adapter.c`

双 DAC 模块不再直接引用 `hdac1`、`htim6`、`DMA2` 或内部固定缓冲；这些资源由 `AppDacOutputPort` 注入，并在初始化时检查 TIM6 实际分频、RAM_D2 范围和 32 字节对齐。

旧 `APP/*`、`BSP/ADC/*` 公共路径保留兼容头和空翻译单元，已有业务 `#include` 不需要一次性全部修改。

## 配置门禁

集中契约：`APP/Config/app_config_contracts.h`。

它检查：

- AD9226 帧长、预处理点数和 FFT 点数一致。
- 锁相采样率不超过 AD9226 驱动上限。
- 锁相与 DAC 的频率边界一致。
- DAC 缓冲满足双半缓冲、NDTR 和 TIM6 采样率约束。
- 输出初相位范围固定为 `0..180 deg`，设置步进固定为 `5 deg`。
- 输出环形队列可完整容纳一帧 ADC 数据。

## 历史实现

旧 `app_signal_pipeline.*`、`app_adc_task.*` 和 `app_fft_task.*` 已退出当前目录。
需要核查时从 Git 提交 `566be5d` 查看；恢复历史方案必须新建分支，不得覆盖当前主线。

## 正式验收依据

优先查看以下已有记录：

- `Docs/信号识别_E9简化测试与交付报告_20260723_UTF-8.md`
- `Docs/工程开发复盘与架构评估报告_20260724_UTF-8.md`
- `Docs/AD9226前级幅度余量实测_20260719_UTF-8.md`
- `文件改动记录_UTF-8.md` 中的 1.6 MS/s DAC/PI-DPLL 真机结论

## 本次验证与剩余风险

架构抽离后的 ARM GNU Debug 全量构建已通过，无重复全局符号或未解析符号：

- FLASH：829748 B。
- DTCMRAM：98272 B。
- RAM_D2：148 KiB。
- DAC 缓冲：`0x30000000 / 0x20000`。
- ELF、HEX、BIN 已同步生成。

本次架构清理后仍未重新烧录，因此真机功能继续以提交 `1b68802` 的既有验收为基线。
后续只需复测锁相启动/停止、DAC 连续输出、VCP 三种输出模式和外部模拟链幅相。

后续维护的主要风险只保留三项：目标工具链/真机回归、模拟链校准、`PrintfTask/MainTask` 栈余量；不要在比赛版本继续改变任务架构或 DMA 分配。

## 2026-07-25 相位设置接口

- 适用条件：A、B 均为正弦波，且 `fB/fA=N` 为大于等于 2 的整数，符合题目发挥部分（2）。
- 设置入口：右侧 `B'-A' PHASE` 的减/加键，或 `AppPhaseLockTask_SetOutputPhaseDifference()`。
- 生成关系：`phase_B = N * phase_A + phi`；两路频率由同一公共基频生成，避免独立频率估计误差造成初相位慢漂。
- 验收字段：屏幕显示 `LOCKED B'-A' <角度> deg`；串口 `PLL_DAC` 增加 `PHASE_CTRL/PHASE_DEG/RATIO`。
- 静态验证：37 个合法设定值与 4 个非法值边界检查通过，Debug 构建通过；FLASH `831360 B`、DTCMRAM `98296 B`、RAM_D2 `148 KiB`，DAC 缓冲保持 `0x30000000 / 0x20000`。
- 未完成证据：实际相位误差和外部缓冲固定相移仍需示波器测量，不能由编译或内部状态代替。

## 2026-07-26 UI 管理现状

- UI 已不是纯只读快照：Monitor RUN/STOP、相位设置键、Test 页六档自检与
  Calibration 生命周期按钮直接调用 Main/PhaseLock/Calibration 服务。
- 校准/自检/QSPI 硬件路径由 `CalWorkerTask` 承载；DisplayTask 只保留
  LVGL 循环与远程 UI 命令消费。
- 串口可通过 `UI PAGE ...`/`UI TAP ...` 驱动页面与按钮；TAP 回显附带
  业务状态行供上位机核对受理结果。
- 问题清单与整改状态见 `Docs/UI管理审计报告_20260726_UTF-8.md`、
  `Docs/UI审计整改记录_20260726_UTF-8.md`。
