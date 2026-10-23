# AD9226 VCP 与 VOFA+ 输出调用维护手册

## 1. 目的

本工程将 USART1/ST-LINK VCP 作为统一运行期输出通道。调度器启动后，只有 `OutputTask` 可以向 VCP 写数据，其他任务只能调用上层输出 API。

这样可以避免：

- 日志文字与 VOFA+ 波形混合；
- ADC 时域数据与 FFT 数据交叉；
- 多任务同时访问 UART；
- 为新增业务功能反复修改底层串口文件。

---

## 2. 文件职责与修改权限

### 2.1 日常允许修改

#### `APP/Config/app_output_config.h`

用于调整：

- 默认输出模式；
- ADC 波形目标显示点率；
- 输出队列容量；
- FFT 最大输出点数；
- 模式切换超时。

常用配置：

```c
#define APP_OUTPUT_DEFAULT_MODE                    2U
#define APP_OUTPUT_ADC_TARGET_POINTS_PER_SECOND    150000U
#define APP_OUTPUT_STREAM_QUEUE_DEPTH              34U
```

默认模式：

```text
0 = OFF
1 = LOG
2 = ADC_WAVE
3 = FFT_WAVE
```

`APP_OUTPUT_DEFAULT_MODE`只决定上电初始值。业务任务调用
`AppOutput_RequestMode()`后，运行时模式以最新请求为准。

#### 业务任务文件

业务任务只调用 `app_output_task.h` 中的接口，不直接操作 UART。

---

### 2.2 非必要不得修改

#### `APP/Debug/debug_io.c`
#### `APP/Debug/debug_io.h`

这是 VCP DMA 环形缓冲和底层传输层。只有出现以下情况才允许修改：

- 已确认的 DMA 环形缓冲缺陷；
- 更换 VCP UART；
- UART 回调所有权发生变化；
- 需要修改底层传输协议且经过架构评审。

普通日志、ADC 波形、FFT、算法结果接入都不应修改这两个文件。

#### CubeMX 生成文件

以下文件不得手工修改非 USER CODE 区域：

```text
Core/Src/usart.c
Core/Src/dma.c
Core/Src/stm32h7xx_hal_msp.c
Core/Src/stm32h7xx_it.c
```

串口波特率、DMA Stream、NVIC 等参数应通过 CubeMX 修改后重新生成。

---

### 2.3 谨慎修改

#### `APP/RTOS/app_output_task.c`
#### `APP/RTOS/app_output_task.h`

只有以下需求才需要修改：

- 新增一种明确互斥的输出模式；
- 修复已复现的输出管理问题；
- 增加新的通用发布接口。

仅增加一条日志、一个 FFT 任务或一个新的 ADC 算法，不应修改该文件。

---

## 3. 业务代码调用规则

### 3.1 日志

```c
#include "app_output_task.h"

AppOutput_Logf("[APP] value=%lu\r\n", (unsigned long)value);
```

规则：

- 仅任务上下文调用；
- 不在 ISR 中调用；
- 不直接调用 `vcp_printf()`；
- 不直接调用 `HAL_UART_Transmit()`；
- 日志单条尽量短于 120 字节。

---

### 3.2 切换模式

```c
AppOutput_RequestMode(APP_OUTPUT_MODE_LOG);
AppOutput_RequestMode(APP_OUTPUT_MODE_ADC_WAVE);
AppOutput_RequestMode(APP_OUTPUT_MODE_FFT_WAVE);
AppOutput_RequestMode(APP_OUTPUT_MODE_OFF);
```

查询实际生效模式：

```c
if (AppOutput_GetMode() == APP_OUTPUT_MODE_ADC_WAVE)
{
    /* 已完成切换 */
}
```

`AppOutput_RequestMode()` 是异步请求，不要在高优先级任务中死循环等待。

---

### 3.3 ADC 时域波形

