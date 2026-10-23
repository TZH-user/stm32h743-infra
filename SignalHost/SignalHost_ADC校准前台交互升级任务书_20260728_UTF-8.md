# SignalHost ADC 校准前台交互升级任务书（UTF-8）

日期：2026-07-28
目标工程：`F:\STM32\SignalHost`
关联固件：`F:\STM32\stm32project\VSCodeCMAKEProject\signal_separation`

## 1. 任务结论

- **有依据**：现有 SignalHost 不是空白工程，已经具备 tkinter 校准工作台、
  Serial Fake、真实串口双门禁、手动逐点采集、线性拟合、Profile 生命周期、
  会话导出、日志和 50 项自动测试。
- **有依据**：现有前台可以手动完成板端校准命令流程，但不能在一个前台内自动编排
  DG4062、DS4014E、42 点目标序列、闭环参考测量和最终质量复核。
- **有依据**：可以升级为“前台设置、人工确认启动、前台看实时进度”的完整工装。
  实施时必须保留真实硬件门禁、仪器 prepare/confirm/apply、失败即停和
  `IMPORT/APPLY` 人工确认，不能把自动化等同于无条件全自动写入。
- **当前任务边界**：本文件只定义软件开发任务，不授权本轮修改源码、打开 COM3、
  控制仪器、烧录固件或写入校准 Profile。

## 2. 2026-07-28 现状测试证据

### 2.1 已通过

1. `python -m unittest discover -s tests -p "test_*.py" -v`
   - `Ran 50 tests`
   - `OK`
   - `HOST_UI_TEST_PASS`
2. 双击启动器无窗口自检：
   - `DOUBLE_CLICK_LAUNCHER_PASS`
3. 现有 parser 读取最终 HIL 日志
   `ADC校准全流程串口_20260728_203309_UTF-8.txt`：
   - `cal_ext_point=42`
   - `cal_export_begin/point/end=1/42/1`
   - EXPORT POINT CRC 错误为 0
4. 现有拟合基础模块可读取最终 42 行 CSV，且 42 行 `flags=0`。

### 2.2 当前失败

`tools\run_calibration_tests.ps1` 最终退出码为 1。50 项 Python 测试已经通过，
失败发生在 `tools\protocol_static_check.py`：

```text
missing in ...\APP\RTOS\app_serial_control_task.c:
"[CAL] STATUS state=%u
```

实际固件已经改为 `CAL STATUS v3`，格式真源位于：

`APP\Calibration\app_calibration_serial_protocol.h`

```text
[CAL] STATUS v=3 state=... error=... exclusive=... can_start=...
[CAL] SESSION worker_active=... reason=... points=.../...
[CAL] SELECT mode=... source=... item=... candidate=...
[CAL] PROFILE configured=... active=... pending=...
[CAL] PROFILE2 previous=... undo_available=... enabled=... in_use=... applied=...
[CAL] ADC valid=... seq=... dc_uv=... rms_uv=... peak_uv=...
[CAL] LAST ...
[CAL] IO ...
[CAL] HW ...
```

**结论（有依据）**：协议静态检查器仍锚定旧文件和旧文本，必须先修复，真实串口
前台升级不得绕过该失败。

### 2.3 最终 HIL 数据暴露的拟合差异

现有 Host 对全部 42 点直接拟合得到：

- gain：`-412.5297553574627 code/V`
- offset：`2119.3451616142747 code`
- R²：`0.9999988327699634`
- 最大残差：`3.288261601068882 code`

最终受控 HIL 的正式拟合只纳入 `|nominal_v| <= 4.5 V` 的 38 点，得到：

- gain：`-412.40217767274504 code/V`
- offset：`2119.451107815013 code`
- R²：`0.9999990331848746`
- 最大残差：`2.206229315303972 code`
- reference 修正后最大滞回：`2.5113488759407474 code`
- CRC32：`3D4E58A9`

**结论（有依据）**：Host 现有拟合函数可复用，但缺少“采集点”和“拟合纳入点”
分离、上下行配对滞回、rail、帧内峰峰和 reference 差等正式质量门。

## 3. 当前校准流程

### 3.1 SignalHost 当前前台流程

