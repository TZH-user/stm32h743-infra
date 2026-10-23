# CubeMX 与 cubemx-ai 工具异常报告

## 1. 基本信息

- 记录时间：2026-07-24
- 工程：`F:\STM32\stm32project\VSCodeCMAKEProject\signal_separation`
- 分支：`feature/dac-output-no-fpga`
- MCU：STM32H743XIH6，TFBGA240
- `.ioc` 文件版本：CubeMX 6.17.0
- 实际 CubeMX：`E:\ST\cubeMX\STM32CubeMX.exe`
- CubeMX 日志版本：STM32CubeMX 6.17.0-RC5
- `cubemx-ai-cli`：0.2.9
- 固件包：STM32Cube FW_H7 V1.13.0
- 当前状态：未修复，工程开发采用受控绕过方案继续进行

## 2. 异常一：generate 误报成功

### 2.1 复现命令

```powershell
& "F:\STM32\stm32toll\skill\.venv\Scripts\cubemx-ai.exe" generate `
  "signal_separation.ioc" `
  --repository "E:\ST\Repository" `
  --timeout 180 `
  --json
```

### 2.2 工具返回

外层结果同时出现：

```text
"ok": true
"returncode": 0
"post_check": { "ok": true }
```

但 CubeMX 标准输出中出现两次：

```text
KO
```

并且没有生成：

```text
Core/Inc/dac.h
Core/Src/dac.c
```

### 2.3 实际异常

配置加载阶段：

```text
java.lang.IndexOutOfBoundsException: Index 0 out of bounds for length 0
at com.st.microxplorer.plugins.pinoutconfig.engine.BGAPackage$GBall.<init>(BGAPackage.java:627)
at com.st.microxplorer.plugins.pinoutconfig.engine.BGAPackage.setPinOut(BGAPackage.java:532)
```

代码生成阶段：

```text
java.lang.NullPointerException:
Cannot invoke "...ProjectSettingsPanel.getProjectChoiceDisplay()"
because ...getProjectSettingsPanel() is null
```

### 2.4 建议修复

`cubemx-ai generate` 不能只依据进程退出码和静态 post-check 判断成功，建议至少同时检查：

1. stdout/stderr 是否包含独立行 `KO`。
2. stdout/stderr 是否包含 `Exception`、`[ERROR]`。
3. CubeMX 是否输出明确的代码生成成功标记。
4. 对比生成前后的关键文件时间戳或预期生成文件。
5. 任一失败条件成立时返回 `ok=false`，并把 CubeMX 异常摘要放入顶层 `error`。

## 3. 异常二：help-ai 中已验证路径失效

`help-ai --json` 给出的已验证路径为：

```text
E:\ST\STMCubeMX_6.18.0\STM32CubeMX.exe
```

实际 `Test-Path` 返回 `False`。Everything 索引只找到：

```text
E:\ST\cubeMX\STM32CubeMX.exe
```

而 `doctor --json` 也选择了后者。

### 建议修复

1. `help-ai` 不应硬编码历史验证路径，或应在输出前再次检查存在性。
2. 将 `verified` 与本次实时发现结果分开命名。
3. 当推荐路径不存在时，在 `help-ai` 中明确标记 `available=false`。

## 4. 排除过的方向

### 4.1 `.ioc` 静态校验

```text
errors = 0
warnings = 5
```

5 个警告均为原工程 TIM2/PA0 既有项，不属于新增 DAC1 配置。

### 4.2 固件包

`firmware-check` 已确认 STM32Cube FW_H7 V1.13.0 可用。

### 4.3 新增引脚索引顺序

曾怀疑 PA4/PA5 被排在 FreeRTOS、SYS、TIM、MEMORYMAP 虚拟引脚之后导致 BGA 加载失败。已把 PA4/PA5 调整到物理引脚区末尾，再把虚拟引脚顺延；静态校验仍通过，但 CubeMX 异常不变，因此该项不是根因。

## 5. 当前判断

