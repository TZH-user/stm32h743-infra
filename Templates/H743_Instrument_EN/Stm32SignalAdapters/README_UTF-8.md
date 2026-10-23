# Stm32SignalAdapters

将 SignalCore 连接到 STM32H743 的 HAL、DMA 和 CMSIS-RTOS。

- `signal_core_stm32_adapter.c`：绑定 AD9226 原始码转换和 RTOS 系统计时器。
- `ad9226_hal_adapter.c`：TIM2 + DMA 双缓冲采集。
- `dual_dac_dma_adapter.c`：双 DAC Q32 DDS、双半缓冲和 DCache 维护。
- `signal_separation_dac_port.c`：本工程的 DAC1/TIM6/DMA2/RAM_D2 资源注入。
- `cmsis_rtos_tracking_task.c`：持续锁相任务骨架。
- `uart_dma_output_service.c`：UART DMA 环形缓冲传输服务。

更换板卡时优先替换端口绑定文件，不修改 SignalCore。
