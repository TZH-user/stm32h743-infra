# STM32H743 ↔ ZYNQ-7020 PL 双 SPI 链路规范 v0.3

> 状态：页面格式、协议状态机、standalone RTL 接口和当前载板 PL 引脚冻结；JTAG 下载及板间双链路已通过，12/6.25 MHz 为当前杜邦线稳定基线。  
> 日期：2026-07-23。  
> 本文取代 v0.2 的 ACK、序号、credit、重传和 SPI-B 保护间隔语义。  
> v0.2 中已经验证的双 SPI 物理结构、1024-byte BRAM 页面和 CDC 架构继续有效。

## 1. 目标与范围

本链路用于 STM32H743 与 ZYNQ-7020 PL 之间的短距离板间高吞吐通信：

- SPI-A：STM32 SPI1 master，PL slave。
- SPI-B：PL master，STM32 SPI6 slave（BDMA）。
- 两条链路均为 Mode 0、8-bit、MSB-first、固定 1024 byte 全双工交换。
- 每条链路有独立 READY、独立序号空间、独立 ACK/result 和独立 0/1 credit。
- DMA/BRAM callback 只转交完整页面；CRC、TLV 和状态机在任务/用户逻辑上下文执行。

当前载板的 ZYNQ PL 引脚、`LVCMOS33` 和 Bank13 已冻结，见第 2 节。
以下内容仍未冻结：

- 最终 PCB 走线、发送端串联电阻值和高频实板信号完整性结论。
- SPI-B 当前 PL 分频为 4，即 6.25 MHz；25 MHz/50 MHz 仅在硬件整改后资格测试。
- SPI-A 当前 STM32 固件使用 192 MHz kernel clock、`/16`，即 12 MHz；
  24 MHz/48 MHz 仅在 PB3 串联 22–33 Ω、缩短连线并完成实板扫频后启用。
- DDS、ADC、算法结果等业务 TLV body。
- 100 MHz SPI-B 不属于 v0.3 上板目标。

当前稳定基线理论吞吐（不计 READY、任务调度和重传间隔）：

| 链路 | SCLK | 原始吞吐 | 988-byte page payload 吞吐 |
|---|---:|---:|---:|
| SPI-A | 12 MHz | 1.500 MB/s | 1.447 MB/s |
| SPI-B | 6.25 MHz | 0.781 MB/s | 0.754 MB/s |

固定页 envelope 效率为 `988/1024 = 96.48%`；单条最大 TLV body 为 980 byte。
两条链路同时持续运行时，当前理论 payload 聚合约 2.20 MB/s。受 READY、页面处理和
Debug 固件调度影响，2026-07-23 实测固定页聚合线速率约 10.17 Mbit/s。

## 2. 固定物理接口

```text
STM32                                         ZYNQ-7020 PL

PB3 / SPI1 SCK   ----------------------------> SPI_A SCLK
PB5 / SPI1 MOSI  ----------------------------> SPI_A MOSI
PB4 / SPI1 MISO  <---------------------------- SPI_A MISO
PB6 / GPIO CS_A  ----------------------------> SPI_A CS_N
PB7 / EXTI7      <---------------------------- SPI_A READY

PG13 / SPI6 SCK  <---------------------------- SPI_B SCLK
PG14 / SPI6 MOSI <---------------------------- SPI_B MOSI
PG12 / SPI6 MISO ----------------------------> SPI_B MISO
PE2 / EXTI2      <---------------------------- SPI_B CS_N
PE6 / READY_B    ----------------------------> SPI_B PEER_READY
```

当前载板 Bank13 接线契约：

| 链路信号 | STM32H743 | ZYNQ-7020 PL | Bank13 排针 |
|---|---|---|---:|
| SPI-A SCLK | PB3 / SPI1_SCK | U7 / `spi_a_sclk` | 10 |
| SPI-A CS_N | PB6 / GPIO | V7 / `spi_a_cs_n` | 9 |
| SPI-A MOSI | PB5 / SPI1_MOSI | U10 / `spi_a_mosi` | 11 |
| SPI-A MISO | PB4 / SPI1_MISO | T9 / `spi_a_miso` | 12 |
| SPI-A READY | PB7 / EXTI7 | Y6 / `spi_a_ready` | 13 |
| SPI-B SCLK | PG13 / SPI6_SCK | Y7 / `spi_b_sclk` | 14 |
| SPI-B CS_N | PE2 / EXTI2，软件 NSS 事务监督 | Y8 / `spi_b_cs_n` | 17 |
| SPI-B MOSI | PG14 / SPI6_MOSI | Y9 / `spi_b_mosi` | 18 |
| SPI-B MISO | PG12 / SPI6_MISO | W8 / `spi_b_miso` | 19 |
| SPI-B PEER_READY | PE6 / GPIO | V8 / `spi_b_peer_ready` | 20 |
| GND | GND | GND | 15、16 |