- **有依据**：`cubemx-ai` 当前存在成功状态误判，因为 CubeMX 明确输出 `KO` 和 Java 异常，且预期 DAC 文件未生成。
- **经验判断**：根因更可能位于 CubeMX 6.17.0-RC5 的 TFBGA240 pinout 命令行加载路径，或者该 RC 版本与现有 `.ioc` 的兼容性。
- **尚未确认**：稳定版 CubeMX 6.16.1 或更新稳定版是否能正常生成；当前机器索引中未找到第二个可执行文件。

## 6. 当前绕过方案

1. 保留已通过 `cubemx-ai validate` 的 DAC1/DMA `.ioc` 配置，作为 CubeMX 配置源。
2. 不声称 CubeMX 已成功生成。
3. DAC 用户逻辑优先放入独立 `APP/DacOutput` 模块。
4. 对必须补齐的 HAL 初始化与中断连接采用最小改动，并在 Git 中单独提交。
5. 后续获得可用稳定版 CubeMX 后，重新生成并逐项对比手工补齐内容。

## 7. 后续追加规则

本次开发后续遇到与 `cubemx-ai`、CubeMX、`stm32-build-ai`、烧录或调试相关的新异常，继续追加到本文件，不另建含义重复的报告。

## 8. 异常三：doctor --fast 顶层状态受可选工具影响

### 8.1 复现命令

```powershell
& "F:\STM32\stm32toll\skill\.venv\Scripts\stm32-build-ai.exe" doctor --fast --json
```

### 8.2 实际结果

构建必需工具均存在，CubeProgrammer 和 OpenOCD 也可用，仅缺少可选的 `ST-LINK_gdbserver`，但顶层仍返回：

```text
"ok": false
```

同时结果内部明确写有：

```text
missing_required = []
missing_optional = ["stlink_gdbserver"]
capabilities.build = true
capabilities.openocd_debug = true
```

### 8.3 建议修复

1. 顶层 `ok` 应只表示本次目标工作流是否可执行，或增加 `build_ok`、`flash_ok`、`debug_ok` 分项状态。
2. 缺失可选工具时建议返回 `ok=true` 加 warning；只有用户明确选择 ST-LINK_gdbserver 路径时才判该能力失败。
3. 避免 AI 把“缺一个可选调试后端”误判为整个 STM32 工具链不可用。

## 9. 异常四：HOTPLUG 烧录擦除失败

### 9.1 现象

STLINK-V3MODS、目标电压和 MCU 均识别正常，但使用：

```text
mode=HOTPLUG, freq=1000
```

烧录时在：

```text
Erasing internal memory sectors [0 6]
Error: failed to erase memory
```

失败。改为 `mode=UR` 后，同一 HEX 在约 20 秒内完成下载、校验、复位和运行。

### 9.2 判断与建议

- 该项更像目标运行态/复位时序兼容问题，不足以认定为 CLI 缺陷。
- 建议工具在 HOTPLUG 擦除失败时给出结构化提示：可预览 Under Reset 重试，但不得自动执行 Mass Erase 或修改 Option Bytes。
- 当前工程的保守首选烧录模式应记录为 `UR + 1 MHz SWD`。

## 10. 异常五：vcp-read 新旧数据边界不可靠

### 10.1 现象

1. 第一次读取 COM3 时返回大量历史测试矩阵，包含多组 30/80、30/100、90/100 kHz 结果，不可能来自本轮一次上电会话。
2. 将缓冲读空后，即使先打开 `vcp-read`、再延迟复位或通过 GDB 触发 `AppMain_RequestStart()`，仍返回空文本。
3. 同期 GDB 显示固件已完成新会话，`vcp_peak_queued_bytes=529`、`vcp_enqueue_rejected_bytes=0`、`output_transport_errors=0`、`output_dropped_log_messages=0`。

### 10.2 建议修复

1. 明确 `vcp-read` 是否设置 DTR/RTS，并提供可配置参数和实际回读状态。
2. 增加“打开后清空旧缓冲”和“保留旧缓冲”模式，返回首字节时间戳或数据来源说明。
3. 对 ST-Link VCP 复位掉线/重连增加检测，避免端口句柄仍成功但新数据不可见。
4. 在返回空文本时附加串口驱动状态、DTR/RTS、读超时次数，便于区分固件静默与传输层未就绪。

