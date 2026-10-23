#include "app_lvgl_ui.h"

#include "app_demo_data.h"
#include "app_instrument_screen.h"
#include "lvgl.h"
#include "signal_separation_ui.h"

/*
 * UI 模板初始页(Demo Scope):lv_chart 实时波形 + 指标标签 + 屏幕导览。
 * 页面组织:
 *   - 本 Demo 屏为运行时创建,验证"数据任务 -> UI"整条链路
 *   - Designer 六屏(spectrum/monitor/control/system/test/calibration)由
 *     LVGL Editor 生成(UI/Designer),按需懒创建(screen_*_create),
 *     作为现成框架随导览按钮浏览
 *   - Scope+ 为手写动态屏资产(UI/Custom/app_instrument_screen)
 * 导览目标屏首次进入时,运行时贴一个 [<] 返回钮(不改动 gen 代码,
 * LVGL Editor 重新 Generate 不受影响)。
 */

static lv_obj_t *s_demo_screen;
static lv_obj_t *s_chart;
static lv_chart_series_t *s_series;
static lv_obj_t *s_freq_label;
static lv_obj_t *s_vpp_label;
static lv_obj_t *s_seq_label;
static int32_t s_chart_points[APP_DEMO_DATA_POINTS];

typedef struct
{
    const char *label;
    lv_obj_t *(*create)(void);
    lv_obj_t *cached;
} NavTarget;

static lv_obj_t *nav_instrument_create(void)
{
    return AppInstrumentScreen_Create();
}

static NavTarget s_nav_targets[] = {
    { "Spectrum", screen_spectrum_create,    NULL },
    { "Monitor",  screen_monitor_create,     NULL },
    { "Control",  screen_control_create,     NULL },
    { "System",   screen_system_create,      NULL },
    { "Test",     screen_test_create,        NULL },
    { "Calib",    screen_calibration_create, NULL },
    { "Scope+",   nav_instrument_create,     NULL },
};

static void nav_back_event_cb(lv_event_t *e)
{
    (void)e;
    lv_screen_load(s_demo_screen);
}

static void attach_back_button(lv_obj_t *screen)
{
    /* 用 user_data 标记只贴一次;运行时添加,不影响 Designer 重新生成。 */
    if ((screen == NULL) || (lv_obj_get_user_data(screen) != NULL))
    {
        return;
    }
    lv_obj_set_user_data(screen, (void *)1);

    lv_obj_t *btn = lv_button_create(screen);
    lv_obj_set_size(btn, 52, 34);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 6, 6);
    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, "<");
    lv_obj_center(lab);
    lv_obj_add_event_cb(btn, nav_back_event_cb, LV_EVENT_CLICKED, NULL);
}

static void nav_goto_event_cb(lv_event_t *e)
{
    NavTarget *target = (NavTarget *)lv_event_get_user_data(e);

    if (target == NULL)
    {
        return;
    }
    if (target->cached == NULL)
    {
        target->cached = target->create();   /* 懒创建 */
    }
    if (target->cached != NULL)
    {
        attach_back_button(target->cached);
        lv_screen_load(target->cached);
    }
}

static void demo_screen_create(void)
{
    s_demo_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_demo_screen, lv_color_hex(0x0c1118), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(s_demo_screen);
    lv_label_set_text(title, "H743 UI Instrument - Demo Scope (simulated data)");
    lv_obj_set_style_text_color(title, lv_color_hex(0xf0f2f4), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    s_chart = lv_chart_create(s_demo_screen);
    lv_obj_set_size(s_chart, 720, 300);
    lv_obj_align(s_chart, LV_ALIGN_TOP_MID, 0, 44);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, APP_DEMO_DATA_POINTS);
    lv_chart_set_axis_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, -1200, 1200);
    lv_chart_set_div_line_count(s_chart, 5, 9);
    lv_obj_set_style_bg_color(s_chart, lv_color_hex(0x05070a), LV_PART_MAIN);
    lv_obj_set_style_line_color(s_chart, lv_color_hex(0x2a343d), LV_PART_MAIN);
    s_series = lv_chart_add_series(s_chart, lv_palette_main(LV_PALETTE_GREEN),
                                   LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_series_ext_y_array(s_chart, s_series, s_chart_points);

    s_freq_label = lv_label_create(s_demo_screen);
    lv_obj_set_style_text_color(s_freq_label, lv_color_hex(0x9be29b), LV_PART_MAIN);
    lv_obj_set_pos(s_freq_label, 12, 356);
    lv_label_set_text(s_freq_label, "freq: -- kHz");

    s_vpp_label = lv_label_create(s_demo_screen);
    lv_obj_set_style_text_color(s_vpp_label, lv_color_hex(0x9bc7e2), LV_PART_MAIN);
    lv_obj_set_pos(s_vpp_label, 232, 356);
    lv_label_set_text(s_vpp_label, "vpp: -- code");

    s_seq_label = lv_label_create(s_demo_screen);
    lv_obj_set_style_text_color(s_seq_label, lv_color_hex(0xd8c27a), LV_PART_MAIN);
    lv_obj_set_pos(s_seq_label, 452, 356);
    lv_label_set_text(s_seq_label, "frame: 0");

    for (uint32_t i = 0U; i < (sizeof(s_nav_targets) / sizeof(s_nav_targets[0])); i++)
    {
        lv_obj_t *btn = lv_button_create(s_demo_screen);
        lv_obj_set_size(btn, 104, 36);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 8 + (int32_t)i * 112, -8);
        lv_obj_t *lab = lv_label_create(btn);
        lv_label_set_text(lab, s_nav_targets[i].label);
        lv_obj_center(lab);
        lv_obj_add_event_cb(btn, nav_goto_event_cb, LV_EVENT_CLICKED, &s_nav_targets[i]);
    }
}

void App_LvglUiInit(void)
{
    /* Designer 生成库:先选目标分辨率再初始化。 */
    signal_separation_ui_set_target(SIGNAL_SEPARATION_UI_TARGET_H743_RGB_800X480);
    signal_separation_ui_init(NULL);

    /* ui_init 若已创建 monitor 全局屏,直接纳入导览缓存,避免重复创建。 */
    s_nav_targets[1].cached = screen_monitor;

    demo_screen_create();
    lv_screen_load(s_demo_screen);
}

void App_LvglUiRun(void)
{
    static AppDemoFrame frame;

    if (AppDemoData_TryFetch(&frame))
    {
        for (uint32_t i = 0U; i < APP_DEMO_DATA_POINTS; i++)
        {
            s_chart_points[i] = frame.points[i];
        }
        lv_chart_refresh(s_chart);
        lv_label_set_text_fmt(s_freq_label, "freq: %d.%01d kHz (sim)",
                              (int)(frame.freq_hz / 1000.0f),
                              (int)(frame.freq_hz / 100.0f) % 10);
        lv_label_set_text_fmt(s_vpp_label, "vpp: %d code", (int)frame.vpp);
        lv_label_set_text_fmt(s_seq_label, "frame: %u", (unsigned)frame.seq);
    }
}