`VCCO_13=3.3 V`，所有十根 PL 信号均使用 `LVCMOS33`。当前 PL 输出均为
`SLEW SLOW`；SPI-B SCLK/Y7 为 `DRIVE 4`，其他 PL 输出为 `DRIVE 8`。
首次飞线联调时，在每根高速输出的发送端预留或串联 22–33 Ω，并保持 SCLK、数据、
CS 和 READY 走线短且有连续地参考；至少连接两根就近 GND。SPI-A 提频时，电阻应优先
放在 PB3 发送端并靠近 STM32。

READY 是从机输出的持续电平，含义始终为：

> 从机已经同时冻结一个 TX page，并保留一个可写 RX page，允许主机开始一次
> 精确 1024-byte 的全双工事务。

READY 只负责当前物理事务准入，不等同于协议 credit。

## 3. 固定页面

```text
EXCHANGE_PAGE_BYTES = 1024
HEADER_BYTES        = 32
PAYLOAD_BYTES_MAX   = 988
PAGE_CRC_BYTES      = 4
```

所有多字节字段均为 little-endian，并按偏移显式读写。

| Offset | Size | 字段 | v0.3 语义 |
|---:|---:|---|---|
| 0 | 4 | `magic` | ASCII `"DSP1"`，LE 值 `0x31505344` |
| 4 | 1 | `version_major` | 固定为 1 |
| 5 | 1 | `header_bytes` | 固定为 32 |
| 6 | 1 | `link_id` | 0=SPI-A，1=SPI-B，必须与实际物理链路一致 |
| 7 | 1 | `page_flags` | 见第 4 节 |
| 8 | 4 | `tx_sequence` | 仅在 `TX_SEQUENCE_VALID=1` 时有效 |
| 12 | 4 | `rx_result_sequence` | 仅在 `RX_RESULT_VALID=1` 时有效 |
| 16 | 2 | `payload_bytes` | 0..988，必须等于 TLV 编码总长度 |
| 18 | 2 | `message_count` | TLV 数量，必须与解析结果精确一致 |
| 20 | 4 | `sender_time_us` | 发送端微秒计数，允许自然回绕 |
| 24 | 2 | `status_code` | 对 `rx_result_sequence` 的 ACK/NACK 结果 |
| 26 | 2 | `rx_credit_pages` | 只能为 0 或 1 |
| 28 | 4 | `header_crc32c` | 覆盖 byte 0..27 |
| 32 | 988 | `payload` | TLV 区；有效区后至 byte 1019 必须全 0 |
| 1020 | 4 | `page_crc32c` | 覆盖 byte 0..1019 |

字段 12 从 v0.2 的 `rx_ack_sequence` 改名为 `rx_result_sequence`，线宽和偏移不变。
有效性不再依赖特殊序号，而由 `RX_RESULT_VALID` 明确表达。

## 4. `page_flags`

| Bit | 名称 | 规则 |
|---:|---|---|
| 0 | `ACK_ONLY` | payload 必须为空，`message_count=0` |
| 1 | `RETRANSMIT` | 重发当前 outstanding 序号 |
| 2 | `RESET_EPOCH` | 复位同步页，固定 `tx_sequence=0` |
| 3 | `ERROR` | `RX_RESULT_VALID=1` 且 `status_code!=0` 时必须为 1 |
| 4 | `MORE_FRAGMENTS` | v0.3 必须为 0 |
| 5 | `TX_SEQUENCE_VALID` | 本页参与序号、ACK 和 outstanding |
| 6 | `RX_RESULT_VALID` | `rx_result_sequence/status_code` 有效 |
| 7 | Reserved | 发送为 0；v0.3 接收端忽略 |

合法组合：

