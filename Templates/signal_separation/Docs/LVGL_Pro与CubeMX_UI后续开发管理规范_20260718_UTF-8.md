# LVGL Pro 与 CubeMX UI 后续开发管理规范

> 文件编码：UTF-8  
> 建立日期：2026-07-18  
> 适用主工程：`F:/STM32/stm32project/VSCodeCMAKEProject/signal_separation`  
> 当前 MCU：STM32H743XIHx  
> 当前 UI 分辨率：800×480，RGB565  
> 当前 LVGL：9.5.0  
> 当前 `.ioc`：CubeMX 6.17.0、STM32Cube FW_H7 V1.13.0、CMake 工具链

## 1. 核心结论

LVGL Pro 和 CubeMX 不是互相生成关系，而是两条独立生产线：

- CubeMX 负责 MCU 硬件和运行骨架，包括时钟、引脚、LTDC、SDRAM、I2C、DMA、
  中断、FreeRTOS 对象和 HAL 初始化代码；
- LVGL Pro 负责 UI 设计源、页面、组件、样式、Subjects 和 LVGL 对象创建代码；
- CMake 把 CubeMX 生成库、LVGL 9.5.0、LVGL Pro 生成库和用户业务代码链接成一个固件；
- `UI/Port` 把 LVGL 对接到真实显示和触摸硬件；
- `UI/Adapter` 负责业务数据和 UI 控件之间的双向交接。

推荐长期采用“一个硬件主工程 + 多个独立 UI 模板包”的管理方式。模板克隆工程只用于
设计、预览和板级验证，不应各自长期维护一套 ADC、DMA、串口和算法主线，否则修复会逐渐分叉。

## 2. 两条生成链及连接点

```text
CubeMX (.ioc)
  -> Core / Drivers / cmake/stm32cubemx / RTOS对象
  -> LTDC + SDRAM + Touch + FreeRTOS基础能力
                                      \
                                       -> CMake -> signal_separation.elf
                                      /
LVGL Pro (XML/project.xml)
  -> *_gen.c/h + file_list_gen.cmake -> lib-ui
  -> 页面、组件、样式、Subjects

业务采集/算法 -> UI/Adapter -> LVGL对象
LVGL事件       -> UI/Adapter -> 命令队列/业务接口
LVGL绘制       -> UI/Port    -> SDRAM framebuffer -> LTDC -> 屏幕
```

两者不会自动发现彼此。当前显式连接位置如下：

| 连接内容 | 当前文件 | 责任 |
| --- | --- | --- |
| 引入 LVGL 9.5.0 | `CMakeLists.txt` | 构建 `lvgl` 静态库 |
| 引入 LVGL Pro 项目 | `CMakeLists.txt` 的 `add_subdirectory(UI/Designer)` | 构建 `lib-ui` |
| 显示和触摸端口 | `UI/Port/` | 注册 display、flush、indev |
| LVGL 初始化和周期运行 | `UI/app_lvgl.c` | `lv_init()`、端口初始化、`lv_timer_handler()` |
| 页面入口 | `UI/pages/app_lvgl_ui.c` | 设置 target、初始化生成 UI、加载首页 |
| 业务数据交接 | `UI/Adapter/app_ui_adapter.c/.h` | 快照、FFT、Subjects、Chart 刷新 |
| UI 所在线程 | `APP/RTOS/app_display_task.c` | DisplayTask 独占调用 LVGL |

## 3. 文件所有权与修改边界

### 3.1 CubeMX 管理区

| 路径 | 是否允许直接修改 | 规则 |
| --- | --- | --- |
| `signal_separation.ioc` | 通过 CubeMX/cubemx-ai 修改 | 修改后 validate、firmware-check、dry-run |
| `Core/Src/*.c`、`Core/Inc/*.h` | 仅 USER CODE 区 | 非 USER CODE 区可能被重新生成覆盖 |
| `Core/Src/freertos.c` | 优先由 CubeMX 修改任务表 | 任务实现放独立用户文件，不把业务塞入弱函数正文 |
| `cmake/stm32cubemx/` | 不手工维护业务逻辑 | CubeMX 生成源文件清单和底层目标 |
| 顶层 `CMakeLists.txt` | 允许维护 | 文件头已声明只生成一次；UI/业务集成放这里或拆到用户 cmake 文件 |

