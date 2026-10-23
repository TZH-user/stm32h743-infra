# AutoBench —— 仪器自动测量台（离线软件阶段）

> 状态：离线软件阶段完成  
> 编码：UTF-8  
> 硬件状态：DG4062、DS4014E 与开发板均下电；所有硬件项待上电验证

## 1. 当前可用能力

- JSON v1 计划加载、结构校验和频点展开。
- DG4062 激励幅度双层限制：计划上限 + `5 Vpp` 不可提高的全局硬上限。
- `--dry-run` 打印逐频点计划，保证零 socket、零串口、零 SCPI。
- 可重复模拟波形分支和 `board_vcp` CSV 分支。
- PC 端计算 Vpp 比值、幅频平坦度、THD、SNR、频率精度。
- 输出 `data_UTF-8.csv`、`报告_UTF-8.md`、`结果_UTF-8.json`、`曲线.png`。
- `scope` 分支在加载 DS4014E Toolkit 前硬拒绝，退出码为 4。
- 异常路径没有 DG4062 输出开启/关闭调用；离线阶段既不开启，也不尝试关闭仪器输出。

## 2. 环境与依赖

本轮复用本机已有环境，没有安装任何依赖：

| 项 | 已验证版本 |
|---|---|
| Python | 3.12.10 |
| numpy | 2.5.1 |
| matplotlib | 3.11.1 |

运行时建议在 PowerShell 7 中设置 `PYTHONDONTWRITEBYTECODE=1`，避免产生 `__pycache__`。
matplotlib 缓存默认放在 `AutoBench\reports\.matplotlib_cache`，不新增大型 C 盘依赖或缓存。

## 3. 快速验收

在 PowerShell 7 中执行：

```powershell
Set-Location F:\STM32\AutoBench
$env:PYTHONDONTWRITEBYTECODE='1'
$env:MPLCONFIGDIR='F:\STM32\AutoBench\reports\.matplotlib_cache'

# T1：仅打印 20 点完整计划；零仪器通信
python -B autobench.py --plan plans\bode_100_1M.json --dry-run

# T2：20 点模拟幅频
python -B autobench.py --plan plans\bode_100_1M.json --simulate `
  --output-dir reports\acceptance_bode_rerun

# T3：1 kHz 模拟 THD/SNR/频率精度
python -B autobench.py --plan plans\thd_1k.json --simulate `
  --output-dir reports\acceptance_thd

# 频率精度扫频
python -B autobench.py --plan plans\freq_accuracy.json --simulate `
  --output-dir reports\acceptance_freq

# board_vcp CSV 分支
python -B autobench.py --plan plans\board_vcp_demo.json `
  --output-dir reports\acceptance_board_vcp
```

成功标志：命令输出 `"passed": true`，同时明确输出：

```json
"hardware_access_attempted": false,
"instrument_output_changed": false
```

## 4. 安全拒绝验收

```powershell
# 预期退出码 3：6 Vpp 超过硬上限
python -B autobench.py --plan plans\safety_reject_6vpp.json --dry-run
$LASTEXITCODE

# 预期退出码 4：scope 在任何网络访问前被离线硬门禁阻止
python -B autobench.py --plan plans\bode_100_1M.json
$LASTEXITCODE
```

预期错误前缀分别为 `[安全拒绝]` 和 `[离线硬门禁]`。

## 5. 自动测试

```powershell
Set-Location F:\STM32\AutoBench
$env:PYTHONDONTWRITEBYTECODE='1'
$env:MPLCONFIGDIR='F:\STM32\AutoBench\reports\.matplotlib_cache'
python -B -m unittest discover -s tests -v
```

当前结果：`10 tests / OK`。测试覆盖：

- 20 点计划展开；
- 未知 schema 字段拒绝；
- 6 Vpp 与提高硬上限均被拒绝；
- dry-run 不创建 socket；
- scope 在 socket 前被阻止；
- 模拟路径不调用输出控制；
- THD/SNR/频率算法；
- 模拟与 board_vcp 的 CSV/Markdown/JSON/PNG 报告。

## 6. JSON 计划

机器可读 schema：`schema\plan_v1.schema.json`。运行时采用项目内轻量校验器，避免新增
`jsonschema` 依赖。核心字段：

