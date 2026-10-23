# H743_UI_Instrument —— 带仪器 UI 框架的模板

基于 H743_LVGL_Baseline(已验证编译烧录),2026-07-26 生成。增量内容全部围绕"仪器 UI 快速接入":

```
在 Baseline 之上新增:
├── UI\Designer\                 LVGL Editor 设计器工程(lib-ui 库,六屏框架)
│   ├── project.xml / screens\*.xml     ← 设计器源(用 LVGL Editor 打开编辑)
│   ├── *_gen.c/h                        ← Editor 生成代码(勿手改)
│   └── CMakeLists + file_list_gen.cmake ← 文件清单由 Editor 维护,Generate 后免手工同步
├── UI\Custom\app_instrument_screen.c/h  手写动态仪器屏资产(结果页+波形详情页)
├── UI\pages\app_lvgl_ui.c               Demo Scope 组装页(波形 chart + 屏幕导览)
└── APP\RTOS\app_demo_data*.{c,h}        演示数据链路(波形任务→mutex 双缓冲→UI)
```

## 一、上电现象(验收标准)

Demo Scope 屏:模拟波形滚动(频率扫动+幅度呼吸+噪声)、freq/vpp/frame 标签刷新;底部 7 个按钮导览 Designer 六屏(Spectrum/Monitor/Control/System/Test/Calib)与手写 Scope+ 屏,目标屏左上角自动出现 [<] 返回钮(运行时贴上,不动生成代码)。

## 二、新项目开工(在 Baseline 流程上多一步 UI 说明)

1. 整目录复制改名(路径勿带空格);改 `CMakeLists.txt` 的 `CMAKE_PROJECT_NAME`;`.ioc` 改名+内部 `ProjectManager.ProjectName/ProjectFileName` 同步。
2. 构建烧录同 Baseline:
   ```powershell
   cmake --preset Debug ; cmake --build --preset Debug -j8
   ```
   (在工程目录内执行;或 build 步用 `cmake --build <工程>/build/Debug -j8`)
3. `git init` 并提交首个基线。

## 三、UI 迭代工作流(核心)

**改界面走设计器,不手写布局**:
1. 用 LVGL Editor 打开 `UI\Designer\project.xml`
2. 增删屏幕、拖控件、改样式(现有六屏就是可改模板;PC 端可预览)
3. Generate Code → 重新编译即可(file_list_gen.cmake 由 Editor 自动维护)

**动态数据页走 adapter 模式**(本模板已示范最小闭环):
```
生产任务(app_demo_data_task, 覆盖 MainTask)
   └─ AppDemoData_Publish(frame)          ← mutex 双缓冲,1KB 帧 12.5Hz
DisplayTask(App_LvglUiRun 内)
   └─ AppDemoData_TryFetch(&frame) → lv_chart_set_series_ext_y_array 刷新
```
接真实数据:保留 `app_demo_data.{c,h}` 接口,把 `app_demo_data_task.c` 的波形生成段换成真实采集(如 AD9226 帧回调里 Publish);高吞吐时按注释换 osMessageQueue/信号量双缓冲(参考 signal_separation 的 AdcFrameReadySem 模式)。

**复杂动态屏(波形/频谱细节)走手写屏**:参考 `UI\Custom\app_instrument_screen.c`(稳定对象命名 + 运行时替换 Designer 屏的模式,见 signal_separation 的 app_lvgl_ui.c 原版与其 app_ui_adapter)。

## 四、命名说明

Designer 生成库沿用 `signal_separation_ui` 前缀(库名 lib-ui 不变)。要改名:在 LVGL Editor 中改项目名并 Generate(Editor 会重命名全部 gen 文件),然后同步 `UI\pages\app_lvgl_ui.c` 里 `#include "signal_separation_ui.h"` 与 `signal_separation_ui_set_target/init` 两处调用、`UI\Custom\app_instrument_screen.c` 里一处 include。不改也不影响使用。

## 五、与 Baseline 的关系及注意

- Baseline 的全部说明(预装 BSP/debug_io/memory_map、CubeMX 重生成规则、Function 模块接入模板、PB3/SWO 限制)对本模板同样有效,不再重复。
- freertos.c 的 MainTask 在本模板被 `app_demo_data_task.c` 覆盖(演示数据源);删除演示链路时把这三个文件从 CMakeLists 去掉即可回到 Baseline 行为。
- `UI\Designer\preview-build`、`preview-bin` 是 Editor 的 PC 预览缓存,模板未携带;Editor 首次预览会自动重建。
- 六屏为 signal_separation 的界面框架(校准/频谱/监控等),新项目按需在 Editor 里删改;Monitor 屏在 signal_separation 中运行时被手写屏替换,本模板保留其 Designer 原版用于浏览。