当前 `app_display_task.c` 通过强符号覆盖 `freertos.c` 中的 `__weak StartDisplayTask()`，
这是可保留的 CubeMX 再生成避让方式。重新生成后仍要检查弱函数和任务名没有变化。

### 3.2 LVGL Pro 管理区

| 文件类型 | 所有者 | 规则 |
| --- | --- | --- |
| `UI/Designer/project.xml` | LVGL Pro 设计源 | 保存 LVGL 版本、target 和分辨率 |
| `UI/Designer/globals.xml` | LVGL Pro 设计源 | 全局颜色、样式、Subjects 等 |
| `UI/Designer/screens/*.xml` | LVGL Pro 设计源 | 页面结构的长期唯一来源 |
| `UI/Designer/components/*.xml` | LVGL Pro 设计源 | 可复用组件的长期唯一来源 |
| `*_gen.c/.h` | LVGL Pro 自动生成 | 禁止长期手改；Generate 后可能覆盖 |
| `file_list_gen.cmake`、`component_lib_list_gen.cmake` | LVGL Pro 自动生成 | 不手改 |
| `UI/Designer/CMakeLists.txt` | LVGL Pro 自动生成 | 不手改；用户源放 `user_config.cmake` |
| 不带 `_gen` 的 UI 文件 | 用户扩展点 | 先确认 LVGL Pro 是否保留，再写自定义逻辑 |

### 3.3 用户稳定区

| 路径 | 内容 |
| --- | --- |
| `UI/Port/` | 显示、触摸、缓存一致性、颜色格式和真实硬件适配 |
| `UI/Adapter/` | UI 与采集/算法/控制命令的数据契约 |
| `UI/Custom/` | 暂时无法由 LVGL Pro 稳定表达的手写页面或组件 |
| `UI/pages/` | 应用入口、页面注册和模板选择 |
| `APP/Config/` | 分辨率、SDRAM 地址、刷新周期、功能开关和资源预算 |
| `APP/RTOS/` | 任务实现、线程间快照、队列和业务调度 |
| `Docs/` | 模板清单、接口契约、性能基线、验收记录和回滚说明 |

## 4. 当前工程必须正视的预览差异

当前 `screen_monitor_gen.c` 和 `screen_spectrum_gen.c` 在函数入口直接调用：

```c
return AppInstrumentScreen_Create();
return AppInstrumentDetailScreen_Create();
```

LVGL Pro 生成的原页面被保留在 `#if 0` 中。因此当前状态实际是：

- LVGL Pro 预览主要反映 XML/生成基线；
- 板端 Monitor 和 Full 页面实际来自 `UI/Custom/app_instrument_screen.c`；
- 再次 Generate 可能覆盖这两个跳转点；
- 只修改 XML 不一定改变板端最终画面，只修改 Custom 也不会同步到 LVGL Pro 预览。

这正是“电脑预览与 STM32 实际 UI 不一致”的主要技术债之一。

短期规则：切换或更新当前示波器模板时，`Designer` 和对应 `Custom` 必须作为同一个模板包
同步，并在 Generate 后复查两个入口。

长期目标：把生产布局完整迁回 XML，以 LVGL Pro 预览作为页面结构唯一真源；`Custom`
只保留 LVGL Pro 不适合生成的行为和控件扩展，禁止继续在 `_gen.c` 中维护永久补丁。

## 5. UI 数据接口规范

### 5.1 线程规则

1. 只有 DisplayTask 可以创建、删除或修改 LVGL 对象。
2. MainTask、ADC/DMA 回调和算法任务禁止直接调用 LVGL。
3. DMA ISR 只发通知或更新轻量状态，不做格式化、FFT、对象查找和页面切换。
4. 业务任务通过快照、队列或双缓冲把数据交给 `UI/Adapter`。
5. Adapter 在 DisplayTask 中读取快照并批量刷新控件。

当前调用链：

