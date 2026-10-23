/**
  ******************************************************************************
  * @file    app_lvgl_ui.c
  * @brief   模板初始页:标题/运行秒数/触摸计数,验证渲染、tick 与触摸通路(UTF-8)。
  ******************************************************************************
  */

#include "app_lvgl_ui.h"

#include "lvgl.h"

/*
 * 模板初始页面:标题 + 运行秒数 + 触摸计数。
 * 三个控件分别验证:LVGL 渲染通路、tick/定时器通路、GT9xx→indev 输入通路。
 * 新项目直接删掉本文件内容换成自己的页面即可(保持 App_LvglUiInit/Run 接口)。
 */
static lv_obj_t *s_uptime_label;
static lv_obj_t *s_touch_label;
static uint32_t s_touch_count;

static void screen_touch_event_cb(lv_event_t *e)
{
  (void)e;
  s_touch_count++;
}

void App_LvglUiInit(void)
{
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x101828), LV_PART_MAIN);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "H743 LVGL Baseline");
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

  s_uptime_label = lv_label_create(scr);
  lv_label_set_text(s_uptime_label, "uptime: 0 s");
  lv_obj_set_style_text_color(s_uptime_label, lv_color_hex(0x9BE29B), LV_PART_MAIN);
  lv_obj_align(s_uptime_label, LV_ALIGN_CENTER, 0, -12);

  s_touch_label = lv_label_create(scr);
  lv_label_set_text(s_touch_label, "touch: 0");
  lv_obj_set_style_text_color(s_touch_label, lv_color_hex(0x9BC7E2), LV_PART_MAIN);
  lv_obj_align(s_touch_label, LV_ALIGN_CENTER, 0, 24);

  lv_obj_add_event_cb(scr, screen_touch_event_cb, LV_EVENT_PRESSED, NULL);
}

void App_LvglUiRun(void)
{
  /* 由 DisplayTask 周期调用;这里只做轻量的文本刷新。 */
  static uint32_t last_second = UINT32_MAX;
  uint32_t now_second = lv_tick_get() / 1000U;

  if (now_second != last_second) {
    last_second = now_second;
    lv_label_set_text_fmt(s_uptime_label, "uptime: %lu s", (unsigned long)now_second);
    lv_label_set_text_fmt(s_touch_label, "touch: %lu", (unsigned long)s_touch_count);
  }
}
