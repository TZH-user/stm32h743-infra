# SignalHost 文件改动记录（UTF-8）

## 2026-07-27：PC 算法仿真台纯软件阶段

### 授权与范围

- 唯一写入目录：`F:\STM32\SignalHost`。
- 只读外部引用：SignalCore 正本和切换测试目录。
- 未修改 `SignalCore`、`CalibrationCore`、`signal_separation`、`F:\STM32\.gitignore` 或其他任务目录。

### 新增/修改文件

1. `CMakeLists.txt`：外部 `SIGNALHOST_CORE_DIR`、MSVC/GNU 分支、Host 静态库与可执行文件。
2. `src\host_main.c`：CSV 读取、port 注入、4096 点逐帧处理、Vote、CSV/JSON 输出。
3. `src\compat\signalhost_compiler_compat.h`：Host 剥离 MCU section 属性。
4. `src\compat\app_analysis_config.h`：Host 专用分析配置。
5. `src\compat\arm_math.h`、`arm_common_tables.h`、`arm_math_compat.c`：SignalCore 所需最小 CMSIS API 兼容层。
6. `src\compat\app_trig_lookup_host.c`：标准数学库三角函数适配。
7. `tools\gen_signal.py`：sine/dual/sweep/AM/FM、噪声和削顶激励。
8. `tools\plot_result.py`：时域/频谱/算法结果三联图。
9. `tools\verify_results.py`：双音与扫频自动验收。
10. `tools\summarize_snr.py`：SNR 退化表、JSON 和 PNG。
11. `tools\run_case.ps1`：编译器探测、构建、运行、绘图、验收一键流程。
12. `cases\*.json`：双音、扫频、三档 SNR、加噪单音、AM、FM 预置用例。
13. `README_使用说明_UTF-8.md`、`验收记录_20260727_UTF-8.md`、本记录。

### 关键差异与原因