## 11. 异常六：GDB 主机编码告警

读取 `hdac1.ErrorCode`、`TIM6->CR1` 等全英文符号时，GDB 偶发输出：

```text
warning: could not convert 'ErrorCode' from the host encoding (CP1252) to UTF-32.
warning: could not convert 'TIM_TypeDef' from the host encoding (CP1252) to UTF-32.
```

变量值仍能正常读取。建议 CLI 显式设置 GDB host/target charset，或在确认不影响结果时把该类告警单独归入 `warnings`，不要与调试失败混淆。

## 12. 异常七：当前 `.ioc` 仍触发 CubeMX BGA 包解析崩溃（2026-07-24）

### 12.1 新增复现证据

当前工程：

```text
F:\STM32\stm32project\VSCodeCMAKEProject\signal_separation\signal_separation.ioc
```

CubeMX 版本：`6.17.0-RC5`。加载当前文件时仍出现：

```text
java.lang.IndexOutOfBoundsException: Index 0 out of bounds for length 0
at com.st.microxplorer.plugins.pinoutconfig.engine.BGAPackage$GBall.<init>(BGAPackage.java:627)
```

随后伴随项目设置面板 `NullPointerException`。`cubemx-ai inspect` 仍能解析文件，`validate` 为 0 error、4 warning；4 个 warning 均为既有 TIM2 参数引用缺失。

### 12.2 已验证的对照

1. 将 `DAC_OutputBuffer` 从 `ENABLE` 改为 `DAC_OUTPUTBUFFER_ENABLE`，不能消除崩溃。
2. 移除两个 `DAC_OutputBuffer` 字段及 `DAC1.IPParameters` 中对应 token，正式工程仍能复现崩溃；因此不能把该字段单独认定为唯一根因。
3. 原始 `AD9226_API.ioc` 可以正常加载；另一个含双 DAC 的独立 `.ioc` 也可以加载，说明 CubeMX 6.17.0-RC5 并非所有 TFBGA240 + DAC 配置都必然失败。
4. 当前固件 `APP/DacOutput/app_dac_output.c` 已在 `HAL_DAC_ConfigChannel()` 中显式设置：

```c
channel_config.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
```

所以即使 `.ioc` 暂不保存该字段，运行时 DAC 输出缓冲仍由用户代码开启。

### 12.3 当前判断与边界

- **有依据**：问题发生在 CubeMX `BGAPackage` 解析阶段，而不是 `cubemx-ai validate` 静态解析阶段。
- **有依据**：当前工程与可加载基线之间存在 `Mcu.Pin`、`Mcu.IP`、DMA 请求和任务元数据差异，尚未完成唯一触发项的二分确认。
- **暂不能确认**：不能仅凭现有日志断言根因是某一个 DAC 字段、PA4/PA5 顺序或 DMA Stream2。
- **保守建议**：后续需要继续使用 CubeMX 时，优先由文本编辑器/其他 AI 在 `.ioc` 上做单字段对照；不要在当前 RC 版本中反复点击生成或覆盖工程。DAC 运行链路可继续由 `APP/DacOutput` 的用户代码维护。

## 13. 人工 CubeMX 配置的最高优先级修复项

对比可加载的原始 `AD9226_API.ioc` 与当前工程，当前文件新增的物理引脚条目为：

```text
Mcu.Pin106=PA4
Mcu.Pin107=PA5
```

这两个条目被直接追加到原有物理引脚列表末尾，再顺延虚拟引脚。当前日志崩溃发生在 TFBGA240 `BGAPackage$GBall` 引脚包映射阶段；而 DAC 通道、TIM6 TRGO 和 DMA 配置本身在其他可加载工程中存在。因此当前最高优先级判断是：**PA4/PA5 的 `.ioc` 物理引脚索引/包映射由文本追加产生了不兼容状态**。这是基于对照证据的经验判断，尚未在同一文件上完成 CubeMX 自动重排后的最终复验。

### 建议的人工操作

在 CubeMX 图形界面中重新选择：