| 页面类型 | `ACK_ONLY` | `TX_SEQUENCE_VALID` | `RESET_EPOCH` | 是否消耗 credit |
|---|---:|---:|---:|---:|
| 普通 ACK/credit/heartbeat | 1 | 0 | 0 | 否 |
| 新数据页 | 0 | 1 | 0 | 是 |
| 数据重传页 | 0 | 1 | 0，且 `RETRANSMIT=1` | 否 |
| RESET/SYNC 首发或重传 | 1 | 1 | 1 | 否 |

普通 ACK_ONLY 不递增序号、不占 outstanding、不要求 ACK、不重传，从而避免
ACK-of-ACK 循环。

## 5. CRC-32C

```text
reflected polynomial = 0x82F63B78
init                 = 0xFFFFFFFF
xorout               = 0xFFFFFFFF
CRC 字段字节序        = little-endian
```

标准向量：

```text
"123456789" -> 0xE3069283
```

接收检查顺序：

1. 页面物理长度。
2. magic、version、header length。
3. Header CRC。
4. Page CRC。
5. payload length。
6. TLV 边界、数量、padding 和 payload 尾部零填充。
7. link_id、flags、credit、序号和 result。

页面未通过完整边界检查前，不得执行任何有副作用的 TLV。
Page CRC 必须先于可回 NACK 的协议字段错误完成确认，避免从损坏页面中
提取 `tx_sequence` 并生成错误 result。

## 6. TLV

每条 TLV：

| Offset | Size | 字段 |
|---:|---:|---|
| 0 | 2 | `type` |
| 2 | 2 | `flags` |
| 4 | 2 | `length` |
| 6 | 2 | `tag` |
| 8 | N | `body` |

规则：

- 总长度按 4 byte 向上对齐。
- padding 必须为 0。
- `payload_bytes` 必须精确等于全部 TLV 头、body 和 padding 之和。
- 解析结束偏移必须等于 `payload_bytes`。
- 解析数量必须等于 `message_count`。
- 未知 `type` 在边界合法时跳过。
- `type=0xFFFF` 永久保留：禁止发送，接收时返回 `BAD_TLV`。
- v0.3 的通用 TLV `flags` 固定为 0。
- `tag` 仅作为请求/响应关联号，不作为分片号。
- v0.3 不启用通用分片，因此 `MORE_FRAGMENTS=0`。

## 7. RX 序号状态机

复位初值：

```text
synchronized       = false
expected_rx_sequence = 0
```

RESET/SYNC：

- 页面必须为 `ACK_ONLY=1`、`RESET_EPOCH=1`、`TX_SEQUENCE_VALID=1`。
- `tx_sequence` 固定为 0。
- 首次接收后清理旧 epoch，设置 `expected_rx_sequence=1`。
- 同一 RESET 的重复页只重新返回 result，不重复执行任何动作。

普通 sequenced 页按模 `2^32` 比较：

```text
seq == expected       -> 新页；原子接纳后 expected++
seq == expected - 1   -> 重复页；不再次分发 payload
其他                  -> SEQUENCE_GAP
```

只有 RESET/SYNC 完成后才接收普通 sequenced 数据页。普通 ACK_ONLY 页面不改变
`expected_rx_sequence`。

## 8. ACK/result

`RX_RESULT_VALID=1` 时：

- `rx_result_sequence` 指向被确认或拒绝的 sequenced 页。
- `status_code=0` 且 `ERROR=0` 表示 ACK。
- `status_code!=0` 且 `ERROR=1` 表示 NACK。
- ACK 的含义是“页面已完整验证，并原子接纳到协议分发队列”。
- ACK 不表示 DDS/ADC/算法业务已经执行完成；业务完成必须使用响应 TLV。

接收重复页时仍处理其 result 和 credit 字段，但不得再次分发 payload。
只有与当前 outstanding 序号匹配的 ACK 才能释放重传映像。

建议动作：

| `status_code` | 发送方动作 |
|---|---|
| `OK` | 释放 matching outstanding |
| `NO_RX_CREDIT` | 保留 outstanding，等待后续 credit |
| `SEQUENCE_GAP` | 进入 RESET/SYNC |
| `LINK_TIMEOUT` | 保留相同序号并重传 |
| `BAD_PAYLOAD_LENGTH/BAD_TLV/BAD_FLAGS/BAD_LINK_ID/BAD_CREDIT` | 记录协议故障并进入 RESET/SYNC |
| 其他非零值 | 保留 outstanding，上报上层决定 |

