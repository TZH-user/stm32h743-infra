# STM32工程移植方案与说明

## 1. 适用范围

本方案用于把v0.3双SPI页协议接入其他STM32工程。模板中的协议层可以直接复用，
端口层与业务层必须按目标工程重新检查。不要把当前工程的引脚、任务名和目录结构
当作固定要求。

## 2. 分层职责

| 层 | 文件 | 职责 | 移植时是否通常修改 |
|---|---|---|---|
| 协议层 | dual_spi_protocol.* | 页格式、CRC32C、TLV、序号 | 否 |
| 会话层 | dual_spi_session.* | ACK/NACK、重传、Credit、Reset Epoch | 否 |
| 传输状态机 | dual_spi_transport.* | Master/Slave收发状态与故障恢复 | 通常否 |
| STM32端口层 | dual_spi_stm32_port.* | HAL SPI、DMA、GPIO、Cache、ISR | 是 |
| 业务服务层 | fpga_link_service.* | 业务TLV封装、任务内协议调度 | 是 |
| RTOS入口 | app_fpga_link_task.* | 只调用服务循环 | 仅改任务名时 |

## 3. CubeMX配置流程

### 3.1 先确认硬件

- FPGA与STM32必须共地。
- 当前实例使用3.3V IO；其他硬件必须按实际Bank电压确认。
- SPI-A由STM32主动发起，SPI-B由FPGA主动发起。

### 3.2 当前实例引脚

| 信号 | STM32引脚 | 当前排针 | 方向 |
|---|---|---|---|
| A_READY | PA12 | P2-6 | FPGA → STM32 |
| A_CS | PB12 | P2-5 | STM32 → FPGA |
| A_MOSI | PB15 | P2-7 | STM32 → FPGA |
| A_SCK | PB13 | P2-8 | STM32 → FPGA |
| A_MISO | PB14 | P2-10 | FPGA → STM32 |
| B_NSS | PE2 | P3-35 | FPGA → STM32 |
| B_READY | PE6 | P3-39 | STM32 → FPGA |
| B_MOSI | PG14 | P3-41 | FPGA → STM32 |
| B_MISO | PG12 | P3-43 | STM32 → FPGA |
| B_SCK | PG13 | P3-44 | FPGA → STM32 |

### 3.3 外设参数

- SPI-A：Master、Mode 0、8 bit、MSB first、软件NSS、初始7.5MHz。
- SPI-B：Slave、Mode 0、8 bit、MSB first、软件NSS。
- A_READY：上升沿EXTI。
- B_NSS：必须是上升沿和下降沿EXTI；下降沿识别开始，升高时检查提前结束。
- SPI2 RX/TX：当前使用DMA1 Stream2/3。
- SPI6 RX/TX：当前使用BDMA Channel0/1。

可分别应用 CubeMX/SPI2_DMA_EXTI_Profile_UTF-8.json 和
CubeMX/SPI6_BDMA_EXTI_Profile_UTF-8.json。应用前必须dry-run并审查引脚冲突；
Profile只作为起点，不保证适用于其他MCU封装。

### 3.4 RTOS任务

在目标 .ioc 中创建任务，例如：

- 名称：FpgaLinkTask
- 入口：StartFpgaLinkTask
- 优先级：High
- 初始栈：512 words

任务入口保持为：

~~~c
void StartFpgaLinkTask(void *argument)
{
    (void)argument;
    FpgaLink_Run();
}
~~~

真机运行后再依据High Water Mark缩减或增加栈，不把协议细节写入入口文件。

## 4. DMA内存与Cache

STM32H743必须按DMA控制器可达性放置缓冲区：

- SPI2的DMA1页：放入RAM_D2或其他DMA1可达RAM。
- SPI6的BDMA页：放入RAM_D3。
- 每页1024字节，地址32字节对齐。
- D-Cache开启时，发送前Clean，接收前/后Invalidate。

链接脚本需要类似：

~~~ld
.bdma_buffer (NOLOAD) :
{
  . = ALIGN(32);
  KEEP(*(.bdma_buffer))
  KEEP(*(.bdma_buffer*))
  . = ALIGN(32);
} >RAM_D3
~~~

构建后必须检查map文件，不能只看源码属性。

## 5. 回调合并

如果工程已有 HAL_GPIO_EXTI_Callback()，不得在端口层再次定义。应由工程唯一
回调统一分发：

~~~c
void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
    DualSpiStm32Port_OnGpioExti(gpio_pin);
    OtherModule_OnGpioExti(gpio_pin);
}
~~~

SPI完成和错误回调也要先检查工程是否已有实现；若有，同样建立统一分发。

## 6. 当前业务TLV

类型：0x0301，tag作为分析会话序号。正文固定12字节、小端序：

| 偏移 | 长度 | 内容 |
|---|---:|---|
| 0 | 1 | version，当前为1 |
| 1 | 1 | valid，0无效/1有效 |
| 2 | 1 | 波形A，0未知/1正弦/2三角 |
| 3 | 1 | 波形B，0未知/1正弦/2三角 |
| 4 | 4 | 频率A，mHz，uint32 |
| 8 | 4 | 频率B，mHz，uint32 |

当前工程在 SignalVote_Finalize() 后只调用一次
FpgaLink_SubmitSignalResult()。后续工程可以替换业务TLV，但不要修改页协议来承载
业务差异。

## 7. CMake接入

把模板源码加入目标可执行文件，并加入以下头文件目录：

- APP/FpgaLink
- APP/RTOS
- BSP/FPGA

CubeMX生成文件继续由 cmake/stm32cubemx 管理；用户协议文件不要放入CubeMX
可能覆盖的生成目录。

## 8. 最小验证顺序

1. .ioc validate为0错误。
2. CubeMX重新生成后审查Git差异，确认USER CODE未丢失。
3. 运行三组主机测试。
4. Debug构建通过，检查map中的 .dma_buffer 与 .bdma_buffer。
5. 烧录后检查启动、Fault、FreeRTOS堆、各任务High Water Mark。
6. FPGA接入后先验证Reset Epoch与ACK，再验证业务TLV 0x0301。

## 9. 当前模板边界

- 模板不包含FPGA端RTL。
- 模板不保证其他STM32系列具有BDMA。
- 当前业务只上报分析结果；FPGA下发命令接口预留但未定义。
- SPI速率7.5MHz是保守起点，提速必须用逻辑分析仪或示波器重新验收。

