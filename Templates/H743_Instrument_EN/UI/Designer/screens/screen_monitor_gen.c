/**
 * @file screen_monitor_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_monitor_gen.h"
#include "../signal_separation_ui.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/***********************
 *  STATIC VARIABLES
 **********************/

/***********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * screen_monitor_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t scope_panel;
    static lv_style_t scope_plot;
    static lv_style_t scope_key;
    static lv_style_t scope_run;
    static lv_style_t scope_stop;
    static lv_style_t scope_home;
    static lv_style_t scope_trace_line;
    static lv_style_t scope_trace_point;

    static bool style_inited = false;

    if (!style_inited) {
        #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
        if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
            lv_style_init(&scope_panel);
            lv_style_set_bg_color(&scope_panel, lv_color_hex(0x16191d));
            lv_style_set_bg_opa(&scope_panel, (255 * 100 / 100));
            lv_style_set_border_color(&scope_panel, lv_color_hex(0x3b4148));
            lv_style_set_border_width(&scope_panel, 1);
            lv_style_set_radius(&scope_panel, 2);
            lv_style_set_pad_all(&scope_panel, 0);

            lv_style_init(&scope_plot);
            lv_style_set_bg_color(&scope_plot, lv_color_hex(0x030405));
            lv_style_set_bg_opa(&scope_plot, (255 * 100 / 100));
            lv_style_set_border_color(&scope_plot, lv_color_hex(0x59616a));
            lv_style_set_border_width(&scope_plot, 1);
            lv_style_set_radius(&scope_plot, 0);
            lv_style_set_pad_all(&scope_plot, 4);
            lv_style_set_line_color(&scope_plot, lv_color_hex(0x354047));

            lv_style_init(&scope_key);
            lv_style_set_bg_color(&scope_key, lv_color_hex(0x272c32));
            lv_style_set_bg_opa(&scope_key, (255 * 100 / 100));
            lv_style_set_border_color(&scope_key, lv_color_hex(0x626b75));
            lv_style_set_border_width(&scope_key, 1);
            lv_style_set_radius(&scope_key, 3);
            lv_style_set_text_color(&scope_key, lv_color_hex(0xf2f3f4));

            lv_style_init(&scope_run);
            lv_style_set_bg_color(&scope_run, lv_color_hex(0x2abf70));
            lv_style_set_bg_opa(&scope_run, (255 * 100 / 100));
            lv_style_set_border_width(&scope_run, 0);
            lv_style_set_radius(&scope_run, 3);
            lv_style_set_text_color(&scope_run, lv_color_hex(0x04130b));

            lv_style_init(&scope_stop);
            lv_style_set_bg_color(&scope_stop, lv_color_hex(0xd84b4b));
            lv_style_set_bg_opa(&scope_stop, (255 * 100 / 100));
            lv_style_set_border_width(&scope_stop, 0);
            lv_style_set_radius(&scope_stop, 3);
            lv_style_set_text_color(&scope_stop, lv_color_hex(0xffffff));

            lv_style_init(&scope_home);
            lv_style_set_bg_color(&scope_home, lv_color_hex(0x176b87));
            lv_style_set_bg_opa(&scope_home, (255 * 100 / 100));
            lv_style_set_border_color(&scope_home, lv_color_hex(0x80d6ff));
            lv_style_set_border_width(&scope_home, 1);
            lv_style_set_radius(&scope_home, 3);
            lv_style_set_text_color(&scope_home, lv_color_hex(0xffffff));

            lv_style_init(&scope_trace_line);
            lv_style_set_line_width(&scope_trace_line, 2);

            lv_style_init(&scope_trace_point);
            lv_style_set_width(&scope_trace_point, 2);
            lv_style_set_height(&scope_trace_point, 2);
            lv_style_set_radius(&scope_trace_point, 1);
            lv_style_set_border_width(&scope_trace_point, 0);
            lv_style_set_bg_opa(&scope_trace_point, (255 * 100 / 100));

        }
        #endif
        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
    if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
        if (screen_monitor == NULL) screen_monitor = lv_obj_create(NULL);
        lv_obj_t * lv_obj_0 = screen_monitor;
        lv_obj_set_name_static(lv_obj_0, "screen_monitor_#");

        lv_obj_add_style(lv_obj_0, &style_screen, 0);
        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_1, 0);
        lv_obj_set_y(lv_obj_1, 0);
        lv_obj_set_width(lv_obj_1, 800);
        lv_obj_set_height(lv_obj_1, 40);
        lv_obj_add_style(lv_obj_1, &scope_panel, 0);
        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_1);
        lv_label_set_text(lv_label_0, "Signal Separator");
        lv_obj_set_x(lv_label_0, 12);
        lv_obj_set_y(lv_label_0, 12);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_1);
        lv_label_set_text(lv_label_1, "AD9226 / Dual DAC");
        lv_obj_set_x(lv_label_1, 268);
        lv_obj_set_y(lv_label_1, 12);
        lv_obj_set_style_text_color(lv_label_1, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_2 = lv_label_create(lv_obj_1);
        lv_label_bind_text(lv_label_2, &analysis_state, NULL);
        lv_obj_set_x(lv_label_2, 650);
        lv_obj_set_y(lv_label_2, 12);
        lv_obj_set_width(lv_label_2, 138);
        lv_obj_set_style_text_align(lv_label_2, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(lv_label_2, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * ui_title = lv_label_create(lv_obj_0);
        lv_obj_set_name(ui_title, "ui_title");
        lv_label_set_text(ui_title, "Mixed input waveform");
        lv_obj_set_x(ui_title, 12);
        lv_obj_set_y(ui_title, 46);
        lv_obj_set_style_text_color(ui_title, lv_color_hex(0xffdf3f), 0);

        lv_obj_t * ui_chart = lv_chart_create(lv_obj_0);
        lv_obj_set_name(ui_chart, "ui_chart");
        lv_obj_set_x(ui_chart, 8);
        lv_obj_set_y(ui_chart, 68);
        lv_obj_set_width(ui_chart, 624);
        lv_obj_set_height(ui_chart, 200);
        lv_chart_set_type(ui_chart, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(ui_chart, 32);
        lv_chart_set_hor_div_line_count(ui_chart, 8);
        lv_chart_set_ver_div_line_count(ui_chart, 10);
        lv_obj_add_style(ui_chart, &scope_plot, 0);
        lv_obj_add_style(ui_chart, &scope_trace_line, LV_PART_ITEMS);
        lv_obj_add_style(ui_chart, &scope_trace_point, LV_PART_INDICATOR);
        lv_chart_set_axis_min_value(ui_chart, LV_CHART_AXIS_PRIMARY_Y, 0);
        lv_chart_set_axis_max_value(ui_chart, LV_CHART_AXIS_PRIMARY_Y, 100);
        lv_chart_series_t * lv_chart_series_0 = lv_chart_add_series(ui_chart, lv_color_hex(0xffdf3f), LV_CHART_AXIS_PRIMARY_Y);
        static const int32_t ui_chart_values_0[] = {50, 66, 77, 79, 71, 56, 39, 25, 20, 27, 42, 59, 73, 80, 76, 63, 46, 31, 21, 22, 33, 49, 65, 77, 79, 70, 54, 37, 24, 20, 29, 44};
        lv_chart_set_series_values(ui_chart, lv_chart_series_0, ui_chart_values_0, 32);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 8);
        lv_obj_set_y(lv_obj_2, 276);
        lv_obj_set_width(lv_obj_2, 624);
        lv_obj_set_height(lv_obj_2, 196);
        lv_obj_add_style(lv_obj_2, &scope_panel, 0);
        lv_obj_t * lv_obj_3 = lv_obj_create(lv_obj_2);
        lv_obj_set_x(lv_obj_3, 8);
        lv_obj_set_y(lv_obj_3, 8);
        lv_obj_set_width(lv_obj_3, 296);
        lv_obj_set_height(lv_obj_3, 92);
        lv_obj_add_style(lv_obj_3, &scope_panel, 0);
        lv_obj_t * lv_label_3 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_3, "Signal A / LF");
        lv_obj_set_x(lv_label_3, 10);
        lv_obj_set_y(lv_label_3, 6);
        lv_obj_set_style_text_color(lv_label_3, lv_color_hex(0xffdf3f), 0);

        lv_obj_t * lv_label_4 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_4, "Type");
        lv_obj_set_x(lv_label_4, 10);
        lv_obj_set_y(lv_label_4, 32);
        lv_obj_set_style_text_color(lv_label_4, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_signal_a_type = lv_label_create(lv_obj_3);
        lv_obj_set_name(ui_signal_a_type, "ui_signal_a_type");
        lv_label_set_text(ui_signal_a_type, "--");
        lv_obj_set_x(ui_signal_a_type, 58);
        lv_obj_set_y(ui_signal_a_type, 32);

        lv_obj_t * ui_signal_a_votes = lv_label_create(lv_obj_3);
        lv_obj_set_name(ui_signal_a_votes, "ui_signal_a_votes");
        lv_label_set_text(ui_signal_a_votes, "F 0/8  T 0/8");
        lv_obj_set_x(ui_signal_a_votes, 174);
        lv_obj_set_y(ui_signal_a_votes, 32);
        lv_obj_set_style_text_color(ui_signal_a_votes, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_label_5 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_5, "Freq");
        lv_obj_set_x(lv_label_5, 10);
        lv_obj_set_y(lv_label_5, 62);
        lv_obj_set_style_text_color(lv_label_5, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_signal_a_frequency = lv_label_create(lv_obj_3);
        lv_obj_set_name(ui_signal_a_frequency, "ui_signal_a_frequency");
        lv_label_set_text(ui_signal_a_frequency, "-- kHz");
        lv_obj_set_x(ui_signal_a_frequency, 58);
        lv_obj_set_y(ui_signal_a_frequency, 62);

        lv_obj_t * lv_label_6 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_6, "Ampl");
        lv_obj_set_x(lv_label_6, 174);
        lv_obj_set_y(lv_label_6, 62);
        lv_obj_set_style_text_color(lv_label_6, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_signal_a_amplitude = lv_label_create(lv_obj_3);
        lv_obj_set_name(ui_signal_a_amplitude, "ui_signal_a_amplitude");
        lv_label_set_text(ui_signal_a_amplitude, "--");
        lv_obj_set_x(ui_signal_a_amplitude, 214);
        lv_obj_set_y(ui_signal_a_amplitude, 62);

        lv_obj_t * lv_obj_4 = lv_obj_create(lv_obj_2);
        lv_obj_set_x(lv_obj_4, 312);
        lv_obj_set_y(lv_obj_4, 8);
        lv_obj_set_width(lv_obj_4, 304);
        lv_obj_set_height(lv_obj_4, 92);
        lv_obj_add_style(lv_obj_4, &scope_panel, 0);
        lv_obj_t * lv_label_7 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_7, "Signal B / HF");
        lv_obj_set_x(lv_label_7, 10);
        lv_obj_set_y(lv_label_7, 6);
        lv_obj_set_style_text_color(lv_label_7, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_8 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_8, "Type");
        lv_obj_set_x(lv_label_8, 10);
        lv_obj_set_y(lv_label_8, 32);
        lv_obj_set_style_text_color(lv_label_8, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_signal_b_type = lv_label_create(lv_obj_4);
        lv_obj_set_name(ui_signal_b_type, "ui_signal_b_type");
        lv_label_set_text(ui_signal_b_type, "--");
        lv_obj_set_x(ui_signal_b_type, 58);
        lv_obj_set_y(ui_signal_b_type, 32);

        lv_obj_t * ui_signal_b_votes = lv_label_create(lv_obj_4);
        lv_obj_set_name(ui_signal_b_votes, "ui_signal_b_votes");
        lv_label_set_text(ui_signal_b_votes, "F 0/8  T 0/8");
        lv_obj_set_x(ui_signal_b_votes, 180);
        lv_obj_set_y(ui_signal_b_votes, 32);
        lv_obj_set_style_text_color(ui_signal_b_votes, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_label_9 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_9, "Freq");
        lv_obj_set_x(lv_label_9, 10);
        lv_obj_set_y(lv_label_9, 62);
        lv_obj_set_style_text_color(lv_label_9, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_signal_b_frequency = lv_label_create(lv_obj_4);
        lv_obj_set_name(ui_signal_b_frequency, "ui_signal_b_frequency");
        lv_label_set_text(ui_signal_b_frequency, "-- kHz");
        lv_obj_set_x(ui_signal_b_frequency, 58);
        lv_obj_set_y(ui_signal_b_frequency, 62);

        lv_obj_t * lv_label_10 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_10, "Ampl");
        lv_obj_set_x(lv_label_10, 180);
        lv_obj_set_y(lv_label_10, 62);
        lv_obj_set_style_text_color(lv_label_10, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_signal_b_amplitude = lv_label_create(lv_obj_4);
        lv_obj_set_name(ui_signal_b_amplitude, "ui_signal_b_amplitude");
        lv_label_set_text(ui_signal_b_amplitude, "--");
        lv_obj_set_x(ui_signal_b_amplitude, 220);
        lv_obj_set_y(ui_signal_b_amplitude, 62);

        lv_obj_t * lv_label_11 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_11, "Frames");
        lv_obj_set_x(lv_label_11, 10);
        lv_obj_set_y(lv_label_11, 112);
        lv_obj_set_style_text_color(lv_label_11, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_valid_frames = lv_label_create(lv_obj_2);
        lv_obj_set_name(ui_valid_frames, "ui_valid_frames");
        lv_label_set_text(ui_valid_frames, "0/8");
        lv_obj_set_x(ui_valid_frames, 60);
        lv_obj_set_y(ui_valid_frames, 112);

        lv_obj_t * lv_label_12 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_12, "Conf");
        lv_obj_set_x(lv_label_12, 128);
        lv_obj_set_y(lv_label_12, 112);
        lv_obj_set_style_text_color(lv_label_12, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_confidence = lv_label_create(lv_obj_2);
        lv_obj_set_name(ui_confidence, "ui_confidence");
        lv_label_set_text(ui_confidence, "0.000");
        lv_obj_set_x(ui_confidence, 178);
        lv_obj_set_y(ui_confidence, 112);

        lv_obj_t * lv_label_13 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_13, "Resid");
        lv_obj_set_x(lv_label_13, 270);
        lv_obj_set_y(lv_label_13, 112);
        lv_obj_set_style_text_color(lv_label_13, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_residual = lv_label_create(lv_obj_2);
        lv_obj_set_name(ui_residual, "ui_residual");
        lv_label_set_text(ui_residual, "0.00000");
        lv_obj_set_x(ui_residual, 306);
        lv_obj_set_y(ui_residual, 112);

        lv_obj_t * lv_label_14 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_14, "Time");
        lv_obj_set_x(lv_label_14, 424);
        lv_obj_set_y(lv_label_14, 112);
        lv_obj_set_style_text_color(lv_label_14, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_session_time = lv_label_create(lv_obj_2);
        lv_obj_set_name(ui_session_time, "ui_session_time");
        lv_label_set_text(ui_session_time, "0 ms");
        lv_obj_set_x(ui_session_time, 470);
        lv_obj_set_y(ui_session_time, 112);

        lv_obj_t * ui_model_diagnostic = lv_label_create(lv_obj_2);
        lv_obj_set_name(ui_model_diagnostic, "ui_model_diagnostic");
        lv_label_set_text(ui_model_diagnostic, "Cand 0 / Pair 0 / Model 0");
        lv_obj_set_x(ui_model_diagnostic, 10);
        lv_obj_set_y(ui_model_diagnostic, 140);
        lv_obj_set_style_text_color(ui_model_diagnostic, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * ui_error_detail = lv_label_create(lv_obj_2);
        lv_obj_set_name(ui_error_detail, "ui_error_detail");
        lv_label_set_text(ui_error_detail, "Ready");
        lv_obj_set_x(ui_error_detail, 10);
        lv_obj_set_y(ui_error_detail, 166);
        lv_obj_set_style_text_color(ui_error_detail, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_label_15 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_15, "Peak");
        lv_obj_set_x(lv_label_15, 424);
        lv_obj_set_y(lv_label_15, 140);
        lv_obj_set_style_text_color(lv_label_15, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_peak_frequency = lv_label_create(lv_obj_2);
        lv_obj_set_name(ui_peak_frequency, "ui_peak_frequency");
        lv_label_set_text(ui_peak_frequency, "--");
        lv_obj_set_x(ui_peak_frequency, 470);
        lv_obj_set_y(ui_peak_frequency, 140);

        lv_obj_t * lv_label_16 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_16, "kHz");
        lv_obj_set_x(lv_label_16, 522);
        lv_obj_set_y(lv_label_16, 140);
        lv_obj_set_style_text_color(lv_label_16, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_17 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_17, "SNR");
        lv_obj_set_x(lv_label_17, 424);
        lv_obj_set_y(lv_label_17, 166);
        lv_obj_set_style_text_color(lv_label_17, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_peak_level = lv_label_create(lv_obj_2);
        lv_obj_set_name(ui_peak_level, "ui_peak_level");
        lv_label_set_text(ui_peak_level, "--");
        lv_obj_set_x(ui_peak_level, 470);
        lv_obj_set_y(ui_peak_level, 166);

        lv_obj_t * ui_peak_level_unit = lv_label_create(lv_obj_2);
        lv_obj_set_name(ui_peak_level_unit, "ui_peak_level_unit");
        lv_label_set_text(ui_peak_level_unit, "dB");
        lv_obj_set_x(ui_peak_level_unit, 522);
        lv_obj_set_y(ui_peak_level_unit, 166);
        lv_obj_set_style_text_color(ui_peak_level_unit, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_obj_5 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_5, 640);
        lv_obj_set_y(lv_obj_5, 48);
        lv_obj_set_width(lv_obj_5, 152);
        lv_obj_set_height(lv_obj_5, 424);
        lv_obj_add_style(lv_obj_5, &scope_panel, 0);
        lv_obj_t * lv_label_18 = lv_label_create(lv_obj_5);
        lv_label_set_text(lv_label_18, "Control");
        lv_obj_set_x(lv_label_18, 14);
        lv_obj_set_y(lv_label_18, 12);
        lv_obj_set_style_text_color(lv_label_18, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * ui_mode_dropdown = lv_dropdown_create(lv_obj_5);
        lv_obj_set_name(ui_mode_dropdown, "ui_mode_dropdown");
        lv_obj_set_x(ui_mode_dropdown, 8);
        lv_obj_set_y(ui_mode_dropdown, 36);
        lv_obj_set_width(ui_mode_dropdown, 136);
        lv_obj_set_height(ui_mode_dropdown, 38);
        lv_dropdown_set_options(ui_mode_dropdown, "Spectrum\nWaveform");
        lv_dropdown_set_selected(ui_mode_dropdown, 1);
        lv_obj_add_style(ui_mode_dropdown, &scope_key, 0);

        lv_obj_t * lv_button_0 = lv_button_create(lv_obj_5);
        lv_obj_set_x(lv_button_0, 8);
        lv_obj_set_y(lv_button_0, 84);
        lv_obj_set_width(lv_button_0, 136);
        lv_obj_set_height(lv_button_0, 42);
        lv_obj_add_style(lv_button_0, &scope_key, 0);
        lv_obj_t * lv_label_19 = lv_label_create(lv_button_0);
        lv_label_set_text(lv_label_19, "Full view");
        lv_obj_set_align(lv_label_19, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_0, LV_EVENT_CLICKED, screen_spectrum_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * lv_label_20 = lv_label_create(lv_obj_5);
        lv_label_set_text(lv_label_20, "B'-A' Phase");
        lv_obj_set_x(lv_label_20, 12);
        lv_obj_set_y(lv_label_20, 136);
        lv_obj_set_style_text_color(lv_label_20, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * ui_phase_decrease = lv_button_create(lv_obj_5);
        lv_obj_set_name(ui_phase_decrease, "ui_phase_decrease");
        lv_obj_set_x(ui_phase_decrease, 8);
        lv_obj_set_y(ui_phase_decrease, 158);
        lv_obj_set_width(ui_phase_decrease, 38);
        lv_obj_set_height(ui_phase_decrease, 36);
        lv_obj_add_style(ui_phase_decrease, &scope_key, 0);
        lv_obj_t * lv_label_21 = lv_label_create(ui_phase_decrease);
        lv_label_set_text(lv_label_21, "-");
        lv_obj_set_align(lv_label_21, LV_ALIGN_CENTER);

        lv_obj_t * ui_phase_value = lv_label_create(lv_obj_5);
        lv_obj_set_name(ui_phase_value, "ui_phase_value");
        lv_label_set_text(ui_phase_value, "0°");
        lv_obj_set_x(ui_phase_value, 50);
        lv_obj_set_y(ui_phase_value, 168);
        lv_obj_set_width(ui_phase_value, 52);
        lv_obj_set_style_text_align(ui_phase_value, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * ui_phase_increase = lv_button_create(lv_obj_5);
        lv_obj_set_name(ui_phase_increase, "ui_phase_increase");
        lv_obj_set_x(ui_phase_increase, 106);
        lv_obj_set_y(ui_phase_increase, 158);
        lv_obj_set_width(ui_phase_increase, 38);
        lv_obj_set_height(ui_phase_increase, 36);
        lv_obj_add_style(ui_phase_increase, &scope_key, 0);
        lv_obj_t * lv_label_22 = lv_label_create(ui_phase_increase);
        lv_label_set_text(lv_label_22, "+");
        lv_obj_set_align(lv_label_22, LV_ALIGN_CENTER);

        lv_obj_t * lv_button_1 = lv_button_create(lv_obj_5);
        lv_obj_set_x(lv_button_1, 8);
        lv_obj_set_y(lv_button_1, 210);
        lv_obj_set_width(lv_button_1, 136);
        lv_obj_set_height(lv_button_1, 50);
        lv_obj_add_style(lv_button_1, &scope_home, 0);
        lv_obj_t * lv_label_23 = lv_label_create(lv_button_1);
        lv_label_set_text(lv_label_23, "Home");
        lv_obj_set_align(lv_label_23, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_1, LV_EVENT_CLICKED, screen_control_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * lv_label_24 = lv_label_create(lv_obj_5);
        lv_label_set_text(lv_label_24, "Cal data: legacy");
        lv_obj_set_x(lv_label_24, 12);
        lv_obj_set_y(lv_label_24, 274);
        lv_obj_set_style_text_color(lv_label_24, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_25 = lv_label_create(lv_obj_5);
        lv_label_set_text(lv_label_25, "Path: OK");
        lv_obj_set_x(lv_label_25, 12);
        lv_obj_set_y(lv_label_25, 296);
        lv_obj_set_style_text_color(lv_label_25, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * ui_start_button = lv_button_create(lv_obj_5);
        lv_obj_set_name(ui_start_button, "ui_start_button");
        lv_obj_set_x(ui_start_button, 8);
        lv_obj_set_y(ui_start_button, 324);
        lv_obj_set_width(ui_start_button, 136);
        lv_obj_set_height(ui_start_button, 42);
        lv_obj_add_style(ui_start_button, &scope_run, 0);
        lv_obj_t * lv_label_26 = lv_label_create(ui_start_button);
        lv_label_set_text(lv_label_26, "Run");
        lv_obj_set_align(lv_label_26, LV_ALIGN_CENTER);

        lv_obj_t * ui_stop_button = lv_button_create(lv_obj_5);
        lv_obj_set_name(ui_stop_button, "ui_stop_button");
        lv_obj_set_x(ui_stop_button, 8);
        lv_obj_set_y(ui_stop_button, 372);
        lv_obj_set_width(ui_stop_button, 136);
        lv_obj_set_height(ui_stop_button, 42);
        lv_obj_add_style(ui_stop_button, &scope_stop, 0);
        lv_obj_t * lv_label_27 = lv_label_create(ui_stop_button);
        lv_label_set_text(lv_label_27, "Stop");
        lv_obj_set_align(lv_label_27, LV_ALIGN_CENTER);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