只有页面物理长度、magic、version、header length、Header CRC 和 Page CRC
全部通过后，接收端才允许使用头部中的 `tx_sequence` 为
`BAD_PAYLOAD_LENGTH/BAD_TLV/BAD_FLAGS/BAD_LINK_ID/BAD_CREDIT` 建立 NACK。
Header CRC、Page CRC 或此前基础字段错误不得产生可信 result；发送方通过超时重传。

## 9. Credit

`rx_credit_pages` 表示：

> 本页发送方承诺为对端保留一个未来新 sequenced 数据页的 RX 槽。

规则：

- credit 只作用于后续事务，不授权当前全双工事务中的对向页面。
- 值只能为 0 或 1。
- `1` 是保留槽承诺，授予后必须保留到新数据页被接纳或 RESET。
- 新数据页消耗 credit。
- 重传、普通 ACK_ONLY 和 RESET/SYNC 不消耗 credit。
- 重复收到 `credit=1` 只覆盖状态，不累加。
- READY 不能替代 credit。

## 10. TX 页面所有权

物理事务完成不等于协议 ACK。

STM32 和 PL 用户逻辑均按下列状态管理 sequenced TX page：

```text
FREE
  -> BUILDING
  -> OUTSTANDING
  -> IN_FLIGHT
  -> OUTSTANDING   (等待 ACK、超时或需要重传)
  -> ACKED
  -> FREE
```

规则：

- sequenced 页的完整重传映像保持不可覆盖，直到 matching ACK、明确终止或 RESET。
- 重传时固定不变：`tx_sequence`、`payload_bytes`、`message_count` 和 payload。
- 允许刷新：`RETRANSMIT`、result、credit、status、time 和两个 CRC。
- DMA complete 只解除 `IN_FLIGHT`，不能直接把 buffer 变为 FREE。
- 普通未排序 ACK_ONLY 可以使用独立控制页 buffer，不占 sequenced outstanding。

## 11. READY、CS/NSS 与保护间隔

### SPI-A

- PB7 使用上升沿 EXTI7 唤醒 STM32 通信任务。
- EXTI 只通知“可能 READY”；任务启动 DMA 前仍必须再次读取 PB7 电平。
- READY 高到 CS_A 低：至少 100 ns。
- CS_A 低到首个 SCLK 上升沿：至少 100 ns。
- 最后一个 SCLK 上升沿到 CS_A 高：至少 100 ns。
- 下一页必须重新观察 READY 低到高。

### SPI-B

在 200 MHz `link_clk` 下，PL master 必须满足：

```text
CS_SETUP_CYCLES   >= 20
CS_HOLD_CYCLES    >= 20
INTERFRAME_CYCLES >= 20
```

即每项至少 100 ns。START 接受后，STM32 的 `B_READY` 必须撤低，并保持到：

- 旧事务结束；
- RX 页面已交付或废弃；
- 新 TX/RX DMA 全部重新挂好。

NSS 提前拉高、BDMA 未完成、OVR 或 PL 复位时，STM32 必须撤低 B_READY、废弃
部分 RX、保留未确认 TX、Abort/Reinit SPI6 + BDMA，并等待 NSS 高后重新武装。

standalone SPI-B RTL 已将三项 guard 参数的默认值和最小值固定为 20，并已通过
独立回归门禁。PE2 是唯一的 `SPI_B_NSS` 事务监督输入，配置为双边沿 EXTI2：
SPI6 RX/TX BDMA 必须先挂好，随后 STM32 才拉高 `B_READY`；PE2 下降沿立即撤低
`B_READY` 并启动 `IN_FLIGHT` 监督，PE2 上升沿检查 EOT 与两个 BDMA 剩余量。
提前上升沿会废弃半页，并在任务上下文 Abort/DeInit/Init SPI6 后重新武装。

## 12. STM32 软件边界

```text
Core/App/DualSpi/
  dual_spi_protocol.h/.c
  dual_spi_session.h/.c
  dual_spi_transport.h/.c
  dual_spi_stm32_port.h/.c
  dual_spi_task.h/.c
```

`dual_spi_protocol`：

- 不依赖 HAL 或 FreeRTOS。
- 实现 CRC、Page、TLV、RX sequence、TX outstanding/result/credit。
- 可由 Windows 主机测试直接编译。

`dual_spi_transport`：