当前主线上电默认只采集并完整输出一帧4096点 ADC 波形，不会启动 FftTask。
当前是最小验证程序，不提供运行时重复请求；需要再次采集时重新启动板卡。
MainTask直接执行“启动→等待一帧→停止”，随后调用下面的整帧接口：

```c
bool accepted = AppOutput_PublishAdcFrame(samples, sample_count);
```

完整帧接口的串口文本固定为两列：

```text
样本序号,ADC码值
```

例如：

```text
0,2048
1,2301
2,2529
3,2709
```

样本序号从 `0` 连续递增到 `4095`，ADC 码值保持 `0~4095`，每行只含两个纯数值，
不附加单位、时间戳或启动日志。桌面分析软件应选择“波形分析”，横轴选择“样本序号”，
波形幅值除数设置为 `1`；采样率填写板端实际采样率，用于把样本序号换算为时间。

2026-07-18 同步示波器 UI 后，同一帧还会通过
`AppUiAdapter_SubmitAdcFrame()`复制中间连续 128 点供屏幕显示。该接口不访问
UART，不会造成第二份串口波形，也不会改变完整 4096 点串口帧。

34个队列槽位提供`33 × 128 = 4224`点有效容量，可以容纳完整4096点帧。

下面的接口继续保留给未来连续 ADC 显示链路，它会按目标点率抽取：

```c
AppOutput_PublishAdcSamples(samples, sample_count, sample_rate_hz);
```

连续接口仍保留原有单列 FireWater 编码，当前 `signal_separation` 单帧流程不调用它。

完整帧接口只允许由MainTask在当前单次流程调用；连续接口只允许由未来明确的
连续采集任务调用。两者都不得与FFT频谱同时发布，其他任务不得重复发送同一份ADC数据。

显示点率在这里调整：

```text
APP/Config/app_output_config.h
```

```c
#define APP_OUTPUT_ADC_TARGET_POINTS_PER_SECOND 20000U
```

该值只影响连续接口`AppOutput_PublishAdcSamples()`，不影响完整单帧接口，
也不改变 ADC 实际采样率和算法输入数据。

参考值（仅在连续 ADC 发布链路启用后生效）：

```text
10000：低负担调试
20000：默认推荐
30000：较流畅显示
50000：高负载验证，不作为默认值
```

---

### 3.4 FFT 或其他一维分析结果

当前 `signal_separation` 单帧固件明确配置：

```c
#define APP_OUTPUT_ENABLE_FFT_WAVE 0U
```

因此 `AppOutput_RequestMode(APP_OUTPUT_MODE_FFT_WAVE)` 会返回 `false`，
`FftTask` 也未加入目标 CMake。示波器下拉框中的 FFT 是 DisplayTask 对同一帧
连续 128 点执行的板内 UI FFT，只更新屏幕，不调用下面的串口 FFT 发布接口。

下面接口为其他工程或以后重新评审输出协议时保留；当前固件不会调用：

```c
int32_t fft_db_q100[2048];

AppOutput_RequestMode(APP_OUTPUT_MODE_FFT_WAVE);

if (AppOutput_GetMode() == APP_OUTPUT_MODE_FFT_WAVE)
{
    bool accepted = AppOutput_PublishFftBins(
        fft_db_q100,
        fft_bin_count,
        start_bin,
        actual_sample_rate_hz,
        fft_size,
        APP_OUTPUT_FREQUENCY_UNIT_KHZ
    );
}
```

推荐将 dB 乘 100 后发送：

```text
-42.37 dB -> -4237
```

发布失败表示当前模式不正确或输出队列没有足够空间。算法任务不得因此阻塞 ADC 任务。

---

## 4. 禁止调用清单

调度器启动后，除 `app_output_task.c` 和 `debug_io.c` 外，业务文件中禁止出现：

```c
HAL_UART_Transmit(...)
HAL_UART_Transmit_DMA(...)
vcp_write(...)
vcp_printf(...)
printf(...)
```

例外：系统调度器启动前的极短启动信息，但不建议继续扩展。

可使用下面的命令检查：