```text
MainTask
  -> AppUiAdapter_SubmitAdcFrame()      复制 128 点，不调用 LVGL

DisplayTask，每 5 ms
  -> App_LvglRun()
     -> App_LvglUiRun()
        -> AppUiAdapter_Run()           每 50 ms 轮询，页面切换后重新绑定
     -> lv_timer_handler()

LVGL 默认刷新周期：33 ms
```

### 5.2 数据类型对应方式

| 数据类型 | 推荐交接方式 | 当前示例 |
| --- | --- | --- |
| 状态、数字、短文本 | LVGL Subject | `analysis_state`、`peak_frequency`、`sample_rate` |
| 波形、频谱、瀑布图 | Adapter 快照/双缓冲后批量写 Chart | 128 点 `waveform` / 64 点 `spectrum` |
| 按钮、下拉框、开关命令 | LVGL event -> Adapter -> 命令队列 | FFT/波形下拉框当前只改 UI 模式 |
| 页面导航 | 页面注册函数或事件回调 | Monitor、Control、Results、System |
| 高频原始数据 | 不直接绑定 Subject | 先抽取、聚合或降低刷新率 |

### 5.3 模板通用对象名契约

所有标记为“可接真实数据”的模板至少提供以下稳定对象名；Adapter 按对象名查找，
禁止依赖 `lv_obj_0`、`lv_label_3` 等生成序号：

| 对象名 | 类型 | 用途 | 必需性 |
| --- | --- | --- | --- |
| `ui_chart` | `lv_chart` | 波形/频谱主图 | 波形模板必需 |
| `ui_mode_dropdown` | `lv_dropdown` | FFT/实时波形选择 | 双模式模板必需 |
| `ui_title` | `lv_label` | 当前显示模式标题 | 推荐 |
| `ui_peak_frequency` | `lv_label` | 峰值频率 | 频谱模板推荐 |
| `ui_peak_level` | `lv_label` | 峰值幅度 | 推荐 |
| `ui_spectrum_info` | `lv_label` | 点数、带宽、分辨率说明 | 推荐 |

对象名或 Subject 名变更属于接口变更，必须同时修改 Adapter、模板清单和验收记录，
不能只在 LVGL Pro 中随意重命名。

### 5.4 UI 到业务的控制命令

后续 RUN、STOP、量程、触发、采样率等真实控制不得直接在事件回调中操作 ADC/DMA。
统一采用：

```text
LVGL事件 -> UI/Adapter 生成 AppUiCommand -> RTOS消息队列
         -> MainTask/控制任务执行 -> AppUiStatus 快照 -> UI/Adapter 回显
```

命令结构至少包含：命令类型、参数、请求序号；状态结构至少包含：运行状态、错误码、
实际采样率、帧序号。这样切换模板时只换控件绑定，不改变采集主线。

## 6. 模板库管理规范

### 6.1 模板分级

| 等级 | 含义 | 是否可直接进入主工程 |
| --- | --- | --- |
| P0 预览模板 | 只保证 LVGL Pro 中视觉完整，允许静态假数据 | 否 |
| P1 接口模板 | 满足对象名/Subject 契约，可接 Adapter | 可进入 PC/编译验证 |
| P2 板级模板 | 通过 800×480 真机、触摸、内存和刷新验收 | 可作为候选模板 |
| P3 发布模板 | 与指定业务版本绑定并有 HEX、截图、性能记录 | 可作为正式版本 |

当前独立目录建议定位为模板研发沙箱：

- `F:/STM32/stm32project/VSCodeCMAKEProject/UI/SignalUI_Oscilloscope`
- `F:/STM32/stm32project/VSCodeCMAKEProject/UI/SignalUI_RF_Spectrum`
- `F:/STM32/stm32project/VSCodeCMAKEProject/UI/SignalUI_VNA_Preview`

其中 `signal_separation` 是实际硬件主工程，不再从模板克隆工程反向覆盖 `.ioc`、BSP、
ADC、DMA、RTOS 或算法文件。

### 6.2 每个模板必须具备的清单

每个模板目录建议增加 `UI_TEMPLATE_MANIFEST_UTF-8.md`，至少记录：