- 是不依赖 HAL/FreeRTOS 的物理事务状态机，管理页面所有权、错误、超时和恢复。
- 对 DMA 启动失败、重复/迟到 callback、部分页废弃和 Abort/Reinit 建模。

`dual_spi_session`：

- 管理每条链路的 RESET/SYNC、序号、result、credit 和 outstanding 重传映像。
- 将任意二进制应用数据编码为 TLV；同一页面可合并多条 TLV。
- 新数据页只分发一次，重复页只重发 result，不重复执行应用回调。

`dual_spi_stm32_port`：

- 在 `RAM_D2/.dma_buffer` 中管理 SPI-A 两个 1024-byte 页面，在
  `RAM_D3/.bdma_buffer` 中管理 SPI-B 两个 1024-byte 页面，均为 32-byte 对齐。
- 管理 PB7 EXTI7、PB6 CS、PE6 READY、PE2 EXTI2、SPI1 DMA 与 SPI6 BDMA，
  并执行所需 cache clean/invalidate。
- callback 只撤销物理握手、保存状态并通知 `DualSpiTask`，不解析 CRC/TLV。

`DualSpiTask`：

- 初始化两条链路并发送 RESET/SYNC。
- 解析完整页面，将有效页面原子接纳到每条链路各自的 RX Page Mailbox。
- 管理超时、重传、credit 和错误统计。
- 由 `.ioc` 生成，优先级为 `osPriorityHigh`，栈为 2048 byte。

STM32 每条链路有一个协议 RX Page Mailbox：

- `dual_spi_session_receive_page()` 先完成 CRC、TLV、link、flags、credit 和序号检查。
- 只有整页 payload 已成功复制进空闲 mailbox 后，才提交新的 RX 序号并建立
  `status=OK` 的 result；这就是 ACK 的“原子接纳”边界。
- 页面一旦进入 mailbox，物理 DMA RX page 即可释放；应用处理慢不会长期占用 DMA buffer。
- mailbox 被占用时，`dual_spi_session_rx_credit_pages()` 必须返回 0；释放后才返回 1。
- mailbox 已满时不得推进接收序号，也不得发送成功 ACK；发送端保留 outstanding 页面。

应用发送接口：

```c
DualSpi_Send(link_id, type, tag, body, body_bytes, timeout_ms);
```

应用整页接收接口：

```c
bool DualSpi_OnPage(dual_spi_link_id_t link_id,
                    const dual_spi_page_view_t *page);
```

`DualSpi_OnPage()` 在 `DualSpiTask` 上下文中接收已经验证且由 RX Page Mailbox
持有的完整页面。只有应用已经原子接纳或处理完整页时才返回 `true`；此时 session
释放 mailbox，并在后续发送页中重新广告 `credit=1`。返回 `false` 时页面继续保留、
credit 保持为 0，并在后续任务循环中再次提供同一页面。弱默认实现返回 `false`，
防止未实现业务回调时静默丢页。

`body` 是最多 980 byte 的任意二进制数据。应用可在 `DualSpi_OnPage()` 中遍历
页面内的一条或多条 TLV，并应尽快把数据转交给 DDS/ADC/算法任务，不应执行长时间
阻塞操作。body 内部多字节值仍须由业务 TLV 明确字节序和量纲。
`DualSpi_Send == SEND_OK` 只表示已复制进本地邮箱，不表示对端已 ACK 或业务已执行；
需要端到端完成语义时，必须让对端返回使用相同 `tag` 的响应 TLV。

ACK 只表示“页面已经进入对端协议 mailbox”，不表示
`DualSpi_OnPage()` 已返回 `true`，更不表示 DDS/ADC/算法业务已经执行完成。

## 13. PL validator 与 RX 页面所有权

SPI-A slave 和 SPI-B master 的 PHY RX 接口统一为：

```text
rx_frame_valid
rx_rd_en + rx_rd_addr
rx_rd_valid + rx_rd_data
rx_release
```

读取规则：

- consumer 只在 `rx_frame_valid=1` 时发起读取。
- `rx_rd_en/rx_rd_addr` 是请求，返回数据只能在 `rx_rd_valid=1` 时采样。
- 不允许假定 BRAM 固定为单周期返回；当前 RTL 在 BRAM 输出后保留
  `rx_rd_valid_pipe_q` 和 `rx_rd_data_pipe_q` 两组 FF 流水寄存器。
