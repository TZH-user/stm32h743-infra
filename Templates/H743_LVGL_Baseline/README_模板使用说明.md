# H743_LVGL_Baseline —— 母板工程(基线)

从 signal_separation(2026-07 最新架构)裁剪的最小可跑底座,2026-07-26 生成。
硬件:STM32H743 + LTDC RGB 屏 + GT9xx 触摸 + SDRAM + ST-LINK VCP(USART1)。
软件:HAL + FreeRTOS(CMSIS-RTOS2)+ LVGL 9.5。上电即点亮初始页面(标题/运行秒数/触摸计数),串口输出启动信息。

## 一、新项目开工流程(目标 10 分钟)

1. 整目录复制 `F:\STM32\Templates\H743_LVGL_Baseline` 到工作区并改名(路径不要带空格)。
2. 改两处工程名:
   - `CMakeLists.txt` 里 `set(CMAKE_PROJECT_NAME H743_LVGL_Baseline)` → 新名
   - `H743_LVGL_Baseline.ioc` 改文件名,并把文件内 `ProjectManager.ProjectFileName/ProjectName` 两行同步改名
3. 构建(VS Code CMake 插件选 Debug preset,或命令行):
   ```powershell
   cmake --preset Debug
   cmake --build --preset Debug -j8
   ```
   产物:`build\Debug\<工程名>.elf/.hex/.bin`(POST_BUILD 自动生成)。
4. 烧录(STM32_Programmer_CLI 或 CubeProgrammer GUI):
   ```powershell
   & "C:\Users/<user>\AppData\Local\stm32cube\bundles\programmer\2.22.0+st.1\bin\STM32_Programmer_CLI.exe" -c port=SWD mode=UR -d "build\Debug\<工程名>.elf" -v -rst
   ```
5. `git init && git add -A && git commit -m "从 H743_LVGL_Baseline 起步"`。

## 二、预装内容

| 位置 | 内容 | 说明 |
|---|---|---|
| Core\ | CubeMX 生成(main/gpio/dma/fmc/ltdc/usart/tim/dac/quadspi/freertos)+ lv_conf.h | main.c 仅额外调用 DebugIO_Init;freertos.c 已清业务引用,任务定义保留 |
| BSP\LTDC、SDRAM、Touch | 显示三件套(同 Function\Hardware 对应模块) | 不用屏幕:删目录 + 注释 CMakeLists 对应组 |
| UI\ | app_lvgl(初始化/主循环)+ Port(disp/indev)+ pages\app_lvgl_ui(模板页) | 换页面:改 pages\app_lvgl_ui.c,保持 Init/Run 接口 |
| APP\Debug\ | debug_io(vcp_printf,DMA 环形缓冲,独立实现) | `vcp_printf("...")` 即串口打印;PB3 被占用,SWO 不可用,SWD 正常 |
| APP\Config\app_memory_map.h | SDRAM 显存/缓冲统一分配表 | 新增大缓冲区先看这里,避免与帧缓冲重叠 |
| APP\RTOS\app_display_task.c | 覆盖 __weak 的显示/触摸任务 | 业务任务另建文件覆盖对应 weak 符号 |

.ioc 里预定义的 RTOS 对象:DisplayTask/TouchTask/PrintfTask/MainTask/PhaseLockTask/SerialControlTask、PrintQueue、AdcFrameReadySem/SerialRxSem。未覆盖的任务是 __weak 空转,不占资源可留作扩展位;确不需要可在 CubeMX 里删除后重新生成。

## 三、接入 Function 模块(硬件/算法)

模块库:`F:\STM32\stm32project\Function\`(Hardware 13 个硬件模块,Algorithm 9 个算法组)。

1. 把模块目录复制进工程(目标路径见 CMakeLists 注释,如 DDS_AD9959 → `APP\DDS_9959\`)。
2. 在 `CMakeLists.txt` 中取消该模块的 target_sources / target_include_directories 注释行(已成组写好,搜索模块名)。
3. 在 CubeMX 中配置该模块所需外设(SPI/I2C/TIM/QSPI 等,参照模块头文件里的句柄引用),Generate Code。
4. 需要 FFT/算法库时按 CMakeLists 中 CMSIS-DSP 段与 Algorithm 段的注释操作。

约定:项目期间随便改复制进来的模块;**结项时 diff 一次,把有价值的驱动改动回流 Function**,库才不腐化。

## 四、CubeMX 重新生成注意

- 本工程用户代码全部位于 USER CODE 区之外的独立目录(BSP/APP/UI),Generate Code 安全。
- freertos.c 的 USER CODE 区当前为空(业务引用已清),重生成后无需手工恢复。
- 若在 CubeMX 里改了外设,重生成后只需检查 cmake\stm32cubemx\CMakeLists.txt 是否新增 HAL 源文件(CubeMX 自动维护)。

## 五、已知边界

- PB3 在源工程被 AD9226_D8 占用,SWO 打印不可用(vcp_printf 走 USART1 VCP)。若新板卡无此约束,可在 CubeMX 释放 PB3。
- QSPI/DAC/TIM2 外设初始化保留(main.c),未接驱动时空闲无害;不需要可在 CubeMX 删除。
- Drivers\CMSIS\DSP 未随模板携带(减重 5000+ 文件);需要时按 CMakeLists 注释清单从旧工程拷入。
- LVGL 目录已裁剪 demos/examples/tests/docs(不参与编译)。
