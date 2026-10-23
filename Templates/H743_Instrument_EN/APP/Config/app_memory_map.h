#ifndef __APP_MEMORY_MAP_H
#define __APP_MEMORY_MAP_H

#include <stdint.h>

/*
 * 外部 SDRAM 用户区统一内存映射（UTF-8）。
 *
 * APP/BSP/UI 用户文件必须引用这里的地址，禁止再次写入同值硬编码。
 * CubeMX 生成的 LTDC/MPU 初始地址仍由 .ioc 管理，不在生成区包含本文件。
 */

#define APP_SDRAM_BASE_ADDR            0xC0000000UL

#define APP_LCD_HOR_RES                800U
#define APP_LCD_VER_RES                480U
#define APP_LCD_PIXEL_SIZE_BYTES       2U

#define APP_FB_ADDR                    APP_SDRAM_BASE_ADDR
#define APP_FB_SIZE_BYTES              (APP_LCD_HOR_RES * APP_LCD_VER_RES * APP_LCD_PIXEL_SIZE_BYTES)
#define APP_FB_END_ADDR                (APP_FB_ADDR + APP_FB_SIZE_BYTES)

#define APP_SDRAM_TEST_ADDR            0xC0100000UL
#define APP_SDRAM_TEST_SIZE_BYTES      (256U * 1024U)
#define APP_SDRAM_TEST_END_ADDR        (APP_SDRAM_TEST_ADDR + APP_SDRAM_TEST_SIZE_BYTES)

#define APP_LVGL_DRAW_BUF_ADDR         0xC0200000UL
#define APP_LVGL_DRAW_BUF_LINES        40U
#define APP_LVGL_DRAW_BUF_SIZE_BYTES   (APP_LCD_HOR_RES * APP_LVGL_DRAW_BUF_LINES * APP_LCD_PIXEL_SIZE_BYTES)
#define APP_LVGL_DRAW_BUF_END_ADDR     (APP_LVGL_DRAW_BUF_ADDR + APP_LVGL_DRAW_BUF_SIZE_BYTES)

/*
 * 2026-07-26 由 64 KiB 扩到 256 KiB：中文字库页面 + 256 点 chart 常驻后
 * 真机实测 used=80%、最大连续空闲块仅 1628 B、碎片率 87%，大块分配一旦
 * 失败 LV_USE_ASSERT_MALLOC 会直接断言挂 UI。堆位于 SDRAM 4 MiB 处，
 * 后方直到 32 MiB 无其他用户区，扩容无冲突。
 */
#define APP_LVGL_HEAP_ADDR             0xC0400000UL
#define APP_LVGL_HEAP_SIZE_BYTES       (256U * 1024U)
#define APP_LVGL_HEAP_END_ADDR         (APP_LVGL_HEAP_ADDR + APP_LVGL_HEAP_SIZE_BYTES)

/* 修改地址或大小后，在编译期阻止用户区互相覆盖。 */
#if (APP_FB_END_ADDR > APP_SDRAM_TEST_ADDR)
#error "Framebuffer overlaps SDRAM test region"
#endif

#if (APP_SDRAM_TEST_END_ADDR > APP_LVGL_DRAW_BUF_ADDR)
#error "SDRAM test region overlaps LVGL draw buffer"
#endif

#if (APP_LVGL_DRAW_BUF_END_ADDR > APP_LVGL_HEAP_ADDR)
#error "LVGL draw buffer overlaps LVGL heap"
#endif

#endif /* __APP_MEMORY_MAP_H */