- 两组流水寄存器使用 `keep` 与 `dont_touch`，防止综合器重新形成
  `RAMB18E1 -> validator` 直通长路径。

`dual_spi_page_validator` 的所有权规则：

- `start` 只能在 PHY 页面冻结且 `start_ready=1` 时脉冲。
- `result_valid` 从验证完成保持到 `result_release`；`page_valid=1` 仅表示完整有效页。
- 有效页的第二遍业务读取使用
  `page_rd_en/page_rd_addr/page_rd_ready/page_rd_valid/page_rd_data`。
- `page_rd_en` 与 `result_release` 同周期被接受时，读取优先完成，release 被挂起；
  `page_rd_valid` 返回后才真正释放结果，避免 read/release 竞态丢最后一个字节。
- 上层 session/dispatch mailbox 完成复制或消费前，不得产生 PHY `rx_release`。
- 每条链路的真实 0/1 credit 必须来自上层 mailbox 是否空闲，而不是来自 READY。

## 14. v0.3 验证状态

截至 2026-07-23：

- Vivado 2023.1 下，`dual_spi_link` 的 16 项 fixed-frame 行为测试通过。
- `dual_spi_page_validator` 的 21 页协议测试通过，包含可变 PHY 读延迟、
  整页二次读取，以及读取请求与 `result_release` 同周期的竞态回归。
- SPI-A 已仿真连续交换两页 1024 byte、64.5 MHz；SPI-B 已仿真连续交换两页
  1024 byte、50 MHz，并通过 20-cycle CS setup/hold/interframe guard 门禁。
- 集成 PHY→validator 的 200 MHz 综合后门禁为：全设计、SPI-A 和 SPI-B
  最差 setup slack 均为 `+0.102 ns`。
- 保留 post-BRAM 流水后，PHY 数据路径的 startpoint 为 `FD*`，
  不再允许 `RAMB18E1` 直接驱动 validator；两路均无 latch。
- 同一 harness 的 Vivado 2023.1 out-of-context 实现后门禁已执行
  `opt_design -> place_design -> phys_opt_design -> route_design`：
  全设计 `WNS=+0.230 ns`、`WHS=+0.049 ns`，SPI-A PHY→validator
  `WNS=+0.269 ns`，SPI-B PHY→validator `WNS=+0.367 ns`；
  2009 条网络 fully routed，0 条 routing error，0 个未约束内部 endpoint。
- 上述实现结果仅是内部 OOC 物理路径门禁。clock port 尚无最终 `HD.CLK_SRC`，
  也没有板级 input/output delay；不能替代最终 BD、引脚和 XDC 的整机签核。
- OOC DRC 当前有 22 个 warning：20 个 `REQP-1840` 指向 SPI-A 异步
  CS/reset 驱动 BRAM 地址寄存器，另有 report-limit 与无 PS7 顶层 warning。
  最终集成前必须完成同步复位改造或给出可审查的 write-enable 关闭证明与 waiver。
- STM32 `.ioc` 已冻结为 PB7 `GPXTI7` 上升沿、PE2 `GPXTI2` 双边沿，
  两个 EXTI 优先级均为 5；SPI-B 使用 SPI6 + BDMA Channel0/1。
- `.ioc` 已加入 `DualSpiTask`；CubeMX 生成、STM32CLI ioc-check、configure
  和不烧录固件构建通过。
- CRC-32C、Page、TLV、RESET、重复页、缺口、回绕、result 和 credit
  的 protocol/session/transport 三组 Windows 主机测试全部通过。
- transport 主机测试覆盖两条链路正常流程、启动失败、DMA/SPI 错误、超时、
  迟到 callback 和恢复；session 主机测试覆盖 RESET 重传、任意二进制 TLV、
  exactly-once 分发、sequence gap、CRC 可信协议错误 NACK 和 CRC 错误不回 NACK。
- `dual_spi_protocol/session/transport` 已通过 Windows GCC 与 ARM GCC 编译。
- ARM 链接结果：`RAM_D2=10048 B`、`RAM_D3=2048 B`，其中 SPI-B 两个页面
  位于 SRAM4；`DTCMRAM=32848 B`，`FLASH=99424 B`。