1. `PA4` → `DAC1_OUT1`；
2. `PA5` → `DAC1_OUT2`；
3. DAC1 两通道触发源均选 `TIM6 TRGO`；
4. TIM6 主模式选择 `TRGO Update Event`；
5. DMA 选择 `DAC1_CH1`，保留 `DMA1_Stream2`、循环、Word/Word。

这样由 CubeMX 自己生成 `Mcu.Pin` 顺序，不要手工把 PA4/PA5 追加到 `Mcu.Pin106/107`。

## 14. 异常七最终复验：副本引脚映射修复 BGA 崩溃

### 14.1 已验证修复

将可加载副本中的以下结构合并到正式 `.ioc`：

- `Mcu.IP1=DAC1` 的 IP 顺序；
- `PA5=Mcu.Pin88`、`PA4=Mcu.Pin99` 的物理引脚顺序；
- `SH.COMP_DAC11_group` 和 `SH.COMP_DAC12_group` 共享信号映射。

合并后 CubeMX 6.17.0-RC5 可正常完成 TFBGA240 包、外设和 FreeRTOS 配置加载，未再出现 `BGAPackage$GBall`、`IndexOutOfBoundsException` 或级联 `NullPointerException`。第 13 节的“PA4/PA5 文本追加导致包映射不兼容”由经验判断升级为本工程内已验证结论。

### 14.2 生成结果

真实代码生成两次均出现：

```text
project generate
OK
exit
Bye bye
```

CubeMX 创建了 `Core/Src/dac.c` 和 `Core/Inc/dac.h`，并生成 DAC1 双通道、DMA1 Stream2、TIM6_DAC 和 DMA 中断代码。Debug 构建和链接通过，说明生成产物可用。

## 15. 异常八：生成成功但 cubemx-ai 返回失败

### 15.1 现象

`cubemx-ai 0.2.11 generate` 中 CubeMX 进程返回码为 0、日志包含 `OK`，目标文件也已创建或更新，但 CLI 最终返回 `ok=false`。误报来源包括：

1. 后置检查固定期待 `Core/Inc/freertos.h`，而当前 CMSIS-RTOS2 工程仅生成 `Core/Src/freertos.c`；
2. CubeMX 首页广告请求产生 `Unexpected HTTP response Code (200)` 和 `Failed to refresh advertisement data`；
3. 第二次生成时浏览器线程出现 `ConcurrentModificationException`，但生成主流程随后仍完成 `OK`，并成功更新 DAC/TIM 文件。

### 15.2 建议修复

1. 根据 `.ioc` 的 FreeRTOS/CMSIS-RTOS 生成模式决定是否要求 `freertos.h`，不要将其设为所有工程的固定必需文件。
2. 将首页广告、用户认证 WebView 等非代码生成网络错误单独归类为 warning。
3. 同时参考进程返回码、`project generate -> OK` 标记、目标文件更新时间和后续构建结果；不要仅因非生成线程异常把完整生成判为失败。
4. 仍需保留浏览器线程异常原文，但应将“生成结果是否可用”和“CubeMX 内部是否出现异常”拆成两个字段。

### 15.3 PhaseLockTask 生成时再次复现

在 `.ioc` 中增加 `PhaseLockTask` 后，CubeMX 主流程再次明确输出：

```text
project generate
OK
Bye bye
```

生成后的 `Core/Src/freertos.c` 已包含任务句柄、属性、创建调用和弱入口，后续 Debug 构建成功；但 `cubemx-ai` 仍因固定检查不存在的 `Core/Inc/freertos.h`，并混入首页广告网络错误而返回 `ok=false`。这说明异常八不是单次偶发，而是 CMSIS-RTOS2 工程可稳定复现的后置判定缺陷。

## 16. 异常九：gdb-eval默认字符集命令不受当前GDB支持

### 16.1 现象

`stm32-build-ai 0.1.17 gdb-eval` 默认发送：

```text
set host-charset UTF-8
set target-charset UTF-8
```

当前 `arm-none-eabi-gdb` 对两条命令均返回：

```text
Undefined item: "UTF-8".
```