```json
{
  "version": 1,
  "name": "thd_1k",
  "dut": "被测通道说明",
  "source": {
    "instrument": "dg4062",
    "channel": 1,
    "waveform": "sine",
    "amplitude_vpp": 1.0,
    "frequency_hz": 1000.0
  },
  "capture": {
    "source": "scope",
    "channel": 1,
    "samples": 16384
  },
  "metrics": ["thd", "snr", "frequency_accuracy"],
  "pass": {
    "thd_percent_max": 1.0,
    "snr_db_min": 50.0,
    "frequency_error_ppm_max": 100.0
  },
  "safety": {"amplitude_vpp_max": 5.0}
}
```

可用 capture source：

- `simulate`：直接生成离线波形；也可对任意计划增加 CLI `--simulate` 覆盖。
- `board_vcp`：读取 CSV，不打开串口。
- `scope`：仅保留 schema 和计划语义；当前离线版本硬拒绝。

## 7. board_vcp CSV 约定

单频最小表头：

```csv
time_s,output_v
0.000000000,0.000000
```

兼容列名：

- 时间：`time_s` 或 `time`
- 输出：`output_v`、`volt_v` 或 `voltage_v`
- 可选输入：`input_v` 或 `source_v`
- 多频点长表必须增加 `frequency_hz` 或 `freq_hz`

CSV 使用 UTF-8/UTF-8-SIG；示例：`examples\board_vcp_1k_UTF-8.csv`。

## 8. 指标定义

- 幅度比：`20 * log10(output_vpp / input_vpp)`；无输入列时，以计划激励 Vpp 为参考。
- 频率：Hann 窗 FFT 峰值 + 抛物线插值。
- THD：基波频率处最小二乘正弦拟合，累计 2～5 次谐波 RMS。
- SNR：基波 RMS 与扣除 DC、基波和 2～5 次谐波后残差 RMS 的比值。
- 频率精度：`(measured - expected) / expected * 1e6 ppm`。

这些定义是 PC 端可重复算法，不调用 DS4014E 内置 `MEAS`。

## 9. 仪器协议复用关系

- DS4014E 驱动唯一来源：
  `F:\STM32\stm32project\VSCodeCMAKEProject\DS4014E_Toolkit\ds4014e.py`。
- `instruments\ds4014e.py` 只保存复用路径、待验证项和离线硬门禁，不复制驱动。
- DG4062 的波形/Noise/`:OUTP<n>` 语义只读提炼自：
  `F:\MCP\LabInstrumentsMCP\source\lab_instruments\dg4062.py`。
- `instruments\dg4062.py` 只有纯字符串规划函数，不含 socket、串口或发送函数。
- `F:\MCP\LabInstrumentsMCP` 未被修改。

## 10. 当前离线验收结果

| 计划 | 结果 | 关键数值 |
|---|---|---|
| `bode_100_1M` 模拟 20 点 | PASS | 平坦度 0.0432 dB；最大频率误差 106.161 ppm |
| `thd_1k` 模拟 | PASS | THD 0.11316%；SNR 67.955 dB；频率误差 5.487 ppm |
| `freq_accuracy` 模拟 5 点 | PASS | 最小 SNR 60.255 dB；最大频率误差 27.057 ppm |
| `board_vcp_demo` | PASS | THD 0.10805%；SNR 74.151 dB；频率误差 60.167 ppm |

以上均为**有依据的离线软件结果**，不能外推为仪器或被测板真实性能。

## 11. 报告目录与 Git 说明

默认报告放在 `F:\STM32\AutoBench\reports`。按用户约束，本轮没有修改
`F:\STM32\.gitignore`。顶层仓库当前采用白名单规则，`AutoBench` 整个目录默认不纳管，
因此源码和报告都不会由顶层仓库自动保护；这不是 `git status` 干净就代表已提交。
若后续需要版本管理，应单独取得授权并设计纳管策略，报告目录仍建议排除。

## 12. 待上电验证与回滚

上电最小矩阵见：

`F:\STM32\stm32project\VSCodeCMAKEProject\DS4014E_Toolkit\Docs\DS4014E_Toolkit接口与安全审计_20260727_UTF-8.md`

回滚方法：本目录是新建交付物，未覆盖其他工程。若用户决定移除，应按“一次删除一个明确
文件”的规则手动逐文件处理；不得使用递归删除。Toolkit 的 README 改动按其目录内
`文件改动记录_UTF-8.md` 逐段恢复。
