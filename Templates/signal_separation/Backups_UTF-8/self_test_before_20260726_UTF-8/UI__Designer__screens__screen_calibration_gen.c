/**
 * @file screen_calibration_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_calibration_gen.h"
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

lv_obj_t * screen_calibration_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t cal_panel;
    static lv_style_t cal_field;
    static lv_style_t cal_primary;
    static lv_style_t cal_safe;
    static lv_style_t cal_stop;
    static lv_style_t cal_pending;
    static lv_style_t cal_plot;
    static lv_style_t cal_trace;

    static bool style_inited = false;

    if (!style_inited) {
        #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
        if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
            lv_style_init(&cal_panel);
            lv_style_set_bg_color(&cal_panel, lv_color_hex(0x16191d));
            lv_style_set_bg_opa(&cal_panel, (255 * 100 / 100));
            lv_style_set_border_color(&cal_panel, lv_color_hex(0x3b4148));
            lv_style_set_border_width(&cal_panel, 1);
            lv_style_set_radius(&cal_panel, 3);
            lv_style_set_pad_all(&cal_panel, 0);

            lv_style_init(&cal_field);
            lv_style_set_bg_color(&cal_field, lv_color_hex(0x272c32));
            lv_style_set_bg_opa(&cal_field, (255 * 100 / 100));
            lv_style_set_border_color(&cal_field, lv_color_hex(0x626b75));
            lv_style_set_border_width(&cal_field, 1);
            lv_style_set_radius(&cal_field, 3);
            lv_style_set_text_color(&cal_field, lv_color_hex(0xf2f3f4));

            lv_style_init(&cal_primary);
            lv_style_set_bg_color(&cal_primary, lv_color_hex(0x176b87));
            lv_style_set_bg_opa(&cal_primary, (255 * 100 / 100));
            lv_style_set_border_color(&cal_primary, lv_color_hex(0x80d6ff));
            lv_style_set_border_width(&cal_primary, 1);
            lv_style_set_radius(&cal_primary, 3);
            lv_style_set_text_color(&cal_primary, lv_color_hex(0xffffff));

            lv_style_init(&cal_safe);
            lv_style_set_bg_color(&cal_safe, lv_color_hex(0x2abf70));
            lv_style_set_bg_opa(&cal_safe, (255 * 100 / 100));
            lv_style_set_border_width(&cal_safe, 0);
            lv_style_set_radius(&cal_safe, 3);
            lv_style_set_text_color(&cal_safe, lv_color_hex(0x04130b));

            lv_style_init(&cal_stop);
            lv_style_set_bg_color(&cal_stop, lv_color_hex(0xd84b4b));
            lv_style_set_bg_opa(&cal_stop, (255 * 100 / 100));
            lv_style_set_border_width(&cal_stop, 0);
            lv_style_set_radius(&cal_stop, 3);
            lv_style_set_text_color(&cal_stop, lv_color_hex(0xffffff));

            lv_style_init(&cal_pending);
            lv_style_set_bg_color(&cal_pending, lv_color_hex(0x7a5b16));
            lv_style_set_bg_opa(&cal_pending, (255 * 100 / 100));
            lv_style_set_border_color(&cal_pending, lv_color_hex(0xffd43b));
            lv_style_set_border_width(&cal_pending, 1);
            lv_style_set_radius(&cal_pending, 3);
            lv_style_set_text_color(&cal_pending, lv_color_hex(0xffffff));

            lv_style_init(&cal_plot);
            lv_style_set_bg_color(&cal_plot, lv_color_hex(0x030405));
            lv_style_set_bg_opa(&cal_plot, (255 * 100 / 100));
            lv_style_set_border_color(&cal_plot, lv_color_hex(0x59616a));
            lv_style_set_border_width(&cal_plot, 1);
            lv_style_set_radius(&cal_plot, 0);
            lv_style_set_pad_all(&cal_plot, 4);
            lv_style_set_line_color(&cal_plot, lv_color_hex(0x354047));

            lv_style_init(&cal_trace);
            lv_style_set_line_width(&cal_trace, 2);

        }
        #endif
        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
    if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_calibration_#");

        lv_obj_add_style(lv_obj_0, &style_screen, 0);
        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_1, 0);
        lv_obj_set_y(lv_obj_1, 0);
        lv_obj_set_width(lv_obj_1, 800);
        lv_obj_set_height(lv_obj_1, 50);
        lv_obj_add_style(lv_obj_1, &cal_panel, 0);
        lv_obj_t * lv_button_0 = lv_button_create(lv_obj_1);
        lv_obj_set_x(lv_button_0, 8);
        lv_obj_set_y(lv_button_0, 7);
        lv_obj_set_width(lv_button_0, 88);
        lv_obj_set_height(lv_button_0, 36);
        lv_obj_add_style(lv_button_0, &cal_field, 0);
        lv_obj_t * lv_label_0 = lv_label_create(lv_button_0);
        lv_label_set_text(lv_label_0, "BACK");
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);

        lv_obj_add_screen_load_event(lv_button_0, LV_EVENT_CLICKED, screen_monitor, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_1);
        lv_label_set_text(lv_label_1, "AD / DA CALIBRATION");
        lv_obj_set_x(lv_label_1, 112);
        lv_obj_set_y(lv_label_1, 16);
        lv_obj_set_style_text_color(lv_label_1, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * cal_internal_button = lv_button_create(lv_obj_1);
        lv_obj_set_name(cal_internal_button, "cal_internal_button");
        lv_obj_set_x(cal_internal_button, 328);
        lv_obj_set_y(cal_internal_button, 7);
        lv_obj_set_width(cal_internal_button, 116);
        lv_obj_set_height(cal_internal_button, 36);
        lv_obj_add_style(cal_internal_button, &cal_primary, 0);
        lv_obj_t * lv_label_2 = lv_label_create(cal_internal_button);
        lv_label_set_text(lv_label_2, "INTERNAL");
        lv_obj_set_align(lv_label_2, LV_ALIGN_CENTER);

        lv_obj_t * cal_external_button = lv_button_create(lv_obj_1);
        lv_obj_set_name(cal_external_button, "cal_external_button");
        lv_obj_set_x(cal_external_button, 452);
        lv_obj_set_y(cal_external_button, 7);
        lv_obj_set_width(cal_external_button, 116);
        lv_obj_set_height(cal_external_button, 36);
        lv_obj_add_style(cal_external_button, &cal_field, 0);
        lv_obj_t * lv_label_3 = lv_label_create(cal_external_button);
        lv_label_set_text(lv_label_3, "EXTERNAL");
        lv_obj_set_align(lv_label_3, LV_ALIGN_CENTER);

        lv_obj_t * cal_exclusive_label = lv_label_create(lv_obj_1);
        lv_obj_set_name(cal_exclusive_label, "cal_exclusive_label");
        lv_label_set_text(cal_exclusive_label, "EXCLUSIVE");
        lv_obj_set_x(cal_exclusive_label, 610);
        lv_obj_set_y(cal_exclusive_label, 16);
        lv_obj_set_style_text_color(cal_exclusive_label, lv_color_hex(0xffd43b), 0);

        lv_obj_t * cal_safe_label = lv_label_create(lv_obj_1);
        lv_obj_set_name(cal_safe_label, "cal_safe_label");
        lv_label_set_text(cal_safe_label, "SAFE");
        lv_obj_set_x(cal_safe_label, 742);
        lv_obj_set_y(cal_safe_label, 16);
        lv_obj_set_style_text_color(cal_safe_label, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 8);
        lv_obj_set_y(lv_obj_2, 58);
        lv_obj_set_width(lv_obj_2, 240);
        lv_obj_set_height(lv_obj_2, 414);
        lv_obj_add_style(lv_obj_2, &cal_panel, 0);
        lv_obj_t * lv_label_4 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_4, "SOURCE & LIMITS");
        lv_obj_set_x(lv_label_4, 12);
        lv_obj_set_y(lv_label_4, 12);
        lv_obj_set_style_text_color(lv_label_4, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_5 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_5, "ADDA SOURCE");
        lv_obj_set_x(lv_label_5, 12);
        lv_obj_set_y(lv_label_5, 42);
        lv_obj_set_style_text_color(lv_label_5, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_source_dropdown = lv_dropdown_create(lv_obj_2);
        lv_obj_set_name(cal_source_dropdown, "cal_source_dropdown");
        lv_obj_set_x(cal_source_dropdown, 12);
        lv_obj_set_y(cal_source_dropdown, 62);
        lv_obj_set_width(cal_source_dropdown, 216);
        lv_obj_set_height(cal_source_dropdown, 38);
        lv_dropdown_set_options(cal_source_dropdown, "DAC1 -> ADC\nDAC2 -> ADC\nExternal Input");
        lv_dropdown_set_selected(cal_source_dropdown, 0);
        lv_obj_add_style(cal_source_dropdown, &cal_field, 0);

        lv_obj_t * lv_label_6 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_6, "CALIBRATION ITEM");
        lv_obj_set_x(lv_label_6, 12);
        lv_obj_set_y(lv_label_6, 112);
        lv_obj_set_style_text_color(lv_label_6, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_item_dropdown = lv_dropdown_create(lv_obj_2);
        lv_obj_set_name(cal_item_dropdown, "cal_item_dropdown");
        lv_obj_set_x(cal_item_dropdown, 12);
        lv_obj_set_y(cal_item_dropdown, 132);
        lv_obj_set_width(cal_item_dropdown, 216);
        lv_obj_set_height(cal_item_dropdown, 38);
        lv_dropdown_set_options(cal_item_dropdown, "Gain / Offset\nFrequency Response\nChannel Amp / Phase\nSafe Amplitude Limits");
        lv_dropdown_set_selected(cal_item_dropdown, 0);
        lv_obj_add_style(cal_item_dropdown, &cal_field, 0);

        lv_obj_t * lv_label_7 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_7, "LOWER LIMIT");
        lv_obj_set_x(lv_label_7, 12);
        lv_obj_set_y(lv_label_7, 184);
        lv_obj_set_style_text_color(lv_label_7, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_lower_limit = lv_textarea_create(lv_obj_2);
        lv_obj_set_name(cal_lower_limit, "cal_lower_limit");
        lv_obj_set_x(cal_lower_limit, 128);
        lv_obj_set_y(cal_lower_limit, 176);
        lv_obj_set_width(cal_lower_limit, 100);
        lv_obj_set_height(cal_lower_limit, 38);
        lv_textarea_set_one_line(cal_lower_limit, true);
        lv_textarea_set_text(cal_lower_limit, "TBD");
        lv_obj_add_style(cal_lower_limit, &cal_field, 0);

        lv_obj_t * lv_label_8 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_8, "UPPER LIMIT");
        lv_obj_set_x(lv_label_8, 12);
        lv_obj_set_y(lv_label_8, 230);
        lv_obj_set_style_text_color(lv_label_8, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_upper_limit = lv_textarea_create(lv_obj_2);
        lv_obj_set_name(cal_upper_limit, "cal_upper_limit");
        lv_obj_set_x(cal_upper_limit, 128);
        lv_obj_set_y(cal_upper_limit, 222);
        lv_obj_set_width(cal_upper_limit, 100);
        lv_obj_set_height(cal_upper_limit, 38);
        lv_textarea_set_one_line(cal_upper_limit, true);
        lv_textarea_set_text(cal_upper_limit, "TBD");
        lv_obj_add_style(cal_upper_limit, &cal_field, 0);

        lv_obj_t * lv_label_9 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_9, "RAMP STEP");
        lv_obj_set_x(lv_label_9, 12);
        lv_obj_set_y(lv_label_9, 276);
        lv_obj_set_style_text_color(lv_label_9, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_ramp_step = lv_textarea_create(lv_obj_2);
        lv_obj_set_name(cal_ramp_step, "cal_ramp_step");
        lv_obj_set_x(cal_ramp_step, 128);
        lv_obj_set_y(cal_ramp_step, 268);
        lv_obj_set_width(cal_ramp_step, 100);
        lv_obj_set_height(cal_ramp_step, 38);
        lv_textarea_set_one_line(cal_ramp_step, true);
        lv_textarea_set_text(cal_ramp_step, "TBD");
        lv_obj_add_style(cal_ramp_step, &cal_field, 0);

        lv_obj_t * lv_label_10 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_10, "DWELL TIME");
        lv_obj_set_x(lv_label_10, 12);
        lv_obj_set_y(lv_label_10, 322);
        lv_obj_set_style_text_color(lv_label_10, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_dwell_time = lv_textarea_create(lv_obj_2);
        lv_obj_set_name(cal_dwell_time, "cal_dwell_time");
        lv_obj_set_x(cal_dwell_time, 128);
        lv_obj_set_y(cal_dwell_time, 314);
        lv_obj_set_width(cal_dwell_time, 100);
        lv_obj_set_height(cal_dwell_time, 38);
        lv_textarea_set_one_line(cal_dwell_time, true);
        lv_textarea_set_text(cal_dwell_time, "TBD");
        lv_obj_add_style(cal_dwell_time, &cal_field, 0);

        lv_obj_t * cal_relay_label = lv_label_create(lv_obj_2);
        lv_obj_set_name(cal_relay_label, "cal_relay_label");
        lv_label_set_text(cal_relay_label, "Relay: P2-18 / P2-16");
        lv_obj_set_x(cal_relay_label, 12);
        lv_obj_set_y(cal_relay_label, 374);
        lv_obj_set_style_text_color(cal_relay_label, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_obj_3 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_3, 256);
        lv_obj_set_y(lv_obj_3, 58);
        lv_obj_set_width(lv_obj_3, 360);
        lv_obj_set_height(lv_obj_3, 414);
        lv_obj_set_flag(lv_obj_3, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_3, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_3, &cal_panel, 0);
        lv_obj_t * lv_label_11 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_11, "MEASUREMENT PROGRESS");
        lv_obj_set_x(lv_label_11, 12);
        lv_obj_set_y(lv_label_11, 12);
        lv_obj_set_style_text_color(lv_label_11, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_12 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_12, "STEP");
        lv_obj_set_x(lv_label_12, 12);
        lv_obj_set_y(lv_label_12, 42);
        lv_obj_set_style_text_color(lv_label_12, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_step_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(cal_step_label, "cal_step_label");
        lv_label_set_text(cal_step_label, "IDLE / WAITING FOR START");
        lv_obj_set_x(cal_step_label, 60);
        lv_obj_set_y(cal_step_label, 42);
        lv_obj_set_style_text_color(cal_step_label, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * cal_progress_bar = lv_bar_create(lv_obj_3);
        lv_obj_set_name(cal_progress_bar, "cal_progress_bar");
        lv_obj_set_x(cal_progress_bar, 12);
        lv_obj_set_y(cal_progress_bar, 68);
        lv_obj_set_width(cal_progress_bar, 336);
        lv_obj_set_height(cal_progress_bar, 18);
        lv_bar_set_min_value(cal_progress_bar, 0);
        lv_bar_set_max_value(cal_progress_bar, 100);
        lv_bar_set_value(cal_progress_bar, 0, false);

        lv_obj_t * cal_progress_chart = lv_chart_create(lv_obj_3);
        lv_obj_set_name(cal_progress_chart, "cal_progress_chart");
        lv_obj_set_x(cal_progress_chart, 12);
        lv_obj_set_y(cal_progress_chart, 100);
        lv_obj_set_width(cal_progress_chart, 336);
        lv_obj_set_height(cal_progress_chart, 180);
        lv_chart_set_type(cal_progress_chart, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(cal_progress_chart, 16);
        lv_chart_set_hor_div_line_count(cal_progress_chart, 4);
        lv_chart_set_ver_div_line_count(cal_progress_chart, 6);
        lv_obj_add_style(cal_progress_chart, &cal_plot, 0);
        lv_obj_add_style(cal_progress_chart, &cal_trace, LV_PART_ITEMS);
        lv_chart_set_axis_min_value(cal_progress_chart, LV_CHART_AXIS_PRIMARY_Y, 0);
        lv_chart_set_axis_max_value(cal_progress_chart, LV_CHART_AXIS_PRIMARY_Y, 100);
        lv_chart_series_t * lv_chart_series_0 = lv_chart_add_series(cal_progress_chart, lv_color_hex(0x80d6ff), LV_CHART_AXIS_PRIMARY_Y);
        static const int32_t cal_progress_chart_values_0[] = {8, 14, 21, 28, 34, 40, 47, 53, 60, 66, 72, 79, 85, 91, 96, 100};
        lv_chart_set_series_values(cal_progress_chart, lv_chart_series_0, cal_progress_chart_values_0, 16);

        lv_obj_t * lv_label_13 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_13, "TARGET");
        lv_obj_set_x(lv_label_13, 12);
        lv_obj_set_y(lv_label_13, 294);
        lv_obj_set_style_text_color(lv_label_13, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_target_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(cal_target_label, "cal_target_label");
        lv_label_set_text(cal_target_label, "--");
        lv_obj_set_x(cal_target_label, 82);
        lv_obj_set_y(cal_target_label, 294);

        lv_obj_t * lv_label_14 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_14, "MEASURED");
        lv_obj_set_x(lv_label_14, 178);
        lv_obj_set_y(lv_label_14, 294);
        lv_obj_set_style_text_color(lv_label_14, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_measured_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(cal_measured_label, "cal_measured_label");
        lv_label_set_text(cal_measured_label, "--");
        lv_obj_set_x(cal_measured_label, 266);
        lv_obj_set_y(cal_measured_label, 294);

        lv_obj_t * lv_label_15 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_15, "POINTS");
        lv_obj_set_x(lv_label_15, 12);
        lv_obj_set_y(lv_label_15, 324);
        lv_obj_set_style_text_color(lv_label_15, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_points_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(cal_points_label, "cal_points_label");
        lv_label_set_text(cal_points_label, "0 / 0");
        lv_obj_set_x(cal_points_label, 82);
        lv_obj_set_y(cal_points_label, 324);

        lv_obj_t * lv_label_16 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_16, "ERROR");
        lv_obj_set_x(lv_label_16, 178);
        lv_obj_set_y(lv_label_16, 324);
        lv_obj_set_style_text_color(lv_label_16, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_error_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(cal_error_label, "cal_error_label");
        lv_label_set_text(cal_error_label, "--");
        lv_obj_set_x(cal_error_label, 246);
        lv_obj_set_y(cal_error_label, 324);

        lv_obj_t * lv_obj_4 = lv_obj_create(lv_obj_3);
        lv_obj_set_x(lv_obj_4, 12);
        lv_obj_set_y(lv_obj_4, 344);
        lv_obj_set_width(lv_obj_4, 336);
        lv_obj_set_height(lv_obj_4, 58);
        lv_obj_set_flag(lv_obj_4, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_4, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_4, &cal_field, 0);
        lv_obj_t * cal_storage_label = lv_label_create(lv_obj_4);
        lv_obj_set_name(cal_storage_label, "cal_storage_label");
        lv_label_set_text(cal_storage_label, "QSPI NOT READY");
        lv_obj_set_x(cal_storage_label, 10);
        lv_obj_set_y(cal_storage_label, 0);
        lv_obj_set_width(cal_storage_label, 316);
        lv_obj_set_style_text_color(cal_storage_label, lv_color_hex(0xff9f43), 0);

        lv_obj_t * cal_profile_label = lv_label_create(lv_obj_4);
        lv_obj_set_name(cal_profile_label, "cal_profile_label");
        lv_label_set_text(cal_profile_label, "ACTIVE: OLD   PENDING: NONE");
        lv_obj_set_x(cal_profile_label, 10);
        lv_obj_set_y(cal_profile_label, 18);
        lv_obj_set_width(cal_profile_label, 316);
        lv_obj_set_style_text_color(cal_profile_label, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_obj_5 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_5, 624);
        lv_obj_set_y(lv_obj_5, 58);
        lv_obj_set_width(lv_obj_5, 168);
        lv_obj_set_height(lv_obj_5, 414);
        lv_obj_set_flag(lv_obj_5, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_5, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_5, &cal_panel, 0);
        lv_obj_t * lv_label_17 = lv_label_create(lv_obj_5);
        lv_label_set_text(lv_label_17, "CAL CONTROL");
        lv_obj_set_x(lv_label_17, 14);
        lv_obj_set_y(lv_label_17, 12);
        lv_obj_set_style_text_color(lv_label_17, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_button_1 = lv_button_create(lv_obj_5);
        lv_obj_set_x(lv_button_1, 8);
        lv_obj_set_y(lv_button_1, 38);
        lv_obj_set_width(lv_button_1, 152);
        lv_obj_set_height(lv_button_1, 36);
        lv_obj_add_style(lv_button_1, &cal_primary, 0);
        lv_obj_t * lv_label_18 = lv_label_create(lv_button_1);
        lv_label_set_text(lv_label_18, "HOME");
        lv_obj_set_align(lv_label_18, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_1, LV_EVENT_CLICKED, screen_results_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * cal_start_button = lv_button_create(lv_obj_5);
        lv_obj_set_name(cal_start_button, "cal_start_button");
        lv_obj_set_x(cal_start_button, 8);
        lv_obj_set_y(cal_start_button, 82);
        lv_obj_set_width(cal_start_button, 152);
        lv_obj_set_height(cal_start_button, 38);
        lv_obj_add_style(cal_start_button, &cal_safe, 0);
        lv_obj_t * lv_label_19 = lv_label_create(cal_start_button);
        lv_label_set_text(lv_label_19, "START");
        lv_obj_set_align(lv_label_19, LV_ALIGN_CENTER);

        lv_obj_t * cal_stop_button = lv_button_create(lv_obj_5);
        lv_obj_set_name(cal_stop_button, "cal_stop_button");
        lv_obj_set_x(cal_stop_button, 8);
        lv_obj_set_y(cal_stop_button, 126);
        lv_obj_set_width(cal_stop_button, 152);
        lv_obj_set_height(cal_stop_button, 38);
        lv_obj_add_style(cal_stop_button, &cal_stop, 0);
        lv_obj_t * lv_label_20 = lv_label_create(cal_stop_button);
        lv_label_set_text(lv_label_20, "STOP");
        lv_obj_set_align(lv_label_20, LV_ALIGN_CENTER);

        lv_obj_t * cal_export_button = lv_button_create(lv_obj_5);
        lv_obj_set_name(cal_export_button, "cal_export_button");
        lv_obj_set_x(cal_export_button, 8);
        lv_obj_set_y(cal_export_button, 176);
        lv_obj_set_width(cal_export_button, 152);
        lv_obj_set_height(cal_export_button, 38);
        lv_obj_add_style(cal_export_button, &cal_field, 0);
        lv_obj_t * lv_label_21 = lv_label_create(cal_export_button);
        lv_label_set_text(lv_label_21, "EXPORT DATA");
        lv_obj_set_align(lv_label_21, LV_ALIGN_CENTER);

        lv_obj_t * cal_import_button = lv_button_create(lv_obj_5);
        lv_obj_set_name(cal_import_button, "cal_import_button");
        lv_obj_set_x(cal_import_button, 8);
        lv_obj_set_y(cal_import_button, 220);
        lv_obj_set_width(cal_import_button, 152);
        lv_obj_set_height(cal_import_button, 38);
        lv_obj_add_style(cal_import_button, &cal_field, 0);
        lv_obj_t * lv_label_22 = lv_label_create(cal_import_button);
        lv_label_set_text(lv_label_22, "IMPORT RESULT");
        lv_obj_set_align(lv_label_22, LV_ALIGN_CENTER);

        lv_obj_t * cal_confirm_button = lv_button_create(lv_obj_5);
        lv_obj_set_name(cal_confirm_button, "cal_confirm_button");
        lv_obj_set_x(cal_confirm_button, 8);
        lv_obj_set_y(cal_confirm_button, 270);
        lv_obj_set_width(cal_confirm_button, 72);
        lv_obj_set_height(cal_confirm_button, 38);
        lv_obj_add_style(cal_confirm_button, &cal_safe, 0);
        lv_obj_t * lv_label_23 = lv_label_create(cal_confirm_button);
        lv_label_set_text(lv_label_23, "CONFIRM");
        lv_obj_set_align(lv_label_23, LV_ALIGN_CENTER);

        lv_obj_t * cal_cancel_button = lv_button_create(lv_obj_5);
        lv_obj_set_name(cal_cancel_button, "cal_cancel_button");
        lv_obj_set_x(cal_cancel_button, 88);
        lv_obj_set_y(cal_cancel_button, 270);
        lv_obj_set_width(cal_cancel_button, 72);
        lv_obj_set_height(cal_cancel_button, 38);
        lv_obj_add_style(cal_cancel_button, &cal_stop, 0);
        lv_obj_t * lv_label_24 = lv_label_create(cal_cancel_button);
        lv_label_set_text(lv_label_24, "CANCEL");
        lv_obj_set_align(lv_label_24, LV_ALIGN_CENTER);

        lv_obj_t * cal_apply_button = lv_button_create(lv_obj_5);
        lv_obj_set_name(cal_apply_button, "cal_apply_button");
        lv_obj_set_x(cal_apply_button, 8);
        lv_obj_set_y(cal_apply_button, 318);
        lv_obj_set_width(cal_apply_button, 152);
        lv_obj_set_height(cal_apply_button, 44);
        lv_obj_add_style(cal_apply_button, &cal_pending, 0);
        lv_obj_t * lv_label_25 = lv_label_create(cal_apply_button);
        lv_label_set_text(lv_label_25, "APPLY NOW");
        lv_obj_set_align(lv_label_25, LV_ALIGN_CENTER);

        lv_obj_t * lv_label_26 = lv_label_create(lv_obj_5);
        lv_label_set_text(lv_label_26, "After apply:");
        lv_obj_set_x(lv_label_26, 12);
        lv_obj_set_y(lv_label_26, 372);
        lv_obj_set_style_text_color(lv_label_26, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_apply_hint = lv_label_create(lv_obj_5);
        lv_obj_set_name(cal_apply_hint, "cal_apply_hint");
        lv_label_set_text(cal_apply_hint, "UNDO APPLY");
        lv_obj_set_x(cal_apply_hint, 12);
        lv_obj_set_y(cal_apply_hint, 392);
        lv_obj_set_style_text_color(cal_apply_hint, lv_color_hex(0xffd43b), 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