1. 启动 GUI，默认选择 `Serial Fake`。
2. 真实串口必须使用 `-AllowRealSerial` 启动，并由用户输入 COM 口后点击连接。
3. 执行只读 `PING/STATUS/RTOS/CAL STATUS`。
4. 手工填写参考最小/最大值、最小点数、R²、最大残差和点序策略。
5. 点击“进入独占”，发送 `CAL EXT ENTER`。
6. 点击“开始会话”，发送 `CAL EXT START <expected_points>`。
7. 用户手工输入每个参考 mV，逐点发送 `CAL EXT POINT <reference_mv>`。
8. 点击“结束采点”，发送 `CAL EXT FINISH`，板端进入 REVIEW。
9. Host 执行线性拟合，允许用户填写原因后显式排除异常点。
10. 只有拟合 PASS 才允许生成并发送 `CAL IMPORT ADC`。
11. `CAL CONFIRM` 将 candidate 写为 pending。
12. `CAL APPLY` 经两次确认后将 pending 变为 active。
13. `CAL UNDO` 经两次确认后回滚 previous profile。
14. `CAL EXPORT LATEST|sequence` 读取会话 B/P/E，显示 CRC 有效点。
15. `CAL EXT EXIT` 退出独占，并可导出 CSV/JSON/原始日志。

当前进度显示方式是“步骤状态标签 + 点表 + 原始日志”。没有统一百分比进度条，
也没有自动 42 点目标队列、仪器状态、当前 target/reference/DG offset/DS scale、
预计剩余时间和恢复状态。

### 3.2 最终 42 点 HIL 的实际流程

1. 只读确认板端、Profile、DG4062 CH1、DS4014E CH1 和错误队列。
2. 确认 DG CH1 与 DS CH1 并联到 ADC 输入节点。
3. 做 `DG +20 mV -> -1.000 V -> +20 mV` 最小阶跃，验证模拟传递。
4. DS CH1 整轮固定 `0.5 V/div`，避免 `0.2/0.5 V/div` 档间 reference 阶跃。
5. 使用 21 个目标点上行、21 个目标点下行，共 42 点：
   `-4.6, -4.5, -4.0, ..., +4.0, +4.5, +4.6`，然后反向。
6. 每个点由 DG 调整输出，DS 读取实际 reference；只有 reference 收敛后才向板端
   发送整数 mV，并等待最终 `[CAL] EXT n=...` 业务行。
7. `+/-4.6 V` 用于采集和 rail 审计；拟合仅纳入 `|nominal_v| <= 4.5 V` 的 38 点。
8. `FINISH -> EXPORT -> PC 拟合 -> 人工审计`。
9. 本轮停在 REVIEW，没有执行 `IMPORT/CONFIRM/APPLY`。
10. finally 恢复 DG CH1 安全小信号和 DS CH1 `0.2 V/div / offset 0 / AVER64`。

## 4. 当前校准项

### 4.1 固件已声明的校准项

| item | 名称 | 当前软件事实 |
| ---: | --- | --- |
| 0 | OUTPUT_SELF_TEST | 固件枚举和内部 DAC1 回环路径存在；SignalHost 未形成完整前台闭环 |
| 1 | GAIN_OFFSET | 当前外部 ADC 42 点 HIL 和 `CAL IMPORT ADC` 的正式项目 |
| 2 | FREQUENCY_RESPONSE | Profile schema 有 DAC1/DAC2 频响数组；Host 未提供端到端校准 |
| 3 | CHANNEL_MATCH | Profile schema 有幅度比和相位差；Host 未提供端到端校准 |
| 4 | SAFE_LIMITS | Profile schema 有 ADC/DAC limits；Host 未提供导入协议和闭环 |
| 5 | ADC_RANGE_ESTIMATE | 固件内部项目存在；Host 未提供完整前台流程 |

### 4.2 固件已声明的信号来源

| source | 名称 | 当前硬件/Host 状态 |
| ---: | --- | --- |
| 0 | DAC1_TO_ADC | 板端继电器回环实现存在 |
| 1 | DAC2_TO_ADC | 当前板级代码明确缺少 DAC2 回环接线依据，会拒绝 |
| 2 | EXTERNAL_INPUT | 本次 DG/DS 并联 ADC 输入的正式外部校准来源 |

### 4.3 Profile 可保存的 section

- ADC model、ADC limits
- DAC1/DAC2 model
- DAC1/DAC2 limits
- DAC1/DAC2 frequency response
- channel amplitude ratio、channel phase offset

**有依据**：当前真正贯通“PC 拟合 -> 串口导入 -> candidate/pending/active”的只有
ADC linear model。GUI 中 DAC1/DAC2 字段显示 `NOT_PROVIDED` 是正确行为，开发时
不得因为 Profile schema 已预留字段就宣称 DAC 校准已经可用。

## 5. 目标用户体验

第一屏直接进入可用的校准工作台，不制作营销页。

### 5.1 顶部连接与现场状态

- 运行模式：`Serial Fake / Replay / Real Hardware`
- COM 端口、固定 `2,000,000 baud`
- DG4062、DS4014E 连接与错误队列状态
- 当前固件 ID、CAL STATUS v3、Profile active/pending/previous
- 明确显示 `REAL HARDWARE` 和当前允许的通道，禁止静默连接

### 5.2 配置区

提供“ADC 外部 42 点正式预置”，默认值：

