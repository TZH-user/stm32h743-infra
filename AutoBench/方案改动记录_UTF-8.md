# AutoBench 方案改动记录

> 编码：UTF-8

## 2026-07-27：任务书到离线实施方案

原始任务书只读位置：

`C:\Users/<user>\AppData\Local\Temp\claude\F--MCP\5fbeff0e-c498-4e64-9167-c0f255923d5a\scratchpad\tasks\任务书_AutoBench_仪器自动测量台.md`

本轮未修改原任务书，也没有权限在其目录创建备份。实施差异：

| 原计划 | 当前实施 | 原因 |
|---|---|---|
| 开工先探测 TCP/IDN | 完全禁止探测 | 仪器下电，用户明确禁止网络/SCPI |
| 运行结束输出 OFF | 离线阶段不含任何输出控制 | OFF 也是状态写入；本阶段从未 ON |
| AutoBench 内复制 DS4014E 驱动 | 保持 Toolkit 为唯一驱动源 | 避免重复造驱动与双真源漂移 |
| scope 直接采集 | schema 保留，执行前硬门禁 | 硬件统一待上电验证 |
| 真实直通 T2/T3 | 模拟 + board_vcp 验收 | 不伪造硬件通过 |
| 修改顶层 `.gitignore` | 明确不修改 | 当前任务禁止写该文件 |

详细方案见 `Docs\离线阶段实施方案_20260727_UTF-8.md`。

