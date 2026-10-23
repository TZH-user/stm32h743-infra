/**
  ******************************************************************************
  * @file    app_instrument_screen.c
  * @brief   信号分离装置结果页、混合波形页与稳定对象命名（UTF-8）。
  ******************************************************************************
  */

#include "app_instrument_screen.h"

#include "signal_separation_ui.h"

static lv_style_t s_panel_style;
static lv_style_t s_plot_style;
static lv_style_t s_key_style;
static lv_style_t s_run_style;
static lv_style_t s_stop_style;
static bool s_styles_initialized = false;

static void app_scope_init_styles(void)
{
    if (s_styles_initialized)
    {
        return;
    }

    lv_style_init(&s_panel_style);
    lv_style_set_bg_color(&s_panel_style, lv_color_hex(0x16191d));
    lv_style_set_bg_opa(&s_panel_style, LV_OPA_COVER);
    lv_style_set_border_color(&s_panel_style, lv_color_hex(0x3b4148));
    lv_style_set_border_width(&s_panel_style, 1);
    lv_style_set_radius(&s_panel_style, 2);
    lv_style_set_pad_all(&s_panel_style, 0);

    lv_style_init(&s_plot_style);
    lv_style_set_bg_color(&s_plot_style, lv_color_hex(0x030405));
    lv_style_set_bg_opa(&s_plot_style, LV_OPA_COVER);
    lv_style_set_border_color(&s_plot_style, lv_color_hex(0x59616a));
    lv_style_set_border_width(&s_plot_style, 1);
    lv_style_set_radius(&s_plot_style, 0);
    lv_style_set_pad_all(&s_plot_style, 4);
    lv_style_set_line_color(&s_plot_style, lv_color_hex(0x354047));

    lv_style_init(&s_key_style);
    lv_style_set_bg_color(&s_key_style, lv_color_hex(0x272c32));
    lv_style_set_bg_opa(&s_key_style, LV_OPA_COVER);
    lv_style_set_border_color(&s_key_style, lv_color_hex(0x626b75));
    lv_style_set_border_width(&s_key_style, 1);
    lv_style_set_radius(&s_key_style, 3);
    lv_style_set_text_color(&s_key_style, lv_color_hex(0xf2f3f4));

    lv_style_init(&s_run_style);
    lv_style_set_bg_color(&s_run_style, lv_color_hex(0x2abf70));
    lv_style_set_bg_opa(&s_run_style, LV_OPA_COVER);
    lv_style_set_border_width(&s_run_style, 0);
    lv_style_set_radius(&s_run_style, 3);
    lv_style_set_text_color(&s_run_style, lv_color_hex(0x04130b));

    lv_style_init(&s_stop_style);
    lv_style_set_bg_color(&s_stop_style, lv_color_hex(0xd84b4b));
    lv_style_set_bg_opa(&s_stop_style, LV_OPA_COVER);
    lv_style_set_border_width(&s_stop_style, 0);
    lv_style_set_radius(&s_stop_style, 3);
    lv_style_set_text_color(&s_stop_style, lv_color_hex(0xffffff));

    s_styles_initialized = true;
}