- 采集端点：`+/-4.6 V`
- 拟合端点：`+/-4.5 V`
- 方向：ascending + descending
- 采集点数：42
- 拟合点数：38
- DS CH1 扫点档位：`0.5 V/div`
- DS 恢复档位：`0.2 V/div`
- reference 收敛门限：`60 mV`
- reference 配对最大差：`60 mV`
- reference Vpp 门限：`100 mVpp`
- rail 门限：`minimum_code > 15` 且 `maximum_code < 4080`
- R²：不低于 `0.999900`
- 最大残差：不高于 `8 code`
- reference 修正后最大滞回：不高于 `4 code`
- 最大帧内峰峰：不高于 `16 code`

所有配置必须显示单位、来源和是否已验证。真实模式不允许直接沿用
`FAKE_DEMO_CONFIG`。

### 5.3 前台流程和进度

使用固定阶段状态机：

```text
现场检查
-> 连接设备
-> 只读快照
-> 最小阶跃
-> 进入独占
-> 开始会话
-> 预爬坡
-> 42 点采集
-> FINISH/REVIEW
-> EXPORT
-> 拟合与质量门
-> 人工审计
-> 可选 IMPORT
-> 可选 CONFIRM
-> 可选 APPLY
-> 恢复仪器
```

前台至少显示：

- 总体进度条和阶段状态
- 当前点 `n/42`、方向、名义目标、DS reference、DG offset
- ADC mean/min/max/flags/CRC
- DS scale、Vpp、reference 误差和闭环迭代次数
- 实时散点、拟合线、残差和上下行滞回图
- 已用时间、最近一步耗时、预计剩余时间
- 当前可执行动作、阻止原因和恢复状态

“暂停”只能发生在两个点之间；仪器写入、reference 读取和板端 POINT 等原子步骤
进行中不得中断。断线后禁止猜测结果。

## 6. 实现任务

### P0：协议契约和最终证据回归（阻塞项）

1. 修改 `tools\protocol_static_check.py`：
   - 从 `app_calibration_serial_protocol.h` 核对 STATUS v3 格式宏；
   - 不再要求旧状态文本位于 `app_serial_control_task.c`；
   - 核对多行 STATUS、IMPORT `int32[5]`、EXPORT B/P/E 和 UNDO。
2. 扩展 `protocol.py`，明确解析 STATUS v3 的
   CORE/SESSION/SELECT/PROFILE/PROFILE2/ADC/LAST/IO/HW 行。
3. 增加最终 HIL artifact replay fixture，忽略带时间戳的 Host 元数据行；不得把
   741 字节 `FIT {...}` Host 记录误判为设备协议损坏。
4. 增加 38 点正式拟合选择、rail、frame pp、上下行配对和
   `code_delta - gain * reference_delta` 滞回。
5. 回归必须精确得到最终复核 JSON 的数值和 CRC `3D4E58A9`。

### P1：前台手动编排和可见进度

1. 把配置、点计划、执行状态和 artifact 保存从 `gui.py` 中抽到独立模块。
2. 增加 42 点计划预览、逐点确认模式、进度条、阶段时间线和实时质量指标。
3. 保持原始点序；`nominal_v/reference_v/measured_code/min/max/flags` 分字段保存。
4. 支持从最终 CSV/JSON/串口日志打开 REVIEW，不需要重新连接或重测。
5. REVIEW 打开后默认只读，只有用户主动进入写入步骤才允许 IMPORT。

### P2：仪器联动

1. 新增 DG4062/DS4014E adapter，首个实机版本复用
   `lab-instrument-control` 结构化 CLI；禁止应用自行发送任意 raw SCPI。
2. 所有仪器写入必须执行：
   `status -> prepare -> 展示变更 -> 用户确认 -> apply -> 回读 -> SYST:ERR`。
3. 不控制 DG/DS 其他通道，不访问 E8267D。
4. DS4014E 首个超时或 `WinError 10054` 立即停止该轮，不自动重试。
5. 扫点固定 `0.5 V/div`；使用纵向 8 格信息检查波形不触顶且占屏合理。
6. finally 仅恢复本任务实际改变且通信仍健康的 CH1 状态。

### P3：真实串口和安全恢复

1. 保留进程级 `-AllowRealSerial` 和 GUI 人工连接双门禁。
2. 启动前保存板端、DG、DS 完整只读快照和本地 journal。
3. 每完成一个原子点就落盘，记录计划索引、命令、reference、测量和 CRC。
4. 断线/超时后进入 `UNKNOWN`，冻结所有 mutating 命令，只允许 STATUS。
5. 固件 STATUS v3 当前未提供足够的会话唯一身份时，不允许静默自动续跑。
   仅当 journal、点数、source/item 和人工确认一致时提供“从下一点继续”；
   否则要求安全退出或由固件后续增加 session sequence 后再开放确定性恢复。