```text
template_id:
display_name:
template_version:
lvgl_version:
resolution/color_depth:
status: P0/P1/P2/P3
designer_source:
custom_source:
adapter_contract_version:
required_object_names:
required_subjects:
supported_data: preview/waveform/fft/control
minimum_lvgl_heap:
linked_flash_baseline:
internal_ram_baseline:
measured_cpu_load:
known_limitations:
preview_image:
last_verified_firmware_commit:
```

### 6.3 推荐的长期目录结构

当前先保持 `UI/Designer` 作为活动模板槽。模板数量稳定后，建议再进行一次独立重构：

```text
UI/
  Templates/
    oscilloscope/
      Designer/
      Custom/
      UI_TEMPLATE_MANIFEST_UTF-8.md
    rf_spectrum/
      Designer/
      Custom/
      UI_TEMPLATE_MANIFEST_UTF-8.md
    vna/
      Designer/
      Custom/
      UI_TEMPLATE_MANIFEST_UTF-8.md
  Adapter/                 通用业务接口
  Port/                    唯一硬件端口
  pages/                   唯一应用入口
```

CMake 每次只选择一个模板目录，避免复制整套 CubeMX 工程。该重构必须单独备份、修改
CMake 并做三套模板构建矩阵验证，本规范阶段不直接实施。

## 7. 当前架构下的模板切换流程

1. 在模板沙箱内用 LVGL Pro 完成设计和 PC 预览，不先覆盖主工程。
2. 确认 `project.xml` 为 LVGL 9.5.0、800×480，并生成代码成功。
3. 填写模板 Manifest，明确是 P0 预览还是 P1/P2 数据模板。
4. 关闭 LVGL Pro，逐文件备份主工程 `UI/Designer`、匹配的 `UI/Custom`、CMake 和记录文件。
5. P0 模板只保存预览，不接入主工程；P1 以上才同步到活动模板槽。
6. 只同步 UI 包，不复制模板工程的 `.ioc`、Core、Drivers、BSP、APP 或 CMake 主线。
7. 若模板满足通用对象名契约，保留主工程 `UI/Adapter`；不满足时先做适配评审，
   不允许用静态假数据冒充已接入真实采集。
8. 当前仍依赖 Custom 的模板必须同时同步 Designer 和 Custom，并复查
   `screen_monitor_create()`、`screen_spectrum_create()` 的实际入口。
9. 严格解析所有 XML，检查生成文件清单，不通过删除目录解决旧文件问题。
10. 执行 CMake configure/build、size、objcopy，记录 FLASH、各 RAM 区和符号。
11. 在板端验证首页、全部导航、返回路径、触摸、真实波形、FFT/波形切换和连续运行。
12. 保存板端照片或视频、最终 HEX 哈希、性能记录并把模板升级到对应等级。

## 8. CubeMX 再生成流程

当前 `.ioc` 声明 CubeMX 6.17.0，而本机可用 CubeMX 为 6.18.0。使用新版本打开或生成前，
必须先备份 `.ioc` 和所有可能被生成的文件，并注意版本升级可能改变生成结果。

规范流程：

1. `cubemx-ai inspect signal_separation.ioc --json`
2. 对修改操作先 dry-run。
3. `validate` 并核对 MCU/CPN。
4. `firmware-check` 确认 FW_H7 V1.13.0 可用。
5. `generate --dry-run --json` 查看命令和影响范围。
6. 用户授权后才执行真实 Generate。
7. Generate 后检查：任务名/栈、弱函数、LTDC 地址、SDRAM、MPU、触摸中断、
   `cmake/stm32cubemx` 源文件清单。
8. 检查顶层 CMake 的 LVGL、Designer、Adapter、Custom 链接仍存在。
9. 重新 configure/build/size/objcopy，不直接沿用旧 HEX。

## 9. 当前资源消耗基线

### 9.1 固定显示内存

依据 `APP/Config/app_memory_map.h`：

