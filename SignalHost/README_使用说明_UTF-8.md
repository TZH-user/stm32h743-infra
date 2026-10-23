# SignalHost PC 算法仿真台

状态：纯软件阶段已完成（2026-07-27，UTF-8）。

SignalHost 在 Windows PC 上执行“合成激励 → 外部 SignalCore 源码 → CSV/JSON → 三联图 → 自动验收”。算法源码不会复制到本目录，也不会修改正本；CMake 通过 `SIGNALHOST_CORE_DIR` 引用外部目录。

## 1. 目录与边界

```text
F:\STM32\SignalHost
├── CMakeLists.txt
├── src\host_main.c
├── src\compat\                 # Host 配置、编译器与 CMSIS API 最小兼容层
├── tools\gen_signal.py
├── tools\plot_result.py
├── tools\verify_results.py
├── tools\summarize_snr.py
├── tools\run_case.ps1
├── cases\*.json
└── build\                      # 全部构建产物、激励、结果和图片
```

- 默认算法目录：`F:\STM32\stm32project\Function\Algorithm\SignalCore`
- 可切换目录：通过 `-CoreDir` 传入。
- SignalHost 自带 `app_analysis_config.h`：SignalCore 正本目前不包含该板级配置头，Host 必须提供独立阈值配置。
- SignalHost 自带 `arm_math.h` 最小兼容实现：目录所有权规则不允许把 CMSIS-DSP 源码复制进来或修改固件工程，因此仅实现 SignalCore 实际调用的 RFFT/SinCos API；算法处理链的源文件仍来自外部 `SIGNALHOST_CORE_DIR`。

## 2. 环境

现场实际使用：

- PowerShell 7
- CMake：`F:\STM32\stm32toll\CMake\bin\cmake.exe`
- Ninja：Visual Studio 2022 Community 随附版本
- C 编译器：MSVC x64 19.42.34436
- Python 3.12.10
- numpy 2.5.1
- matplotlib 3.11.1

`run_case.ps1` 按 `gcc → PATH 中 clang → MSVC` 选择编译器。现场 PATH 中没有 gcc/clang；曾探测到 LVGL Editor 内部 clang，但其 lld 不可执行，脚本不会把该私有 clang 当成通用编译器，最终使用已有 MSVC，未安装新依赖。

## 3. 一键运行

在 PowerShell 7 执行：

```powershell
cd F:\STM32\SignalHost

# T1：1 kHz + 10 kHz 双音，幅度 3:1，SNR 40 dB
.\tools\run_case.ps1 dual_1k_10k

# T2：每帧恒频的 stepped-sweep，100 Hz → 50 kHz
.\tools\run_case.ps1 sweep_100_50k

# T3：同一双音的 40/20/10 dB SNR 退化
.\tools\run_case.ps1 snr_degradation

# T4：切换外部算法目录并使用独立构建目录
.\tools\run_case.ps1 dual_1k_10k `
    -CoreDir F:\STM32\stm32project\VSCodeCMAKEProject\signal_separation\SignalCore `
    -BuildName core_switch_msvc
```

成功标志均为末行 `PASS:`，失败会抛出非零退出码。

## 4. 数据格式

输入 CSV：

```text
# sample_rate_hz=204800
# sample_format=unsigned12
# analysis_mode=dual
# case_name=dual_1k_10k
2048
...
```

- 样本为 `0..4095` 的无符号 12 位 ADC 码。
- 每帧固定 4096 点。
- 双信号识别使用 8 帧投票；每帧由同一随机种子的连续独立噪声样本构成，不重复复制单帧。
- Sweep 使用逐帧恒频 stepped-sine。单音不满足双分量 Joint/Vote 模型，因此验收使用 SignalCore FFT 的 `full_spectrum_peak_frequency_hz` 轨迹。

输出位于：

```text
F:\STM32\SignalHost\build\results\<BuildName>\<CaseName>\
├── input.csv
├── truth.json
├── result.csv
├── result.json
├── verification.json      # T1/T2
└── result.png
```

SNR 汇总位于：

