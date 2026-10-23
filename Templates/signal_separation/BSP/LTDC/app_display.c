#include "app_display.h"
#include "app_memory_map.h"

#include "cmsis_os2.h"
#include "gpio.h"
#include "main.h"

#include <stdint.h>

/* LTDC 帧缓冲地址和尺寸统一由 app_memory_map.h 管理。 */

#define RGB565_RED         0xF800U
#define RGB565_GREEN       0x07E0U
#define RGB565_BLUE        0x001FU
#define RGB565_WHITE       0xFFFFU

/*
 * 方案 1：保留上电彩条，但允许后续一键关闭。
 * 等显示基线完全稳定后，可以把它改成 0，直接进 LVGL。
 */
#define APP_DISPLAY_ENABLE_POWER_ON_PATTERN 0

static void App_DisplayBacklightOn(void);
static void App_DisplayCleanDCacheByAddr(uint32_t addr, uint32_t size);
static void App_DisplayFillColor(uint16_t color);

void App_DisplayInit(void)
{
  /*
   * 现在 LCD_BL 已经由 CubeMX 接管为普通 GPIO 输出脚，
   * 这里不再重复初始化 GPIO 模式，只负责把背光拉高。
   */
  App_DisplayBacklightOn();
}

void App_DisplayRunPowerOnPattern(void)
{
#if !APP_DISPLAY_ENABLE_POWER_ON_PATTERN
  return;
#endif

  /*
   * 在 RTOS 任务上下文里，彩条延时统一用 osDelay()，避免继续混用 HAL_Delay()。
   * 这样任务调度语义更清楚，也更符合后续全部迁入 RTOS 的方向。
   */
  App_DisplayFillColor(RGB565_RED);
  osDelay(300);
  App_DisplayFillColor(RGB565_GREEN);
  osDelay(300);
  App_DisplayFillColor(RGB565_BLUE);
  osDelay(300);
  App_DisplayFillColor(RGB565_WHITE);
}

static void App_DisplayBacklightOn(void)
{
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
}

static void App_DisplayCleanDCacheByAddr(uint32_t addr, uint32_t size)
{
  uint32_t aligned_addr = addr & ~31UL;
  uint32_t aligned_size = ((addr + size + 31UL) & ~31UL) - aligned_addr;

  /* LTDC 直接从 SDRAM 取像素，不清 DCache 就可能继续显示旧图像。 */
  SCB_CleanDCache_by_Addr((uint32_t *)aligned_addr, (int32_t)aligned_size);
}

static void App_DisplayFillColor(uint16_t color)
{
  uint32_t pixel_count = APP_LCD_HOR_RES * APP_LCD_VER_RES;
  uint32_t index = 0;
  volatile uint16_t *framebuffer = (volatile uint16_t *)APP_FB_ADDR;

  for(index = 0; index < pixel_count; index++) {
    framebuffer[index] = color;
  }

  App_DisplayCleanDCacheByAddr(APP_FB_ADDR, APP_FB_SIZE_BYTES);
}