| 项目 | 地址/配置 | 容量 | 说明 |
| --- | --- | ---: | --- |
| LTDC framebuffer | `0xC0000000` | 768000 B（750 KiB） | 800×480×RGB565 |
| LVGL draw buffer | `0xC0200000`，40 行 | 64000 B（62.5 KiB） | 单缓冲、PARTIAL 模式 |
| LVGL heap | `0xC0400000` | 65536 B（64 KiB） | LVGL 对象和运行分配上限 |
| UI 固定外部内存合计 | 上述三项 | 897536 B（876.5 KiB） | 不含地址间空洞和 SDRAM 测试区 |

`APP_SDRAM_TEST_SIZE_BYTES` 另占用 256 KiB 测试区域，但它不属于 LVGL 日常对象堆。

### 9.2 当前 Debug 链接结果

最终构建：

| 区域 | 使用量 | 容量 | 占用率 |
| --- | ---: | ---: | ---: |
| FLASH | 777928 B | 2 MiB | 37.09% |
| DTCMRAM | 92344 B | 128 KiB | 70.45% |
| RAM_D2 | 20 KiB | 288 KiB | 6.94% |

依据最终 `.map` 按对象来源统计：

| 来源 | 链接 FLASH | 链接内部静态 RAM | 说明 |
| --- | ---: | ---: | --- |
| LVGL 9.5.0 核心 | 519274 B | 728 B | 不含外部 64 KiB LVGL heap |
| UI Port/Adapter/Custom/pages | 7633 B | 2453 B | 当前用户 UI 连接层 |
| LVGL Pro `lib-ui` 实际链接部分 | 1957 B | 3489 B | 未使用生成函数被链接器回收 |
| CMSIS-DSP FFT 子集 | 124084 B | 0 B | 当前 128 点 UI FFT 的主要代码依赖 |
| `libm` 数学函数 | 1808 B | 0 B | `cosf/log10f/sqrtf` 等 |

这些数值是当前 Debug 固件的链接归属，不等同于“删除某一文件就一定能全部省下”的增量值，
因为库函数之间存在共享依赖。但它足以说明当前 FLASH 大头是 LVGL 核心和 UI FFT 依赖，
而不是 XML 页面本身。

### 9.3 任务与刷新配置

| 项目 | 当前值 | 位置 |
| --- | ---: | --- |
| DisplayTask 栈 | 16384 B | `Core/Src/freertos.c` |
| TouchTask 栈 | 2048 B | `Core/Src/freertos.c` |
| FreeRTOS 动态堆 | 40960 B | `Core/Inc/FreeRTOSConfig.h` |
| DisplayTask 循环 | 5 ms | `APP/RTOS/app_display_task.c` |
| LVGL 默认刷新周期 | 33 ms，约 30 Hz | `Core/Inc/lv_conf.h` |
| Adapter 轮询周期 | 50 ms，最高 20 Hz | `UI/Adapter/app_ui_adapter.c` |
| 主页面波形快照 | 128 点 | `UI/Adapter/app_ui_adapter.c` |
| UI FFT | 128 点输入、64 点显示 | `UI/Adapter/app_ui_adapter.c` |

2026-07-17 的旧页面实测中 DisplayTask 最大使用约 4836 B，但该测量早于当前示波器 UI
和 UI FFT 接入，只能作为历史参考，不能作为当前模板的最终栈结论。

### 9.4 尚未获得的真实数据

当前还没有对本模板重新实测：

- DisplayTask CPU 占用；
- LVGL heap 峰值、最大连续空闲块和碎片率；
- 当前页面/触摸操作后的 DisplayTask 栈高水位；
- 频繁切页、动画和 20 Hz Chart 更新时的最坏刷新时间；
- LTDC 带宽、DCache 清理和局部 memcpy 的峰值耗时。

`LV_USE_PERF_MONITOR` 和 `LV_USE_MEM_MONITOR` 当前默认关闭。需要测量时应建立临时诊断构建，
或在 DisplayTask 中周期调用 `lv_mem_monitor()`，配合 FreeRTOS 栈高水位与运行时间统计。
诊断功能不得默认长期显示在正式仪器界面上。

## 10. 新模板性能门槛