`F:\STM32\SignalHost\build\results\host_msvc\snr_degradation`

## 5. 已验证结果

详见 `F:\STM32\SignalHost\验收记录_20260727_UTF-8.md`。

- 双音频率误差：0.0% / 0.0%，类型均为 sine。
- Sweep 64 帧轨迹：RMSE 4.08 Hz，最大绝对误差 5.76 Hz。
- SNR 40/20/10 dB：投票置信度 `0.58143 → 0.20519 → 0.03852`。
- 两个 SignalCore 目录各 22 个文件 SHA-256 清单一致，切换后重新构建和 T1 验收通过。

## 6. 风险与回滚

- SignalHost 是独立新目录，不改变固件工程、算法正本、`.gitignore` 或硬件状态。
- Host 的 FFT 兼容层用于 PC 软件回归，不替代板端 CMSIS-DSP 性能与数值一致性验证。
- 回滚不需要恢复其他工程：停止调用本目录即可。若确需移除，按用户规则由人工逐个删除明确文件；不要使用递归删除命令。

## 7. 图形化校准工作台（纯软件 fake 阶段）

校准工作台在现有 SignalHost 内以独立 Python package 集成，没有修改原有 CMake、
`signalhost.exe` 或 SignalCore 引用关系。技术栈为 Python 3.12、`tkinter`、标准库
`unittest`；真实串口后端使用现场已有 `pyserial`，没有安装新依赖。

默认启动 serial fake：

可直接双击：

`F:\STM32\SignalHost\启动校准上位机_UTF-8.cmd`

也可以在 PowerShell 中启动：

```powershell
cd F:\STM32\SignalHost
.\tools\run_calibration_gui.ps1
```

启动后选择 `Serial Fake` 并点击“连接”。fake 使用明确标注的
`SERIAL_FAKE_V1 / FAKE_DEMO_CONFIG`，不会枚举或打开 Windows COM 口。连接后先切到
LOG 模式，但不会自动进入校准、自动 `CONFIRM`、自动 `APPLY` 或自动 `UNDO`。

“会话导出”页支持输入 `LATEST`（也可留空）或非零十进制 sequence，再点击“读取导出”。
Host 只接受在线短格式 `[CAL] EXPORT B/P/E`，将 `r/m/p` 的 8 位十六进制按
IEEE-754 binary32 解码，并按小端 `CalibrationMeasurementPoint` 20 字节 payload
重算每个 POINT CRC；CRC 错误点不会进入结果表。旧 `BEGIN/POINT/END` 只在加载历史
回放日志时启用，截断 CRC 始终作为 malformed 记录。

生命周期页的 `UNDO` 会发送 `CAL UNDO`，发送前必须连续通过两次确认。它与 APPLY
一样属于 mutating 命令，超时、断线或取消后会冻结写操作，要求先用 `CAL STATUS` 复核。

真实串口有两层门禁：默认进程拒绝打开；只有用户另行授权后，人工使用
`.\tools\run_calibration_gui.ps1 -AllowRealSerial` 启动，并在界面输入端口后主动点击
连接，才可能打开端口。本阶段未使用该参数，未打开 COM3。

### 7.1 自动测试

```powershell
cd F:\STM32\SignalHost
.\tools\run_calibration_tests.ps1
```

成功标志：

```text
HOST_UI_TEST_PASS
PROTOCOL_STATIC_PASS
Ran 74 tests
OK
```

覆盖 parser 分包/粘包/CR/LF/CRLF/乱码/超长/未知/重复行，CRC 已知向量、负数、
边界与大小端反例，正负 gain、噪声、离群、平台、重复值、退化跨度、NaN/Inf，
单飞 POINT、乱序/重复点击、队列满、超时、取消、断线、重连复核，candidate / pending /
active 生命周期，`CAL IMPORT ADC` 小端 `int32[5]` 与已知 CRC 向量，短格式 EXPORT、
binary32、逐点 CRC、legacy 历史回放、UNDO，CSV/JSON 往返、三组日志回放和 GUI
二次确认/缩放布局。

