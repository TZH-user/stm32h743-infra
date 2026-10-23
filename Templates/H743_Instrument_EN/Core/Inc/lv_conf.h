#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>
#include "app_memory_map.h"

/*
 * 当前工程的第一轮目标是：
 * 1. 在 H743 + FreeRTOS + RGB565 + 800x480 下先跑通 LVGL 9.5.0
 * 2. 先只接显示，不接触摸，不接 DMA2D
 * 3. 先让最小 UI 能稳定显示，再逐步扩展
 *
 * 因此这里先保留“最小可用配置”，其余选项交给 LVGL 默认值补齐。
 */

/*====================
 * Color settings
 *====================*/

/* 当前屏幕链路是 RGB565，因此 LVGL 也统一使用 16-bit 颜色深度。 */
#define LV_COLOR_DEPTH 16

/*=========================
 * Stdlib wrapper settings
 *=========================*/

/*
 * 第一轮先使用 LVGL 内建内存实现，避免一开始就把外部 SDRAM 内存池、
 * 自定义 malloc 和图形问题混在一起。
 */
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING  LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN

/* LVGL 堆位于外部 SDRAM，地址和容量统一由 app_memory_map.h 管理。 */
#define LV_MEM_SIZE APP_LVGL_HEAP_SIZE_BYTES
#define LV_MEM_ADR  APP_LVGL_HEAP_ADDR

/*====================
 * HAL settings
 *====================*/

/* 当前显示刷新目标先按约 30fps 的节奏配置。 */
#define LV_DEF_REFR_PERIOD 33

/* 5 寸 800x480 屏先按现有项目经验取 130 DPI。 */
#define LV_DPI_DEF 130

/*=================
 * Operating system
 *=================*/

/*
 * 必须保持 LV_OS_NONE：本工程只有 DisplayTask 调用 LVGL（串口 UI 命令
 * 经队列由 DisplayTask 消费）。CMSIS_RTOS2 模式会让 LVGL 9.5 把软件渲染
 * 放进独立绘制线程，与 UI 线程产生数据竞争——2026-07-26 真机曾因此在
 * "自检运行中开 ADC DISPLAY" 时拿到损坏的 draw_buf（data=NULL、宽高为
 * 垃圾值），绘制线程陷入数十亿次越界蠕写导致整机冻结（详见
 * 文件改动记录_UTF-8.md 当日 UI 卡死条目）。单核 MCU 上独立绘制线程
 * 也没有性能收益。
 */
#define LV_USE_OS LV_OS_NONE

/*========================
 * Rendering configuration
 *========================*/

/* 第一轮先只用软件绘图，关闭并行绘制和额外加速。 */
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_DRAW_UNIT_CNT 1

/*
 * 2026-07-26 起启用 DMA2D 加速大面积填充与图片搬运。
 * 轮询模式（LV_USE_DRAW_DMA2D_INTERRUPT=0）：LV_USE_OS 为 NONE 时中断
 * 模式本就无效（lv_draw_dma2d.c 顶部有编译警告），轮询与单线程渲染
 * 契合且不依赖 stm32h7xx_it.c 挂中断钩子。cache 一致性由 LVGL 自带的
 * clean/invalidate（M7 下 LV_DRAW_DMA2D_CACHE 自动为 1）+ MPU 对 SDRAM
 * 的 write-through 策略双重保证。
 */
#define LV_USE_DRAW_DMA2D 1
#define LV_DRAW_DMA2D_HAL_INCLUDE "stm32h7xx_hal.h"
#define LV_USE_DRAW_DMA2D_INTERRUPT 0

/*=======================
 * Feature configuration
 *=======================*/

/* LVGL Pro 静态 C 代码使用对象名、subjects 和 chart。 */
#define LV_USE_OBJ_NAME 1
#define LV_USE_OBSERVER 1
#define LV_USE_CHART    1

/* 固件编译生成后的静态 C，不启用 XML 运行时与文件系统资源。 */

/* 第一轮建议打开日志，便于后续排查初始化和显示问题。 */
#define LV_USE_LOG 1
#if LV_USE_LOG
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
    #define LV_LOG_PRINTF 0
#endif

/* 这些断言对早期移植排错很有帮助，代价较低，建议先开。 */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*
 * 第一轮不跑官方 demo，也不依赖复杂资源系统。
 * 后面如果需要 benchmark/widgets，再按需打开。
 */
#define LV_USE_DEMO_WIDGETS   0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_STRESS    0
#define LV_USE_DEMO_MUSIC     0

/*==================
 * Font usage
 *==================*/

/*
 * 先保留一个最常用的小字体，足够显示最小 label。
 * 后面需要中文字体时，再单独接入自己的字库。
 */
#define LV_FONT_MONTSERRAT_14 1

/*
 * 中文字库 font_cn_16（UI/Fonts/font_cn_16.c，Noto Sans SC Medium 16px 4bpp）。
 * 覆盖 ASCII + GB2312 一级 3755 字 + 全角标点 + 工程符号，缺字回落 montserrat_14。
 * 重新生成：UI/Fonts/tools/gen_font_cn.ps1。此处声明后所有引用 lvgl.h 处可见。
 */
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(font_cn_16)

/*
 * 默认字体切到中文字库：LVGL Editor 生成代码未显式指定字体，全部继承此默认值，
 * 因此 Editor 重新导出不会影响中文显示。数字/英文由 font_cn_16 内置 ASCII 渲染。
 */
#define LV_FONT_DEFAULT &font_cn_16

#endif /* LV_CONF_H */