变量读取、目标恢复和分离仍成功，CLI最终返回 `ok=true`。

### 16.2 建议修复

1. 启动时先查询 `show host-charset` 和可用字符集，或仅在用户显式指定时发送charset命令。
2. 将不支持的charset命令降级为结构化warning，并明确变量读取是否仍成功。
3. 不要让字符集设置错误掩盖真正的GDB错误；同时不要因该警告否定已成功的寄存器和变量读取。

## 17. 异常十：暂停式GDB读取会破坏实时DAC验收

### 17.1 现象

`stm32-build-ai gdb-eval`连接后执行`monitor halt`，本轮读取和恢复约需十余秒。CPU暂停期间TIM6与DAC DMA继续运行，PhaseLockTask无法补充半缓冲，恢复运行后立即出现`DMA_UNDERRUN`并停止DAC。

### 17.2 判断

- 这不是GDB变量读取结果错误，而是暂停式调试与连续实时输出的固有冲突。
- GDB读取前看到`UNDERRUN=0`、下一次读取看到`UNDERRUN=1`时，不能把后者当成固件自然运行证据。

### 17.3 建议

1. 锁相和DAC长期验收使用`COM3 @ 8000000`的`PLL_STATE/PLL_DAC`遥测，不在运行中暂停内核。
2. GDB只用于复位后静态变量、Fault或任务资源检查；需要读实时状态时，先明确该次读取会破坏DAC连续性。
3. 若CLI后续扩展无暂停遥测能力，应在命令帮助中明确标注`gdb-eval`的实时系统副作用。

## 18. 异常十一：成功标记顺序导致生成结果仍被判失败

### 18.1 现象

恢复TIM6为1.6 MS/s后，CubeMX进程返回码为0，日志明确包含`OK`、`project generate`、最终`OK/exit/Bye bye`，并且`Core/Src/tim.c`正确更新为`150-1`。但`cubemx-ai 0.2.11`报告`success_marker_found=false`，同时仍因缺少`Core/Inc/freertos.h`返回`ok=false`。

### 18.2 建议修复

1. 成功标记匹配应兼容`OK -> project generate`与`project generate -> OK`两种日志顺序。
2. 以进程返回码、生成文件差异、后续构建结果联合判断，不应由广告网络错误覆盖主流程成功状态。
3. CMSIS-RTOS2工程不应固定要求`Core/Inc/freertos.h`。

## 19. 异常十二：vcp-list枚举超时但直读正常

### 19.1 现象

`stm32-build-ai 0.1.17 vcp-list --json`在15秒后超时并返回空端口列表；同一时刻Windows `Win32_SerialPort`在约6秒内识别`COM3`为STLink Virtual COM Port，随后`vcp-read COM3 --baud 8000000`连续120秒读取60038字节且零读错误。

### 19.2 建议修复

1. 为设备枚举增加可配置超时，并在超时时保留已发现的部分端口。
2. Windows路径可在WMI/CIM、SetupAPI和pyserial枚举之间设置有界回退。
3. 枚举失败不应阻止用户对已知端口执行`vcp-read`，CLI应明确给出该替代路径。

## 20. 异常十三：同一任务内 PowerShell 的 Git PATH 解析不稳定

### 20.1 事故现场（2026-07-25）

- 本轮开始时 `git status/diff` 能正常执行；后续同一工作目录、同一任务中再次调用时，PowerShell 返回 `The term 'git' is not recognized`。
- `stm32-build-ai env-check` 同时仍能找到 CMake、Ninja 和 Arm GNU 工具链，工程构建不受影响。
- 改用 `C:\Users/<user>\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\git\cmd\git.exe` 绝对路径后，`diff/status/log` 正常。

### 20.2 建议修复

1. Codex shell 会话应保持稳定的 Git PATH，或在每次 shell 调用前统一注入 bundled Git 目录。
2. 工具层检测到 `git` 解析失败时，应回退到已注册的 bundled Git 绝对路径，并返回结构化 warning。
3. 不要把单个并行子命令的 PATH 失败吞掉其他已完成命令的输出，便于保留完整事故现场。