测试临时文件固定写入 `F:\STM32\SignalHost\build\calibration_test_*`。测试不会打开
COM3、仪器或 ST-Link，不烧录、不复位、不执行真实 CAL/QSPI。

### 7.2 分层与数据

```text
calibration_host\protocol.py       增量组行、EXPORT B/P/E、binary32 和 POINT CRC
calibration_host\crc.py            CAL IMPORT 小端 int32[5] CRC32
calibration_host\fit.py            无串口/UI 依赖的线性拟合
calibration_host\session.py        单飞命令、超时/取消、生命周期门禁
calibration_host\transport.py      后台串口 I/O、serial fake、日志回放
calibration_host\persistence.py    CSV/JSON/原始日志导出
calibration_host\gui.py            tkinter 工作台
replays\                            正常、confirm/apply 失败、断线未知结果
examples\                           示例 CSV/JSON 和离线报告
tests\                              unittest 自动测试
```

点表始终保留 `original_index` 和原始采集顺序；拟合排序只作用于副本。异常点只能
显式填写原因后排除，原始数据仍保留。未配置实际参考范围、最小点数或质量阈值时，
界面显示 `NOT_CONFIGURED` 并阻止采点/危险动作。DAC1/DAC2 的 gain、offset、限幅、
最大步进、单调性、平台和无响应字段在串口未提供时统一显示 `NOT_PROVIDED`。

### 7.3 已知协议缺口和验收边界

- 当前固件源码与协议文档均已定义短格式 `CAL EXPORT B/P/E` 和 `CAL UNDO`；Host 已接入
  这两个命令，fake/parser/回放/GUI 与源码静态检查通过，但仍未进行上电串口/QSPI 验证。
- 在线 parser 不再接受旧长格式；仅历史回放 Session 显式启用 legacy 解析。旧日志中的
  截断 CRC 不会被判为有效。
- Host 与当前固件的 `CAL IMPORT ADC` 已对齐为
  `<gain_mcpv> <offset_mc> <r2_ppm> <residual_mc> <points> <crc32hex>`。CRC payload 是
  小端 `int32[5]`；R² 乘 `1,000,000`，最大绝对残差 code 乘 `1,000`。
- 当前 `CAL STATUS` 不输出 profile sequence、完整模型或质量字段；工作台显示
  `NOT_PROVIDED`，不会杜撰。
- `HOST_UI_TEST_PASS` 只证明 fake/回放/GUI 软件行为。
- `PROTOCOL_STATIC_PASS` 只证明命令、单位、CRC 和生命周期与当前固件源码静态一致。
- COM3、真实 CAL EXT、真实 confirm/apply、QSPI 持久化、掉电重启、仪器参考电平和
  校准精度均为 `HARDWARE_PENDING`，必须另行逐项授权。

### 7.4 备份与回滚

初始校准工作台修改前备份位于：

`F:\STM32\SignalHost\build\backup_20260727_calibration`

本轮 EXPORT/UNDO 修改前逐文件备份位于：

`F:\STM32\SignalHost\build\backup_20260727_export_undo`

本轮 IMPORT `int32[5]` 修改前逐文件备份位于：

`F:\STM32\SignalHost\build\backup_20260727_import_int32x5`

新增校准模块不改变原 CMake 入口，停止运行 `run_calibration_gui.ps1` 即可退出新流程。
如需物理回滚，按文件改动记录逐个处理明确文件；禁止递归或批量删除。

## 8. ADC 外部 42 点前台（2026-07-28）

第一工作页现为正式 42 点流程。默认 `Serial Fake`，连接后点击“启动 42 点流程”；
流程按现场检查、连接、只读快照、最小阶跃、独占、会话、预爬坡、42 点采集、
FINISH、EXPORT、质量门和人工审计顺序推进。人工审计通过后仅生成 candidate 预览，
可选 IMPORT/CONFIRM/APPLY 阶段默认跳过，不发送写命令；随后自动发送
`CAL EXT EXIT` 并确认会话回到 `phase=idle`。