### P4：Profile 生命周期

1. 质量门 PASS 只生成 candidate 预览，不自动发送 IMPORT。
2. IMPORT、CONFIRM、APPLY 分为三个独立按钮和三个独立审计事件。
3. APPLY 和 UNDO 保留两次确认；CLEAR 不在默认流程中。
4. APPLY 后必须只读确认 active sequence、enabled，并在真实业务帧确认 `in_use=1`。
5. 未验证第二 Profile、UNDO、掉电重启前，不把完整生命周期标为 PASS。

## 7. 建议模块边界

优先复用现有模块，不重建工程：

- `protocol.py`：只负责串口行解析和字段校验
- `fit.py`：线性拟合及正式 ADC 质量报告
- `session.py`：板端命令状态机和 Profile 生命周期
- `transport.py`：Fake/Replay/Serial
- 新增 `workflow.py`：跨串口和仪器的前台阶段状态机
- 新增 `instrument_adapter.py`：结构化仪器 CLI 适配
- 新增 `calibration_plan.py`：42 点计划、配置和校验
- 新增 `artifacts.py`：journal、CSV/JSON、复核报告和哈希
- `gui.py`：仅绑定状态、渲染和收集用户确认

实施复杂调用链修改前必须使用已有 `.codegraph` 索引执行 explore；若索引有 pending
changes，先 sync，再以当前源码、测试和最终 artifact 为准。

## 8. 自动测试要求

至少新增以下覆盖：

1. STATUS v3 多行、拆包、粘包、缺行、重复行和未知扩展行。
2. 42 点计划顺序、42/38 数量和端点策略。
3. `+/-4.8 V` rail 故障注入必须停止；`+/-4.6 V` 正常 fixture PASS。
4. `0.2/0.5 V/div` 混档数据必须被拒绝或分组，不得静默合并。
5. reference 修正滞回、原始码差和最大 reference 差。
6. DG/DS prepare/apply 回读失败、错误队列非零、DS 10054。
7. COM 断线、POINT 未知、journal 恢复和无身份时拒绝自动续跑。
8. IMPORT/APPLY/UNDO 确认次数和取消路径。
9. 960x640、1280x780、1440x900 下文本不重叠、按钮不越界、进度不抖动。
10. 原 50 项测试和 SignalHost CMake/C 仿真用例不得回归。

## 9. 软件验收标准

以下全部满足才可标记“纯软件 PASS”：

1. `HOST_UI_TEST_PASS`
2. `PROTOCOL_STATIC_PASS`
3. 双击启动器 `DOUBLE_CLICK_LAUNCHER_PASS`
4. 最终 `203309` 日志解析为 EXT 42 点、EXPORT 1/42/1、CRC 错误 0
5. 最终 42 行 CSV 全部 `flags=0`
6. 正式 38 点复核数值与
   `ADC校准全流程拟合复核_20260728_203309_UTF-8.json` 一致
7. GUI fake 完整跑完 42 点，进度从 0 单调到 100%，不自动 IMPORT/APPLY
8. Python compile、PowerShell AST、严格 UTF-8、尾随空白检查通过
9. 更新 README、文件改动记录和新的验收记录，保存测试日志与截图

## 10. 硬件验收边界

纯软件 PASS 不代表以下项目通过：

- COM3 真实连续命令
- DG/DS 真实联动
- 42 点重新采集
- IMPORT/CONFIRM/APPLY
- active/in_use
- 第二 Profile、UNDO、掉电重启
- QSPI 恢复和真实校准精度

上述项目必须由用户另行明确授权，并按 HIL 与仪器控制规则逐阶段执行。

## 11. 禁止项

- 不自动烧录、复位、Mass Erase、Option Bytes 或 QSPI 擦写。
- 不在应用启动时自动打开 COM 或连接仪器。
- 不自动执行 IMPORT、CONFIRM、APPLY、UNDO、CLEAR。
- 不发送 raw SCPI，不访问 E8267D，不控制未授权通道。
- 不删除或覆盖原始 HIL 日志、原始 FAIL 拟合和复核 PASS 文件。
- 不通过 reset/checkout/clean、递归删除或删除后重建实现回滚。

## 12. 交付物

1. 源码和测试差异
2. 前台 42 点 fake 完整运行截图
3. STATUS v3 和最终 HIL replay 报告
4. 最终复核数值一致性 JSON
5. 自动测试完整日志
6. 更新后的 README、文件改动记录和验收记录
7. 修改前备份、关键 SHA-256 和逐文件回滚说明

本任务推荐按 `P0 -> P1 -> P2 -> P3 -> P4` 顺序实施。P0 未通过时不得进入真实硬件。
