# AutoBench 文件改动记录

> 编码：UTF-8

## 2026-07-27 离线软件阶段

### 新增核心

- `autobench.py`
- `autobench_core\__init__.py`
- `autobench_core\errors.py`
- `autobench_core\plan.py`
- `autobench_core\signals.py`
- `autobench_core\metrics.py`
- `autobench_core\reports.py`
- `autobench_core\runner.py`

### 新增协议边界

- `instruments\__init__.py`
- `instruments\dg4062.py`
- `instruments\ds4014e.py`

### 新增 schema、计划与示例

- `schema\plan_v1.schema.json`
- `plans\bode_100_1M.json`
- `plans\thd_1k.json`
- `plans\freq_accuracy.json`
- `plans\board_vcp_demo.json`
- `plans\safety_reject_6vpp.json`
- `examples\board_vcp_1k_UTF-8.csv`

### 新增测试与文档

- `tests\__init__.py`
- `tests\test_plan.py`
- `tests\test_metrics.py`
- `tests\test_offline_safety.py`
- `tests\test_reports.py`
- `README_使用说明_UTF-8.md`
- `Docs\离线阶段实施方案_20260727_UTF-8.md`
- `文件改动记录_UTF-8.md`
- `方案改动记录_UTF-8.md`

### 运行生成物

- `reports\acceptance_*`：离线验收 CSV/Markdown/JSON/PNG。
- `reports\test_*`：自动测试报告。
- `reports\.matplotlib_cache`：项目内 matplotlib 缓存。
- `reports\acceptance_bode\首次算法分辨率失败记录_请勿作为终态_UTF-8.md`：标明首次失败现场和最终有效报告目录。

### 范围确认

- 未修改 `F:\MCP\LabInstrumentsMCP`。
- 未修改 `F:\STM32\.gitignore`、signal_separation、OCXO_PhaseLock、SignalHost 或 Function。
- 未执行网络、串口、烧录、复位或仪器控制。

### 最终回归（2026-07-27）

- `python -B -m unittest discover -s tests -v`：10/10 PASS。
- dry-run：PASS，打印 20 个频点，零硬件通信。
- `final_bode`、`final_thd`、`final_freq`、`final_board_vcp`：全部 PASS。
- 6 Vpp 安全用例：预期退出码 3。
- scope 下电硬门禁：预期退出码 4。
- 所有成功报告：`hardware_access_attempted=false`、`instrument_output_changed=false`。