- Vivado 2018.3 已安装到 `F:\vivado2018\Vivado\2018.3`，其 `bin` 目录已写入
  Machine PATH 首项。本轮用 2018.3 重跑 fixed-frame、page validator、
  RESET/`PL_SPI_LINK_OK` bring-up endpoint、SPI-A READY、SPI-B guard-cycle、
  SPI-A 64.5 MHz 以及 SPI-B 50 MHz 共七项自检，全部通过。
- 当前整机顶层为 `system_bd_spi_wrapper`。独立实现 run
   `impl_dual_spi_bringup` 已完成，18112 条 routable nets 全部完成路由，
  routing error、critical warning 和 error 均为 0；当前实现 Setup WNS=`+1.546 ns`，
  Hold WHS=`+0.032 ns`。
- 当前载板 PL XDC 已固定为 Bank13 的 U7/V7/U10/T9/Y6/Y7/Y8/Y9/W8/V8，
  全部为 `LVCMOS33`；U7 为 clock-capable pin，SPI-A SCLK 在实现中经
  `IBUF -> BUFG` 驱动 56 个时钟负载。
- 上板联调文件已生成：
  `AD9226.runs/impl_dual_spi_bringup/system_bd_spi_wrapper.bit` 和
  `AD9226.runs/impl_dual_spi_bringup/system_bd_spi_wrapper.ltx`。
- JTAG 已枚举为 FTDI `VID_0403&PID_6014`。Vivado 2018.3 已找到唯一
  `xc7z020` 并成功下载正式 bitstream；现有 ILA 诊断也能发现 debug hub/ILA，
  证明 PL 配置和系统时钟均在线。
- 板间接线已按 PB3/PB4/PB5/PB6/PB7 与 PG13/PG12/PG14/PE2/PE6 映射修正，
  双链路 READY、CS、DMA/BDMA 和 1024-byte 页面交换均已实际跑通。
- 当前正式 bitstream 对 SPI-A CS_N 使用三采样数字滤波，滤除约 60–80 ns 毛刺；
  SPI-B 实际 SCLK 为 `50 MHz / (2×4) = 6.25 MHz`。bitstream SHA-256 为
  `49A4D11FEEBE2B6FE0B79B3158EEC5C1E49D625B64F13B4A555AAC349F8C7752`。
- 6 MHz SPI-A、6.25 MHz SPI-B 低速基线曾交换 A=21904、B=21900 页，零错误。
- 24 MHz SPI-A + PB3 `HIGH` 会强烈串扰 SPI-B：B=10267 有效页、2343 错误；
  PB3 `MEDIUM` 时 B 恢复零错误，但 SPI-A 约 211381 有效页中仍有 9 个偶发错误。
  PB3 `LOW` 在 24 MHz 下会因边沿过慢产生约 0.81% 的页头 CRC 错误。
- 当前冻结为 SPI-A 12 MHz + PB3 `LOW`、SPI-B 6.25 MHz。30 秒首轮后继续观察
  120 秒，最终 A=`valid=125159 err=0`、B=`valid=83448 err=0`；SPI/DMA、timeout、
  early-NSS、recovery、CRC、duplicate 和 gap 计数全部为 0。
- 证据保存在 STM32 artifacts：
  `20260723-163255-998-flash-and-observe` 和 `20260723-163347-141-serial`。

尚未完成：

- SPI-A `REQP-1840` reset/CS 到 BRAM 地址路径的整改或正式 waiver。
- 在 PB3 发送端增加 22–33 Ω、缩短飞线并补足就近 GND 后，重新执行
  SPI-A 24/48 MHz 与 SPI-B 25/50 MHz 的示波器检查、扫频和长时间资格测试。
- CRC/长度/提前 NSS/READY 抖动/复位错误注入，以及多小时双向满载测试。

## 15. 禁止

- 普通 ACK_ONLY 占用序号或要求 ACK。
- 使用特殊 ACK 序号表示“无 ACK”。
- credit 累加到 2。
- 物理 DMA complete 后立即覆盖 sequenced TX buffer。
- 在 RX Page Mailbox 尚未释放时广告 `credit=1`。
- 把 `DualSpi_OnPage()` 返回 `false` 的页面当作已完成业务处理。
- READY=0 时强行开始事务。
- callback 中解析 CRC/TLV。
- 直接发送编译器布局的 C/SystemVerilog struct。
- 使用 4-bit SPI 数据宽度。
- 用普通 PL GPIO 承载高速 SPI-A SCLK 后关闭 dedicated-route DRC。