以下是当前 H743/800×480 工程的建议验收线，属于工程预算，不是 LVGL 的绝对限制：

1. LVGL heap 峰值使用不高于 70%，最大连续空闲块能覆盖一次最大页面创建需求。
2. LVGL heap 碎片率长期运行后建议低于 15%。
3. DisplayTask 栈最小剩余量至少 4096 B；未实测前保持 16 KiB 配置。
4. 主 Chart 常规刷新建议 10~20 Hz；仪器数值标签无必要时不高于 10 Hz。
5. Overview Chart 建议 128~256 点；更高点数先抽取再显示，算法仍可使用完整原始帧。
6. 切页后不允许对象持续累积；连续切换 100 次后 heap 使用应回到稳定区间。
7. 不允许因为 UI 更新导致 ADC DMA 溢出、串口丢帧或高优先级任务超时。
8. P2/P3 模板必须记录静态 size、运行 heap、任务栈、CPU/刷新时间和连续运行结果。

## 11. 文档和记录放置规范

| 内容 | 建议文件 |
| --- | --- |
| 总体 UI/CubeMX 管理规则 | 本文件 |
| 每个模板能力和依赖 | 模板目录 `UI_TEMPLATE_MANIFEST_UTF-8.md` |
| 数据字段、单位、刷新率、命令 | `Docs/UI数据与控制接口契约_UTF-8.md` |
| 每次构建静态资源 | `Docs/UI性能与内存基线_YYYYMMDD_UTF-8.md` |
| 真机 CPU/heap/栈/刷新实测 | 同一性能基线文档的“运行实测”章节 |
| 模板切换和同步文件 | `文件改动记录_UTF-8.md` |
| 为什么采用某种架构 | `方案改动记录_UTF-8.md` |
| LVGL Pro PC 预览 | 模板目录下带日期 PNG |
| 板端验收照片/视频 | 模板 `Evidence_UTF-8/`，并在 Manifest 引用 |
| 可烧录产物 | `build/<Preset>/`，记录 HEX SHA256，不把 build 当设计源 |

性能数据必须注明是“配置容量”“静态链接结果”还是“真机运行实测”，三者禁止混写。

## 12. 发布前检查清单

### LVGL Pro

- XML 可解析，Generate 无错误；
- 预览 target、分辨率、颜色和板端一致；
- 无过暗文字、越界控件、不可点击遮挡；
- P1 以上模板满足稳定对象名和 Subjects 契约；
- `_gen.c/.h` 没有未记录的手工补丁。

### CubeMX/CMake

- `.ioc` inspect/validate 通过；
- 顶层 CMake 只选择一个活动模板；
- `lib-ui`、`lvgl`、Port、Adapter、Custom 的链接关系正确；
- configure/build/size/objcopy 全部成功；
- `.map` 中 framebuffer、任务栈和关键 UI 符号位置正确。

### 真机

- 开机、首页、全部页面导航和返回正常；
- 触摸无偏移、无死区、无遮挡；
- 显示的是实时数据或明确标注的 Preview，不允许混淆；
- 波形/FFT 切换、页面切换后 Adapter 能重新绑定；
- 无栈溢出、malloc 失败、HardFault、DMA 错误和串口数据破坏；
- 记录 heap、栈、CPU/刷新时间和连续运行时长；
- 固件 HEX 哈希、模板版本和主工程提交能够互相追溯。

## 13. 建议推进顺序

1. 先把本规范作为当前主工程的管理基线。
2. 为 Oscilloscope、RF Spectrum、VNA 三个模板补齐 Manifest，并明确 P0/P1/P2 等级。
3. 建立 `UI数据与控制接口契约`，先固定对象名、Subjects 和命令结构。
4. 解决当前 `Custom` 绕过 XML 导致的预览/板端差异，使 XML 成为布局唯一真源。
5. 再做多模板 CMake 选择机制，避免以后继续复制整个 CubeMX 工程。
6. 最后增加只在诊断构建启用的 UI heap、栈、CPU 和刷新时间采集。

执行这六步后，UI 设计、AI 实现、板端显示、数据接入和资源预算才会形成稳定闭环。