static lv_obj_t *app_scope_panel(lv_obj_t *parent,
                                 int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_add_style(panel, &s_panel_style, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

static lv_obj_t *app_scope_label(lv_obj_t *parent,
                                 const char *text,
                                 int32_t x,
                                 int32_t y,
                                 uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static lv_obj_t *app_scope_key(lv_obj_t *parent,
                               const char *text,
                               int32_t y,
                               lv_style_t *style)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, 8, y);
    lv_obj_set_size(button, 136, 42);
    lv_obj_add_style(button, style, 0);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

static void app_scope_apply_trace_style(lv_obj_t *chart)
{
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_width(chart, 2, LV_PART_INDICATOR);
    lv_obj_set_style_height(chart, 2, LV_PART_INDICATOR);
    lv_obj_set_style_radius(chart, 1, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, LV_PART_INDICATOR);
}

lv_obj_t *AppInstrumentScreen_Create(void)
{
    app_scope_init_styles();

    if (screen_monitor == NULL)
    {
        screen_monitor = lv_obj_create(NULL);
    }
    lv_obj_set_name_static(screen_monitor, "screen_monitor_scope");
    lv_obj_add_style(screen_monitor, &style_screen, 0);

    lv_obj_t *header = app_scope_panel(screen_monitor, 0, 0, 800, 40);
    app_scope_label(header, "SIGNAL SEPARATION DEVICE", 12, 12, 0xf2f3f4);
    app_scope_label(header, "AD9226 / 4096 FFT / 8 FRAME VOTE",
                    260, 12, 0x80d6ff);
    lv_obj_t *status = app_scope_label(header, "", 590, 12, 0x72eaa5);
    lv_obj_set_width(status, 196);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_bind_text(status, &analysis_state, NULL);

    lv_obj_t *title = app_scope_label(screen_monitor,
                                      "MIXED INPUT WAVEFORM",
                                      12,
                                      46,
                                      0xffdf3f);
    lv_obj_set_name_static(title, "ui_title");

    lv_obj_t *chart = lv_chart_create(screen_monitor);
    lv_obj_set_name_static(chart, "ui_chart");
    lv_obj_set_pos(chart, 8, 68);
    lv_obj_set_size(chart, 624, 200);
    lv_obj_add_style(chart, &s_plot_style, 0);
    app_scope_apply_trace_style(chart);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, 32);
    lv_chart_set_hor_div_line_count(chart, 8);
    lv_chart_set_ver_div_line_count(chart, 10);
    lv_chart_set_axis_min_value(chart, LV_CHART_AXIS_PRIMARY_Y, 0);
    lv_chart_set_axis_max_value(chart, LV_CHART_AXIS_PRIMARY_Y, 100);
    lv_chart_series_t *series = lv_chart_add_series(
        chart, lv_color_hex(0xffdf3f), LV_CHART_AXIS_PRIMARY_Y);
    static const int32_t initial_waveform[32] = {
        50, 66, 77, 79, 71, 56, 39, 25,
        20, 27, 42, 59, 73, 80, 76, 63,
        46, 31, 21, 22, 33, 49, 65, 77,
        79, 70, 54, 37, 24, 20, 29, 44
    };
    lv_chart_set_series_values(chart, series, initial_waveform, 32);

    lv_obj_t *result = app_scope_panel(screen_monitor, 8, 276, 624, 196);
    lv_obj_t *signal_a = app_scope_panel(result, 8, 8, 296, 92);
    app_scope_label(signal_a, "SIGNAL A  (LOWER FREQUENCY)",
                    10, 6, 0xffdf3f);
    app_scope_label(signal_a, "TYPE", 10, 32, 0x9aa4ad);
    lv_obj_t *type_a = app_scope_label(signal_a, "--", 58, 32, 0xffffff);
    lv_obj_set_name_static(type_a, "ui_signal_a_type");
    lv_obj_t *votes_a = app_scope_label(signal_a, "F 0/8  T 0/8",
                                        174, 32, 0x72eaa5);
    lv_obj_set_name_static(votes_a, "ui_signal_a_votes");
    app_scope_label(signal_a, "FREQ", 10, 62, 0x9aa4ad);
    lv_obj_t *frequency_a = app_scope_label(signal_a, "--", 58, 62, 0xffffff);
    lv_obj_set_name_static(frequency_a, "ui_signal_a_frequency");
    app_scope_label(signal_a, "kHz", 126, 62, 0x9aa4ad);
    app_scope_label(signal_a, "AMP", 174, 62, 0x9aa4ad);
    lv_obj_t *amplitude_a = app_scope_label(signal_a, "--", 214, 62, 0xffffff);
    lv_obj_set_name_static(amplitude_a, "ui_signal_a_amplitude");

    lv_obj_t *signal_b = app_scope_panel(result, 312, 8, 304, 92);
    app_scope_label(signal_b, "SIGNAL B  (HIGHER FREQUENCY)",
                    10, 6, 0x80d6ff);
    app_scope_label(signal_b, "TYPE", 10, 32, 0x9aa4ad);
    lv_obj_t *type_b = app_scope_label(signal_b, "--", 58, 32, 0xffffff);
    lv_obj_set_name_static(type_b, "ui_signal_b_type");
    lv_obj_t *votes_b = app_scope_label(signal_b, "F 0/8  T 0/8",
                                        180, 32, 0x72eaa5);
    lv_obj_set_name_static(votes_b, "ui_signal_b_votes");
    app_scope_label(signal_b, "FREQ", 10, 62, 0x9aa4ad);
    lv_obj_t *frequency_b = app_scope_label(signal_b, "--", 58, 62, 0xffffff);
    lv_obj_set_name_static(frequency_b, "ui_signal_b_frequency");
    app_scope_label(signal_b, "kHz", 126, 62, 0x9aa4ad);
    app_scope_label(signal_b, "AMP", 180, 62, 0x9aa4ad);
    lv_obj_t *amplitude_b = app_scope_label(signal_b, "--", 220, 62, 0xffffff);
    lv_obj_set_name_static(amplitude_b, "ui_signal_b_amplitude");

    app_scope_label(result, "VALID", 10, 112, 0x9aa4ad);
    lv_obj_t *valid_frames = app_scope_label(result, "0/8", 60, 112, 0xffffff);
    lv_obj_set_name_static(valid_frames, "ui_valid_frames");
    app_scope_label(result, "CONF", 128, 112, 0x9aa4ad);
    lv_obj_t *confidence = app_scope_label(result, "0.000", 178, 112, 0xffffff);
    lv_obj_set_name_static(confidence, "ui_confidence");
    app_scope_label(result, "RES", 270, 112, 0x9aa4ad);
    lv_obj_t *residual = app_scope_label(result, "0.00000", 306, 112, 0xffffff);
    lv_obj_set_name_static(residual, "ui_residual");
    app_scope_label(result, "TIME", 424, 112, 0x9aa4ad);
    lv_obj_t *time = app_scope_label(result, "0 ms", 470, 112, 0xffffff);
    lv_obj_set_name_static(time, "ui_session_time");

    lv_obj_t *diagnostic = app_scope_label(result,
                                            "CAND 0  PAIRS 0  MODELS 0",
                                            10, 140, 0x80d6ff);
    lv_obj_set_name_static(diagnostic, "ui_model_diagnostic");
    lv_obj_t *error = app_scope_label(result, "READY", 10, 166, 0x72eaa5);
    lv_obj_set_name_static(error, "ui_error_detail");
    app_scope_label(result, "PEAK", 424, 140, 0x9aa4ad);
    lv_obj_t *frequency = app_scope_label(result, "--", 470, 140, 0xffffff);
    lv_obj_set_name_static(frequency, "ui_peak_frequency");
    app_scope_label(result, "kHz", 522, 140, 0x9aa4ad);
    app_scope_label(result, "SNR", 424, 166, 0x9aa4ad);
    lv_obj_t *level = app_scope_label(result, "--", 470, 166, 0xffffff);
    lv_obj_set_name_static(level, "ui_peak_level");
    lv_obj_t *level_unit = app_scope_label(result, "dB", 522, 166, 0x9aa4ad);
    lv_obj_set_name_static(level_unit, "ui_peak_level_unit");

    lv_obj_t *menu = app_scope_panel(screen_monitor, 640, 48, 152, 424);
    app_scope_label(menu, "CONTROL", 14, 12, 0x80d6ff);
    lv_obj_t *mode = lv_dropdown_create(menu);
    lv_obj_set_name_static(mode, "ui_mode_dropdown");
    lv_obj_set_pos(mode, 8, 36);
    lv_obj_set_size(mode, 136, 38);
    lv_dropdown_set_options(mode, "Spectrum Preview\nWaveform");
    lv_dropdown_set_selected(mode, 1);
    lv_obj_add_style(mode, &s_key_style, 0);

    lv_obj_t *detail = app_scope_key(menu, "FULL VIEW", 84, &s_key_style);
    lv_obj_add_screen_create_event(detail,
                                   LV_EVENT_CLICKED,
                                   screen_spectrum_create,
                                   LV_SCREEN_LOAD_ANIM_NONE,
                                   0,
                                   0);

    app_scope_label(menu, "B'-A' PHASE", 12, 132, 0x9aa4ad);
    lv_obj_t *phase_decrease = lv_button_create(menu);
    lv_obj_set_name_static(phase_decrease, "ui_phase_decrease");
    lv_obj_set_pos(phase_decrease, 8, 154);
    lv_obj_set_size(phase_decrease, 38, 36);
    lv_obj_add_style(phase_decrease, &s_key_style, 0);
    lv_obj_t *phase_decrease_label = lv_label_create(phase_decrease);
    lv_label_set_text(phase_decrease_label, "-");
    lv_obj_center(phase_decrease_label);

    lv_obj_t *phase_value = app_scope_label(menu, "0 deg", 50, 164,
                                             0xffffff);
    lv_obj_set_name_static(phase_value, "ui_phase_value");
    lv_obj_set_width(phase_value, 52);
    lv_obj_set_style_text_align(phase_value, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *phase_increase = lv_button_create(menu);
    lv_obj_set_name_static(phase_increase, "ui_phase_increase");
    lv_obj_set_pos(phase_increase, 106, 154);
    lv_obj_set_size(phase_increase, 38, 36);
    lv_obj_add_style(phase_increase, &s_key_style, 0);
    lv_obj_t *phase_increase_label = lv_label_create(phase_increase);
    lv_label_set_text(phase_increase_label, "+");
    lv_obj_center(phase_increase_label);

    lv_obj_t *home = app_scope_key(menu, "HOME", 218, &s_key_style);
    lv_obj_add_screen_create_event(home,
                                   LV_EVENT_CLICKED,
                                   screen_control_create,
                                   LV_SCREEN_LOAD_ANIM_NONE,
                                   0,
                                   0);

    lv_obj_t *run = app_scope_key(menu, "RUN", 324, &s_run_style);
    lv_obj_set_name_static(run, "ui_start_button");
    lv_obj_t *stop = app_scope_key(menu, "STOP", 372, &s_stop_style);
    lv_obj_set_name_static(stop, "ui_stop_button");

    return screen_monitor;
}

lv_obj_t *AppInstrumentDetailScreen_Create(void)
{
    app_scope_init_styles();

    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_name_static(screen, "screen_spectrum_scope");
    lv_obj_add_style(screen, &style_screen, 0);

    lv_obj_t *header = app_scope_panel(screen, 0, 0, 800, 50);
    lv_obj_t *back = lv_button_create(header);
    lv_obj_set_pos(back, 8, 7);
    lv_obj_set_size(back, 88, 36);
    lv_obj_add_style(back, &s_key_style, 0);
    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, "BACK");
    lv_obj_center(back_label);
    lv_obj_add_screen_load_event(back,
                                 LV_EVENT_CLICKED,
                                 screen_monitor,
                                 LV_SCREEN_LOAD_ANIM_NONE,
                                 0,
                                 0);

    lv_obj_t *title = app_scope_label(header,
                                      "FULL WAVEFORM",
                                      112,
                                      16,
                                      0xffdf3f);
    lv_obj_set_name_static(title, "ui_title");
    lv_obj_t *mode = lv_dropdown_create(header);
    lv_obj_set_name_static(mode, "ui_mode_dropdown");
    lv_obj_set_pos(mode, 282, 7);
    lv_obj_set_size(mode, 210, 36);
    lv_dropdown_set_options(mode, "FFT Spectrum\nReal-time Waveform");
    lv_dropdown_set_selected(mode, 1);
    lv_obj_add_style(mode, &s_key_style, 0);
    lv_obj_t *info = app_scope_label(header,
                                     "128 consecutive ADC samples",
                                     520,
                                     16,
                                     0x80d6ff);
    lv_obj_set_name_static(info, "ui_spectrum_info");

    lv_obj_t *chart = lv_chart_create(screen);
    lv_obj_set_name_static(chart, "ui_chart");
    lv_obj_set_pos(chart, 8, 60);
    lv_obj_set_size(chart, 784, 348);
    lv_obj_add_style(chart, &s_plot_style, 0);
    app_scope_apply_trace_style(chart);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, 32);
    lv_chart_set_hor_div_line_count(chart, 8);
    lv_chart_set_ver_div_line_count(chart, 10);
    lv_chart_set_axis_min_value(chart, LV_CHART_AXIS_PRIMARY_Y, 0);
    lv_chart_set_axis_max_value(chart, LV_CHART_AXIS_PRIMARY_Y, 100);
    lv_chart_series_t *series = lv_chart_add_series(
        chart, lv_color_hex(0xffdf3f), LV_CHART_AXIS_PRIMARY_Y);
    static const int32_t initial_waveform[32] = {
        50, 65, 76, 80, 73, 59, 42, 27,
        20, 25, 39, 56, 71, 79, 78, 66,
        50, 34, 22, 21, 31, 46, 63, 75,
        80, 74, 61, 44, 29, 21, 24, 37
    };
    lv_chart_set_series_values(chart, series, initial_waveform, 32);

    lv_obj_t *readout = app_scope_panel(screen, 8, 416, 784, 56);
    app_scope_label(readout, "CH1", 12, 18, 0xffdf3f);
    app_scope_label(readout, "1.00 V/div", 52, 18, 0xf2f3f4);
    app_scope_label(readout, "H", 206, 18, 0x80d6ff);
    app_scope_label(readout, "20.0 us/div", 230, 18, 0xf2f3f4);
    app_scope_label(readout, "TRIG", 396, 18, 0x72eaa5);
    app_scope_label(readout, "CH1 Rising", 442, 18, 0xf2f3f4);
    app_scope_label(readout, "LIVE SNAPSHOT", 650, 18, 0x72eaa5);
    return screen;
}
