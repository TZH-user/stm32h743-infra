# STM32H743 与 ZYNQ 双 SPI 通信模板

## 用途

本模板保存一套可移植的 STM32H743 ↔ ZYNQ 双 SPI 页协议实现。它来自
signal_separation 工程的实际接入，但不要求后续工程使用相同目录、引脚、
SPI实例、DMA通道、RTOS任务名或业务TLV。

## 目录

- Protocol/：v0.3通信协议原文。
- CubeMX/：SPI2与SPI6的可选CubeMX配置Profile。
- Template/APP/FpgaLink/：协议、会话、传输状态机和业务服务示例。
- Template/BSP/FPGA/：STM32 HAL、DMA、Cache和GPIO端口示例。
- Template/APP/RTOS/：保持简短的CubeMX任务入口。
- Tests/：不依赖STM32 HAL的主机单元测试。
- Docs/：移植步骤、当前工程实例和参考资料。

## 当前实例

- SPI-A：STM32为Master，使用SPI2 + DMA1。
- SPI-B：STM32为Slave，使用SPI6 + BDMA。
- 页大小：1024字节。
- 模式：CPOL=0、CPHA=0、8 bit、MSB first。
- 当前业务TLV：0x0301，发送两路波形类型与频率。

## 使用前必须重新确认

1. 目标MCU是否具有SPI6和BDMA；没有时必须重新选择外设和DMA。
2. FPGA Bank电压与STM32 IO电压是否一致。
3. 引脚复用、排针位置、EXTI线和现有外设是否冲突。
4. DMA缓冲区是否放在对应DMA控制器可访问的RAM。
5. 工程是否已经存在HAL GPIO/SPI回调；必须统一分发，不能重复定义。
6. FreeRTOS任务、栈、堆和中断优先级是否适合目标工程。

完整步骤见 Docs/STM32工程移植方案与说明_UTF-8.md。

