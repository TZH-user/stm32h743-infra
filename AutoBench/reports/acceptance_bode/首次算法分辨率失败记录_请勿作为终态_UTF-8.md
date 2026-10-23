# 首次模拟幅频失败记录（非终态）

> 编码：UTF-8

该目录保留首次 20 点模拟验收现场。失败原因是低频模拟记录只覆盖少量周期，导致 FFT
频率插值误差超过 200 ppm；硬件访问和输出改变字段均为 `false`。

已通过最小修正（默认模拟采样率按频率自适应）并在以下目录复验通过：

- `F:\STM32\AutoBench\reports\acceptance_bode_rerun`
- `F:\STM32\AutoBench\reports\final_bode`

最终交付请以后两者为准。本文件保留失败现场，不执行删除。