- 没有直接 `add_subdirectory(SignalCore)`：正本 CMake 依赖固件工程的 `APP/Config`、`cmsis_dsp_fft`、GNU 编译选项和链接库，不能直接用于独立 MSVC Host。
- 没有复制算法源码：Host CMake 从 `${SIGNALHOST_CORE_DIR}/src` 编译同一源文件，保持算法单一事实源。
- Sweep 采用 stepped-sine 和 FFT 主峰轨迹：SignalCore Joint/Vote 是双分量模型，单音扫频不应伪造 Joint 有效。
- 构建输出全部位于 `build\`；早期 clang 失败现场保留在 `build\host`，没有用批量删除清理。

### 验证

- MSVC Configure/Build：PASS。
- T1/T2/T3/T4：全部 PASS。
- PowerShell 解析：PASS。
- Python 4 个脚本 AST 解析：PASS。
- 默认 SignalCore 构建前后 SHA-256：差异 0。
- 两个 SignalCore 目录 22 文件清单：差异 0。

### 回滚

本任务开始前目录不存在，因此没有旧版本需要恢复。停止使用 `F:\STM32\SignalHost` 即可撤销对工作流的影响；若要物理移除，必须由人工按明确路径逐文件删除，禁止递归删除。

## 2026-07-27：图形化校准上位机纯软件阶段

### 授权与硬件边界

- 写入范围：`F:\STM32\SignalHost`。
- 只读核对：`signal_separation` 协议文档、串口 parser、CalibrationCore CRC 和 USART1 配置。
- 未修改固件工程、SignalCore、CalibrationCore、父仓库 `.gitignore` 或硬件状态。
- 未打开 COM3、仪器、ST-Link；未烧录、复位、切继电器或执行真实 CAL/QSPI。

### 新增文件

1. `calibration_host\*.py`：parser、CRC、拟合、状态机、串口/fake/回放、持久化和 tkinter GUI。
2. `tests\test_*.py`：38 项 parser、CRC、拟合、状态机、回放、持久化和 GUI 自动测试。
3. `replays\*_UTF-8.log`：正常流程、confirm/apply 失败、断线未知结果。
4. `examples\*`：示例 CSV、JSON 和离线校准报告。
5. `tools\run_calibration_gui.ps1`：默认 fake 的 GUI 启动脚本。
6. `tools\run_calibration_tests.ps1`：自动测试与协议静态检查入口。
7. `tools\protocol_static_check.py`：对固件真源执行静态协议契约检查。
8. `校准上位机验收记录_20260727_UTF-8.md`：本阶段证据与剩余硬件门。

### 修改文件与备份

- `README_使用说明_UTF-8.md`：追加 GUI 启动、测试、分层、安全门禁和已知缺口。
- 本文件：追加实际文件变更与回滚记录。
- 修改前备份：`build\backup_20260727_calibration\README_使用说明_UTF-8.md.bak`、
  `build\backup_20260727_calibration\文件改动记录_UTF-8.md.bak`。

### 关键实现差异

- 保持现有 CMake/C 仿真链原样，校准域作为独立 Python package 集成。
- GUI 默认仅用 serial fake；真实串口必须使用 `-AllowRealSerial` 进程门禁并人工连接。
- POINT 收到 queued 行后仍保持 in-flight，只有最终 `[CAL] EXT ... crc=...` 才完成。
- mutating 命令超时、取消或断线后标记结果未知，冻结写命令，要求 `CAL STATUS` 复核。
- `CONFIRM` 与 `APPLY` 独立；APPLY 二次确认；无 `CAL UNDO` 假按钮。
- 原始点序不变，拟合副本可排序；异常点显式排除并保留原因。
- 固件未提供的 DAC/Profile 字段显示 `NOT_PROVIDED`。

### 验证

- `python -m compileall`：PASS。
- PowerShell AST：PASS。
- 逐文件尾随空白检查：PASS。
- 校准自动测试：`Ran 38 tests`，`OK`，`HOST_UI_TEST_PASS`。
- 固件静态协议检查：本任务所用协议为 `PROTOCOL_STATIC_PASS`；最终复核发现外部未提交
  工作树新增 `CAL UNDO/CAL EXPORT` 而协议文档仍否认 UNDO，额外输出
  `PROTOCOL_BASELINE_DRIFT`，本任务未跟随启用。
- 原有 `dual_1k_10k` CMake/C 用例：PASS，Ninja 无需重编译。
- 父 Git 根为 `F:\STM32`，但 `.gitignore:2 /*` 忽略 `SignalHost`；因此
  `git diff --check` 返回 0 不能覆盖这些新增文件，已用逐文件检查补充且未修改 `.gitignore`。

### 回滚

恢复两个既有文档时可使用上述 `.bak` 明确文件。新增校准文件必须按本节清单逐个
处理；禁止批量或递归删除。原有 CMake/C 工作流没有入口变更，停止调用两个新增
PowerShell 脚本即可在不删除文件的情况下回到旧工作流。

## 2026-07-27：增加双击启动入口

- 新增 `启动校准上位机_UTF-8.cmd`，双击后在工程根目录执行
  `python.exe -m calibration_host.app`。
- 默认不传 `--allow-real-serial`，因此仍只具备 fake 安全启动边界。
- Python 不在 PATH 或 GUI 启动失败时保留命令窗口并显示诊断提示。
- 回滚方式：停止使用该单个 `.cmd` 文件；如需删除，按明确绝对路径单文件处理。

## 2026-07-27：接入短格式 EXPORT 与远程 UNDO

### 修改范围与备份

- 仅修改 `F:\STM32\SignalHost`；固件工程保持只读。
- 修改前逐文件备份：`build\backup_20260727_export_undo`。
- 未打开 COM3、仪器或 ST-Link，未烧录、复位或执行真实 CAL/QSPI。

### 实际修改

1. `calibration_host\protocol.py`：在线解析 `[CAL] EXPORT B/P/E`；`r/m/p` 严格按
   8 位 hex 重解释为 IEEE-754 binary32；按小端 20 字节点结构校验完整 8 位 POINT CRC。
2. `calibration_host\transport.py`：SerialFake 输出短格式，生成真实 POINT CRC，支持
   CRC 故障注入、`CAL EXPORT [LATEST|sequence]` 和 `CAL UNDO`。
3. `calibration_host\session.py`：新增导出结果模型，校验 B→P*→E 的 sequence、0 基索引、
   声明点数和结束点数；错误 CRC 点不入表；UNDO 纳入 mutating 未知结果门禁。
4. `calibration_host\gui.py`：新增“会话导出”页与结果表；启用 UNDO，并保留两次确认。
5. `tests\test_protocol.py`、`test_session.py`、`test_gui.py`、
   `test_persistence_replay.py`：新增短格式、binary32、CRC 错误、历史 legacy、导出事务与
   UNDO GUI/状态机覆盖。
6. `tools\protocol_static_check.py`：核对固件短格式宏、board 引用、POINT 结构和 UNDO
   Service→Worker 状态；报告既存 IMPORT 4/5 字段漂移。
7. `README_使用说明_UTF-8.md`、`校准上位机验收记录_20260727_UTF-8.md`、本记录：
   更新使用方式、证据和硬件边界。

### 验证与边界

- 校准自动测试：`Ran 48 tests`、`OK`、`HOST_UI_TEST_PASS`。
- 固件静态检查：`PROTOCOL_STATIC_PASS`；当前 IMPORT 字段差异另报
  `PROTOCOL_BASELINE_DRIFT`。
- GUI 1280×780 fake 截图：`build\gui_export_1280x780_UTF-8.png`，无重叠。
- 原 `dual_1k_10k`：PASS，两个频率误差均为 0.0%。
- 以上不代表真实串口、QSPI 回滚、掉电保持或校准精度通过，仍为 `HARDWARE_PENDING`。

### 回滚

按 `build\backup_20260727_export_undo` 中的同名备份逐个恢复明确文件；不得通过递归或
批量删除回滚。

## 2026-07-27：对齐 CAL IMPORT ADC int32[5] 并启用 CodeGraph

### 修改内容

- `calibration_host\crc.py`：IMPORT payload 从小端 `int32[4]` 升级为 `int32[5]`，字段
  顺序为 gain、offset、R² ppm、最大绝对残差 millicode、points。
- `calibration_host\fit.py`：R² 缩放由 `×1000` 改为 `×1000000`，新增最大绝对残差
  `×1000` 字段。
- `calibration_host\transport.py`：SerialFake 严格要求五字段加 CRC，并按 `<5i` 校验。
- `tests\test_crc.py`、`test_fit.py`、`test_session.py`：增加截图已知向量和完整命令断言：
  `CAL IMPORT ADC -414002 2056898 999981 7439 38 6B0AA818`。
- `tools\protocol_static_check.py`：将 Host `<5i`、`r2_ppm`、`residual_mc` 作为硬性契约，
  删除已解决的 IMPORT 漂移提示。
- 初始化 `F:\STM32\SignalHost\.codegraph`，初始索引为 28 文件、450 nodes、1166 edges；
  修改后执行 `codegraph sync`。
- 当前用户 PATH 新增 `F:\MCP\codegraph-win32-x64\codegraph-win32-x64\bin`，版本 1.5.0。

### 备份与回滚

- 源码/测试/文档备份：`build\backup_20260727_import_int32x5`。
- PATH 修改前注册表备份：
  `build\backup_20260727_codegraph_path\用户环境变量修改前_20260727_UTF-16.reg`。
- 源码按备份逐文件恢复；PATH 回滚只移除上述 CodeGraph bin 条目，或导入注册表备份。
- 未删除文件，未访问 COM3、仪器、ST-Link，未烧录、复位或执行真实 CAL/QSPI。

### 验证

- `Ran 50 tests`、`OK`、`HOST_UI_TEST_PASS`、`PROTOCOL_STATIC_PASS`。
- 静态契约输出 `CAL IMPORT ADC int32[5]`，不再输出 `PROTOCOL_BASELINE_DRIFT`。
- 已知向量 `{ -414002, 2056898, 999981, 7439, 38 }` 的 CRC32 为 `6B0AA818`，
  Session 完整命令断言通过。
- Python compile、PowerShell AST、严格 UTF-8、尾随空白、`git diff --check` 全部 PASS。
- CodeGraph 最终状态：28 files、453 nodes、1184 edges、`Index is up to date`。

## 2026-07-28：ADC 校准前台交互升级任务书

### 新增文件

- `SignalHost_ADC校准前台交互升级任务书_20260728_UTF-8.md`

### 依据与范围

- 只读检查现有 SignalHost GUI、session、fit、protocol、transport、测试和当前
  `signal_separation` 校准协议。
- 实际运行现有 50 项 Host 测试：全部 `OK`，`HOST_UI_TEST_PASS`。
- 双击启动器自检：`DOUBLE_CLICK_LAUNCHER_PASS`。
- 最终 HIL 日志兼容性检查：EXT 42 点、EXPORT B/P/E=`1/42/1`、POINT CRC 错误 0。
- 发现 `protocol_static_check.py` 仍检查旧 `[CAL] STATUS state=%u` 文本，
  与当前 `CAL STATUS v3` 宏位置和格式不一致，因此整套测试入口最终退出码为 1。
- 本轮只新增任务说明和更新本记录；没有修改软件源码，没有打开 COM3、仪器或
  ST-Link，没有执行真实校准和 Profile 写入。

### 备份与回滚

- 修改本记录前备份：
  `build\backup_20260728_calibration_frontend_task\文件改动记录_UTF-8.md.bak`
- 备份 SHA-256：
  `892BECA723DCB6D88DAA9C50396C3BF5F7FF6466C7E42ACCB36BAAE0BF2BD6CA`
- 回滚时逐文件恢复本记录；新增任务书停止采用即可。禁止递归或批量删除。

## 2026-07-28：ADC 校准前台交互升级

### 修改与新增

- `calibration_host\protocol.py`：STATUS v3 CORE/SESSION/SELECT/PROFILE/PROFILE2/
  ADC/LAST/IO/HW 分行解析、严格字段和未知扩展兼容。
- `calibration_host\fit.py`：42/38 点正式质量报告，rail、frame pp、reference Vpp、
  配对差和 `code_delta - gain * reference_delta` 修正滞回。
- 新增 `calibration_plan.py`、`workflow.py`、`artifacts.py`、
  `instrument_adapter.py`：正式计划、阶段状态机、journal/离线 REVIEW、结构化仪器 CLI。
- `session.py`、`transport.py`、`persistence.py`：完整点字段、Fake 42 点、只读 REVIEW、
  candidate 预览、Profile 审计和 apply 后 STATUS 运行态确认。
- `gui.py`、`app.py`、`run_calibration_gui.ps1`：42 点进度页、响应式布局、仪器页、
  `--allow-instruments` 独立门禁。
- 新增最终 203309 CSV/串口 replay fixture、测试、截图/复核工具和本次验收记录。

### 安全边界

- 没有打开 COM3、仪器、ST-Link；没有烧录、复位、QSPI 写入或 Profile 写入。
- GUI 默认关闭真实串口和仪器两个进程门禁；Fake 不自动 IMPORT/CONFIRM/APPLY。
- 仪器 adapter 无 raw SCPI，只允许 DG4062/DS4014E CH1；DS 首个 10054 后不重试。
- 未删除文件，未使用递归删除、checkout/reset/clean 或删除后重建。

### 验证

- `Ran 66 tests`、`OK`、`HOST_UI_TEST_PASS`、`PROTOCOL_STATIC_PASS`。
- 最终 HIL：EXT 42、EXPORT 1/42/1、CRC 错误 0，正式复核 CRC `3D4E58A9`。
- Fake 42 点进度单调到 100%，无 IMPORT/APPLY。
- 960×640、1280×780、1440×900 截图无重叠或越界。
- 原 C/CMake `dual_1k_10k` 用例单独回归，结果见本轮测试日志。

### 备份与回滚

- 修改前逐文件备份：`build\backup_20260728_frontend_upgrade`，SHA-256 清单保存在
  `build\backup_20260728_frontend_upgrade\SHA256_UTF-8.txt`。
- 回滚按明确文件逐个从 `.bak` 恢复；新增模块停止引用即可。若需删除，由用户逐文件
  手工处理，禁止批量或递归删除。

## 2026-07-28：42 点流程重复启动门禁与安全退出

### 修改内容

- `calibration_host\gui.py`：标题持续显示真实 `session.phase`；启动按钮按连接、
  idle、在途命令和旧点数据门禁；失败启动不再覆盖原工作流；新增“安全停止并退出”。
- `calibration_host\workflow.py`：正常完成新增 `CAL EXT EXIT`；安全停止等待当前原子
  命令，未知状态先用 `CAL STATUS` 对账，再退出到 idle，并保留点表与 journal。
- `calibration_host\instrument_adapter.py`：保存首次写前状态，只恢复本程序实际改动过
  且通信健康的 DG4062/DS4014E CH1；恢复仍执行 status/prepare/精确确认/apply/readback。
- `tests\test_workflow.py`、`test_gui.py`、`test_instrument_adapter.py`：新增重复启动、
  正常 EXIT、安全停止、UNKNOWN 对账、旧非 idle 会话、数据保留和仪器恢复测试。
- 更新 README、验收记录、三档完成态截图和 960×640 采集中安全停止截图。

### 安全边界

- “暂停”“取消当前等待”“断开”均不冒充退出；只有确认完成的 `CAL EXT EXIT` 才将
  板端释放到 idle。
- 仪器恢复需要用户逐台确认；通信异常、错误队列非零、DS 首个 10054 或用户拒绝时
  停止恢复并明确显示失败，不自动重试。
- 本轮只运行 Fake/Mock 和静态测试，没有打开 COM3、DG4062、DS4014E 或 ST-Link。

### 验证与回滚

- `Ran 74 tests`、`OK`、`HOST_UI_TEST_PASS`、`PROTOCOL_STATIC_PASS`。
- 960×640、1280×780、1440×900 截图中新增按钮和 phase 文本无重叠或越界。
- 修改前逐文件备份位于 `build\backup_20260728_safe_stop_UTF-8`；回滚时按明确文件
  逐个恢复对应 `.bak`。未删除文件，禁止递归或批量删除。
