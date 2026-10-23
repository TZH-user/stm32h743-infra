# SignalCore

可复用的纯信号处理库。公共 API 位于 `include/`，实现位于 `src/`。

平台必须在调用预处理或 FFT 前配置 `SignalCorePort`。STM32 工程使用 `SignalCoreStm32Adapter_Init()` 完成绑定；PC 测试可注入数组码值转换和单调计时函数。

本目录不应包含 `ad9226.h`、`cmsis_os2.h`、`dac.h`、`tim.h` 等工程硬件头文件。