```powershell
Get-ChildItem APP,BSP,UI,Core/Src -Recurse -File -Include *.c,*.h |
    Select-String -Pattern 'HAL_UART_Transmit|vcp_write|vcp_printf|printf\('
```

检查结果中，底层传输文件之外出现这些调用时，应优先改为 `AppOutput_*` 接口。

---

## 5. 输出状态监控

```c
AppOutputStats stats;
AppOutput_GetStats(&stats);
```

关键字段：

```text
dropped_log_messages    日志队列已满
dropped_stream_packets  波形输出包被丢弃
transport_retries       VCP 环形缓冲暂时已满，等待重试
transport_errors        传输等待超时或底层错误
```

底层 VCP 丢弃字节：

```c
DebugIO_GetVcpDroppedBytes()
```

正常要求：

```text
transport_errors = 0
DebugIO_GetVcpDroppedBytes() 不持续增长
```

`dropped_stream_packets` 增长不会破坏 ADC 原始采集，但会导致 VOFA+ 实时显示出现断裂或相位跳变。

---

## 6. 50 kpoints/s 毛刺排查

当前 FireWater 使用十进制文本，每个 12 位 ADC 点通常需要：

```text
4 个数字 + 1 个换行 ≈ 5 字节
```

50 kpoints/s 约产生：

```text
50,000 × 5 ≈ 250 kB/s
```

10 Mbaud、8N1 的理论有效载荷约为 1 MB/s，因此纯带宽仍有余量；但实际还受到以下因素影响：

- ST-LINK VCP 在高波特率下的稳定性；
- USB 驱动和 VOFA+ 解析；
- OutputTask 的整数转文本开销；
- 4 KiB VCP DMA 环形缓冲；
- 17 个可用波形包的上层队列；
- ADC 帧以突发方式提交；
- LVGL 和其他高优先级任务调度。

截图中的 ADC 代码理论上被限制在 `0~4095`。如果 VOFA+ 自动纵轴曾被拉到远高于 4095，说明至少出现过一次文本拼接、换行丢失、旧数据残留或解析异常，而不是 ADC 真正输出了该数值。

### 快速判定步骤

1. 依次测试 20k、30k、50k 点率，每档运行 2 分钟。
2. 从 ADC_WAVE 切回 LOG 后读取：
   - `dropped_stream_packets`
   - `transport_retries`
   - `transport_errors`
   - `DebugIO_GetVcpDroppedBytes()`
3. 如果只有 50k 时计数持续增加，属于输出链路压力。
4. 用 Snapshot 或调试器查看同一时段的原始 DMA 帧：
   - 原始帧平滑、VOFA+ 有毛刺：输出或解析问题；
   - 原始帧也有毛刺：继续检查 ADC 时序、数据总线和模拟前端。
5. 清空 VOFA+ 历史数据并关闭自动量程后再复测。
6. 在 10 Mbaud 不稳定时，测试 6 Mbaud 或 8 Mbaud；50k 文本流仍有带宽余量。

### 当前建议

日常观察使用：

```c
#define APP_OUTPUT_ADC_TARGET_POINTS_PER_SECOND 20000U
```

需要更流畅时使用：

```c
#define APP_OUTPUT_ADC_TARGET_POINTS_PER_SECOND 30000U
```

50k 用于压力验证，不建议作为默认配置。若未来必须长期稳定输出 50k 以上，再增加独立的二进制波形模式；不通过修改 `debug_io.c` 来解决业务协议问题。

---

## 7. 新功能接入决策

新增输出需求时按以下顺序处理：

1. 只是日志：调用 `AppOutput_Logf()`。
2. 是 ADC 时域数据：复用 `ADC_WAVE`。
3. 是 FFT 或一组分析结果：复用 `FFT_WAVE`。
4. 与现有格式确实不兼容：评估新增一个 `AppOutputMode`。
5. 只有底层传输本身出现已复现缺陷，才修改 `debug_io.c/h`。

原则：先扩展上层输出管理，不轻易修改串口 DMA 传输层。
