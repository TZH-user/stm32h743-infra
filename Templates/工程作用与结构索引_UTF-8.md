# Templates 工程作用与结构索引

> 编码：UTF-8  
> 扫描日期：2026-07-26  
> 用途：说明模板的适用场景、结构、复用边界和其他 AI 应优先借鉴的内容。

## 1. 模板总览

| 模板 | 作用 | 适合借鉴 | 复用前必须确认 |
| --- | --- | --- | --- |
| `LTDC(touch+fmc+LVGL)、vcp、swo、Rtos` | STM32H743 显示系统通用模板，整合 FMC/SDRAM、LTDC、GT9xx 触摸、LVGL、VCP/SWO 与 FreeRTOS | 显示/触摸/LVGL 端口拆分、显示任务入口、内存映射、BSP/APP/UI 分层、CMake 接入 | 屏幕时序和像素格式、SDRAM 型号与地址、触摸 I2C/中断、Cache、任务栈、VCP/SWO 引脚冲突 |
| `STM32H743_ZYNQ_DualSPI_Link_Template` | STM32H743 与 ZYNQ-7020 PL 的双 SPI 固定页通信模板 | 协议/会话/传输/端口/业务/RTOS 六层解耦；CRC32C、ACK/NACK、Credit、重传、Reset Epoch；主机单元测试；DMA/Cache 约束 | MCU 是否有 SPI6/BDMA、FPGA Bank 电压、引脚冲突、DMA 可达 RAM、Cache 维护、唯一 HAL 回调分发、RTOS 栈和中断优先级 |

## 2. `LTDC(touch+fmc+LVGL)、vcp、swo、Rtos`

### 作用

这是从 `AD9226_API.ioc` 派生的 STM32H743 综合显示模板。它适合作为“先把板级显示与人机界面跑通，再叠加业务”的起点，而不是具体采集算法或仪器业务成品。

### 结构

- `APP/Config/app_memory_map.h`：集中记录 framebuffer、draw buffer 等内存地址约定。
- `APP/RTOS/app_display_task.c`：显示/LVGL 的任务级入口。
- `APP/RTOS/PrintfTask.c`：运行期输出任务参考。
- `BSP/LTDC`：显示控制与面板相关板级封装。
- `BSP/SDRAM`：外部 SDRAM 初始化后的测试与使用辅助。
- `BSP/Touch`：GT9xx 触摸控制器适配。
- `UI/Port`：LVGL display/indev 端口。
- `UI/pages`：页面与控件逻辑。
- `Core`、`Drivers`、`Middlewares`：CubeMX/HAL/第三方生成内容。

### 其他 AI 应借鉴什么

1. 把板级驱动放在 `BSP`，任务编排放在 `APP/RTOS`，UI 和 LVGL port 放在 `UI`。
2. 用独立 `app_memory_map.h` 明确绝对地址及其所有权，修改后再用 map 文件核验链接管理的对象。
3. `app_display_task.c` 保持为短入口，不把所有 LTDC/LVGL 细节塞入 CubeMX 任务函数。
4. 触摸输入和显示刷新分别通过 LVGL port 接入，避免 UI 页面直接调用 HAL。

### 边界

- **有依据**：目录、`.ioc`、CMake 和用户源码确认上述模块存在。
- **未确认**：本次未发现 `.codegraph`，未建立完整调用链，也未执行编译、烧录或实机显示验证。
- 模板名中的 VCP/SWO 只表示配置参考；PB3 等复用脚在不同工程中可能与并口 ADC 冲突，必须以目标硬件为准。

## 3. `STM32H743_ZYNQ_DualSPI_Link_Template`

### 作用

保存一套来自实际工程接入、但已经去除具体业务绑定的 STM32H743 ↔ ZYNQ 双 SPI 页协议模板。当前参考实例为 SPI2 Master + DMA1 和 SPI6 Slave + BDMA，每页 1024 字节。

### 结构

- `Protocol/`：v0.3 通信规范，定义页格式、CRC32C、序号、ACK/NACK、Credit、重传和 Reset Epoch。
- `CubeMX/`：SPI2/SPI6 的可选配置 Profile；只能 dry-run 后审查使用。
- `Template/APP/FpgaLink/dual_spi_protocol.*`：纯页面编码、CRC 与 TLV。
- `dual_spi_session.*`：会话可靠性、ACK/NACK、Credit、重传和复位同步。
- `dual_spi_transport.*`：双链路 Master/Slave 传输状态机。
- `fpga_link_service.*`：业务 TLV 和任务上下文调度。
- `Template/BSP/FPGA/dual_spi_stm32_port.*`：HAL、DMA、GPIO、Cache 和 ISR 适配。
- `Template/APP/RTOS/app_fpga_link_task.*`：保持简短的 RTOS 入口。
- `Tests/`：不依赖 STM32 HAL 的协议、会话与传输主机测试。

### 其他 AI 应借鉴什么

1. 协议层不依赖 HAL；换板时通常只改 STM32 port 和业务 service。
2. DMA1 页面放 DMA1 可达 RAM，BDMA 页面放 RAM_D3，32 字节对齐；D-Cache 开启时显式 Clean/Invalidate。
3. HAL GPIO/SPI 回调必须由工程唯一入口统一分发，不能由模板重复定义。
4. RTOS 任务只调用 `FpgaLink_Run()`，协议细节留在 service/transport/session 层。
5. 先跑主机单元测试，再构建并检查 map，最后接 FPGA 验证 Reset Epoch、ACK 和业务 TLV。

### 边界

- **有依据**：README、移植说明、v0.3 协议和模板源码。
- 模板不包含 FPGA 端 RTL；当前业务 TLV `0x0301` 只是示例。
- `SPI6 + BDMA`、引脚、7.5 MHz 起始速率和 3.3 V IO 都不能机械复制到其他 MCU/板卡。
- 本次未发现 `.codegraph`；需要精确调用链时先在模板根目录初始化索引。

## 4. 模板选择建议

| 当前任务 | 使用模板 | 起步位置 |
| --- | --- | --- |
| H743 RGB 屏、触摸和 LVGL | `LTDC(touch+fmc+LVGL)、vcp、swo、Rtos` | 先检查 `.ioc`、`app_memory_map.h`、`app_display_task.c` |
| H743 与 ZYNQ 双向批量数据通信 | `STM32H743_ZYNQ_DualSPI_Link_Template` | 先读 README、协议 v0.3、移植说明，再运行 `Tests` |
| 只需要单 SPI 或简单寄存器读写 | 不建议直接搬完整双 SPI 协议 | 先评估是否真的需要页协议、重传和 Credit |

## 5. 后续维护要求

- 模板新增实机验证结论时，写明板卡、频率、连接方式、固件/RTL 版本和测试证据。
- 模板接口变化时同步更新 README、协议、移植说明和测试。
- 从模板创建新工程时应修改工程身份，不要保留旧 `.ioc` 名、CMake project 名或构建产物名。