标题栏持续显示真实 `session.phase`。只有已连接、`phase=idle`、没有在途命令且当前
会话没有旧采点数据时，“启动 42 点流程”才可用；重复点击不会替换正在运行的工作流。
中止后如需重新开始，应先导出已采数据，再断开并重新连接以建立新会话。

“安全停止并退出”用于中途释放占用：

1. 等待当前 POINT 等原子命令完成，不在命令中间截断；
2. 停止继续发送新点；
3. 结果未知时先只读执行 `CAL STATUS` 对账；
4. 发送 `CAL EXT EXIT` 并等待 `state=0 / phase=idle`；
5. 保留已采点、journal 和原始日志；
6. 对本程序实际改动过且通信健康的 DG4062/DS4014E CH1，重新执行
   `status -> prepare -> 用户确认 -> apply -> readback` 恢复首次写入前状态。

“暂停”只在两点之间暂停，不解除独占；“取消当前等待”不等于退出；顶部“断开”只关闭
串口，也不等于 `CAL EXT EXIT`。真实硬件需要释放占用时应使用“安全停止并退出”。

正式预置为：

- 采集 `+/-4.6 V`，拟合 `+/-4.5 V`，ascending + descending 共 42 点，拟合 38 点。
- DS CH1 扫点 `0.5 V/div`，恢复 `0.2 V/div`；reference 配对差 `<=60 mV`，
  reference Vpp `<=100 mVpp`。
- rail 要求 `minimum_code > 15` 且 `maximum_code < 4080`。
- `R²>=0.999900`、最大残差 `<=8 code`、reference 修正滞回 `<=4 code`、
  最大帧内峰峰 `<=16 code`。

每个原子点完成后追加 UTF-8 JSONL journal。断线、超时或未知 POINT 结果会冻结 mutating
命令；当前固件没有会话唯一身份，因此 journal 只提供人工核对，不允许静默自动续跑。

“打开 REVIEW”支持最终 HIL CSV、串口 TXT/LOG，以及包含 `points` 或 `csv_path` 的 JSON。
离线 REVIEW Session 固定 `mutating_enabled=False`，不能发送 IMPORT。

### 8.1 仪器结构化 CLI

仪器页只接入 `lab-instrument-control` Skill CLI，不包含 raw SCPI。默认进程门禁关闭。
另行取得真实仪器授权后，可使用：

```powershell
cd F:\STM32\SignalHost
.\tools\run_calibration_gui.ps1 -AllowInstruments
```

DG4062 和 DS4014E 仅提供 CH1；每次写入必须先读取 status，再 prepare 并展示
`confirmation_required`，用户确认后 apply，随后回读 status 和错误队列。DS4014E 首个
`WinError 10054` 或超时会冻结该设备，本轮不自动重试。E8267D 和其他通道没有入口。

真实串口仍使用独立 `-AllowRealSerial` 门禁；两个开关互不隐含。

### 8.2 回归与证据

```powershell
cd F:\STM32\SignalHost
.\tools\run_calibration_tests.ps1
python .\tools\review_final_hil_UTF-8.py
python .\tools\capture_calibration_gui_UTF-8.py
```

当前成功标志为 `Ran 74 tests`、`HOST_UI_TEST_PASS` 和 `PROTOCOL_STATIC_PASS`。测试
覆盖重复启动门禁、真实 phase 显示、正常完成自动 EXIT、安全停止等待原子点、
未知状态对账、旧非 idle 会话退出、数据保留和仪器确认恢复。最终
203309 fixture 回放为 EXT 42 点、EXPORT B/P/E=`1/42/1`、POINT CRC 错误 0；
正式复核 CRC 为 `3D4E58A9`。GUI 截图覆盖 960×640、1280×780、1440×900。
采集中安全停止按钮证据为
`build\gui_fake_safe_stop_midpoint_960x640_UTF-8.png`。

以上为纯软件 PASS。COM3、真实 DG/DS 联动、重新采集、IMPORT/CONFIRM/APPLY、
第二 Profile、UNDO、掉电重启和 QSPI 恢复仍为 `HARDWARE_PENDING`。
