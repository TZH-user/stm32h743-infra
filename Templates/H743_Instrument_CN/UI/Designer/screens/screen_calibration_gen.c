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

    static lv_style_t result_cal_panel;
    static lv_style_t result_cal_field;
    static lv_style_t result_cal_primary;
    static lv_style_t result_cal_safe;
    static lv_style_t result_cal_stop;
    static lv_style_t result_cal_pending;

    static bool style_inited = false;

    if (!style_inited) {
        #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
        if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
            lv_style_init(&result_cal_panel);
            lv_style_set_bg_color(&result_cal_panel, lv_color_hex(0x16191d));
            lv_style_set_bg_opa(&result_cal_panel, (255 * 100 / 100));
            lv_style_set_border_color(&result_cal_panel, lv_color_hex(0x3b4148));
            lv_style_set_border_width(&result_cal_panel, 1);
            lv_style_set_radius(&result_cal_panel, 3);
            lv_style_set_pad_all(&result_cal_panel, 0);

            lv_style_init(&result_cal_field);
            lv_style_set_bg_color(&result_cal_field, lv_color_hex(0x272c32));
            lv_style_set_bg_opa(&result_cal_field, (255 * 100 / 100));
            lv_style_set_border_color(&result_cal_field, lv_color_hex(0x626b75));
            lv_style_set_border_width(&result_cal_field, 1);
            lv_style_set_radius(&result_cal_field, 3);
            lv_style_set_text_color(&result_cal_field, lv_color_hex(0xf2f3f4));

            lv_style_init(&result_cal_primary);
            lv_style_set_bg_color(&result_cal_primary, lv_color_hex(0x176b87));
            lv_style_set_bg_opa(&result_cal_primary, (255 * 100 / 100));
            lv_style_set_border_color(&result_cal_primary, lv_color_hex(0x80d6ff));
            lv_style_set_border_width(&result_cal_primary, 1);
            lv_style_set_radius(&result_cal_primary, 3);
            lv_style_set_text_color(&result_cal_primary, lv_color_hex(0xffffff));

            lv_style_init(&result_cal_safe);
            lv_style_set_bg_color(&result_cal_safe, lv_color_hex(0x2abf70));
            lv_style_set_bg_opa(&result_cal_safe, (255 * 100 / 100));
            lv_style_set_border_width(&result_cal_safe, 0);
            lv_style_set_radius(&result_cal_safe, 3);
            lv_style_set_text_color(&result_cal_safe, lv_color_hex(0x04130b));

            lv_style_init(&result_cal_stop);
            lv_style_set_bg_color(&result_cal_stop, lv_color_hex(0xd84b4b));
            lv_style_set_bg_opa(&result_cal_stop, (255 * 100 / 100));
            lv_style_set_border_width(&result_cal_stop, 0);
            lv_style_set_radius(&result_cal_stop, 3);
            lv_style_set_text_color(&result_cal_stop, lv_color_hex(0xffffff));

            lv_style_init(&result_cal_pending);
            lv_style_set_bg_color(&result_cal_pending, lv_color_hex(0x7a5b16));
            lv_style_set_bg_opa(&result_cal_pending, (255 * 100 / 100));
            lv_style_set_border_color(&result_cal_pending, lv_color_hex(0xffd43b));
            lv_style_set_border_width(&result_cal_pending, 1);
            lv_style_set_radius(&result_cal_pending, 3);
            lv_style_set_text_color(&result_cal_pending, lv_color_hex(0xffffff));

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
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_1, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_1, &result_cal_panel, 0);
        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_1);
        lv_label_set_text(lv_label_0, "AD / DA 校准");
        lv_obj_set_x(lv_label_0, 16);
        lv_obj_set_y(lv_label_0, 16);
        lv_obj_set_style_text_color(lv_label_0, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * cal_internal_button = lv_button_create(lv_obj_1);
        lv_obj_set_name(cal_internal_button, "cal_internal_button");
        lv_obj_set_x(cal_internal_button, 320);
        lv_obj_set_y(cal_internal_button, 7);
        lv_obj_set_width(cal_internal_button, 108);
        lv_obj_set_height(cal_internal_button, 36);
        lv_obj_add_style(cal_internal_button, &result_cal_primary, 0);
        lv_obj_t * lv_label_1 = lv_label_create(cal_internal_button);
        lv_label_set_text(lv_label_1, "内部");
        lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);

        lv_obj_t * cal_external_button = lv_button_create(lv_obj_1);
        lv_obj_set_name(cal_external_button, "cal_external_button");
        lv_obj_set_x(cal_external_button, 436);
        lv_obj_set_y(cal_external_button, 7);
        lv_obj_set_width(cal_external_button, 108);
        lv_obj_set_height(cal_external_button, 36);
        lv_obj_add_style(cal_external_button, &result_cal_field, 0);
        lv_obj_t * lv_label_2 = lv_label_create(cal_external_button);
        lv_label_set_text(lv_label_2, "外部");
        lv_obj_set_align(lv_label_2, LV_ALIGN_CENTER);

        lv_obj_t * cal_exclusive_label = lv_label_create(lv_obj_1);
        lv_obj_set_name(cal_exclusive_label, "cal_exclusive_label");
        lv_label_set_text(cal_exclusive_label, "独占");
        lv_obj_set_x(cal_exclusive_label, 580);
        lv_obj_set_y(cal_exclusive_label, 16);
        lv_obj_set_style_text_color(cal_exclusive_label, lv_color_hex(0xffd43b), 0);

        lv_obj_t * cal_safe_label = lv_label_create(lv_obj_1);
        lv_obj_set_name(cal_safe_label, "cal_safe_label");
        lv_label_set_text(cal_safe_label, "安全");
        lv_obj_set_x(cal_safe_label, 738);
        lv_obj_set_y(cal_safe_label, 16);
        lv_obj_set_style_text_color(cal_safe_label, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 8);
        lv_obj_set_y(lv_obj_2, 58);
        lv_obj_set_width(lv_obj_2, 224);
        lv_obj_set_height(lv_obj_2, 358);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_2, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_2, &result_cal_panel, 0);
        lv_obj_t * lv_label_3 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_3, "信号源与限幅");
        lv_obj_set_x(lv_label_3, 12);
        lv_obj_set_y(lv_label_3, 10);
        lv_obj_set_style_text_color(lv_label_3, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_4 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_4, "ADDA 信号源");
        lv_obj_set_x(lv_label_4, 12);
        lv_obj_set_y(lv_label_4, 38);
        lv_obj_set_style_text_color(lv_label_4, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_source_dropdown = lv_dropdown_create(lv_obj_2);
        lv_obj_set_name(cal_source_dropdown, "cal_source_dropdown");
        lv_obj_set_x(cal_source_dropdown, 12);
        lv_obj_set_y(cal_source_dropdown, 56);
        lv_obj_set_width(cal_source_dropdown, 200);
        lv_obj_set_height(cal_source_dropdown, 36);
        lv_dropdown_set_options(cal_source_dropdown, "DAC1 -> ADC\nDAC2 -> ADC\n外部输入");
        lv_dropdown_set_selected(cal_source_dropdown, 0);
        lv_obj_add_style(cal_source_dropdown, &result_cal_field, 0);

        lv_obj_t * lv_label_5 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_5, "校准项目");
        lv_obj_set_x(lv_label_5, 12);
        lv_obj_set_y(lv_label_5, 102);
        lv_obj_set_style_text_color(lv_label_5, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_item_dropdown = lv_dropdown_create(lv_obj_2);
        lv_obj_set_name(cal_item_dropdown, "cal_item_dropdown");
        lv_obj_set_x(cal_item_dropdown, 12);
        lv_obj_set_y(cal_item_dropdown, 120);
        lv_obj_set_width(cal_item_dropdown, 200);
        lv_obj_set_height(cal_item_dropdown, 36);
        lv_dropdown_set_options(cal_item_dropdown, "增益 / 偏置\n频率响应\n通道幅相\n安全幅度限值");
        lv_dropdown_set_selected(cal_item_dropdown, 0);
        lv_obj_add_style(cal_item_dropdown, &result_cal_field, 0);

        lv_obj_t * lv_label_6 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_6, "下限");
        lv_obj_set_x(lv_label_6, 12);
        lv_obj_set_y(lv_label_6, 170);
        lv_obj_set_style_text_color(lv_label_6, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_lower_limit = lv_textarea_create(lv_obj_2);
        lv_obj_set_name(cal_lower_limit, "cal_lower_limit");
        lv_obj_set_x(cal_lower_limit, 118);
        lv_obj_set_y(cal_lower_limit, 162);
        lv_obj_set_width(cal_lower_limit, 94);
        lv_obj_set_height(cal_lower_limit, 34);
        lv_textarea_set_one_line(cal_lower_limit, true);
        lv_textarea_set_text(cal_lower_limit, "0.00 V");
        lv_obj_add_style(cal_lower_limit, &result_cal_field, 0);

        lv_obj_t * lv_label_7 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_7, "上限");
        lv_obj_set_x(lv_label_7, 12);
        lv_obj_set_y(lv_label_7, 210);
        lv_obj_set_style_text_color(lv_label_7, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_upper_limit = lv_textarea_create(lv_obj_2);
        lv_obj_set_name(cal_upper_limit, "cal_upper_limit");
        lv_obj_set_x(cal_upper_limit, 118);
        lv_obj_set_y(cal_upper_limit, 202);
        lv_obj_set_width(cal_upper_limit, 94);
        lv_obj_set_height(cal_upper_limit, 34);
        lv_textarea_set_one_line(cal_upper_limit, true);
        lv_textarea_set_text(cal_upper_limit, "3.00 V");
        lv_obj_add_style(cal_upper_limit, &result_cal_field, 0);

        lv_obj_t * lv_label_8 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_8, "爬升步进");
        lv_obj_set_x(lv_label_8, 12);
        lv_obj_set_y(lv_label_8, 250);
        lv_obj_set_style_text_color(lv_label_8, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_ramp_step = lv_textarea_create(lv_obj_2);
        lv_obj_set_name(cal_ramp_step, "cal_ramp_step");
        lv_obj_set_x(cal_ramp_step, 118);
        lv_obj_set_y(cal_ramp_step, 242);
        lv_obj_set_width(cal_ramp_step, 94);
        lv_obj_set_height(cal_ramp_step, 34);
        lv_textarea_set_one_line(cal_ramp_step, true);
        lv_textarea_set_text(cal_ramp_step, "50 mV");
        lv_obj_add_style(cal_ramp_step, &result_cal_field, 0);

        lv_obj_t * lv_label_9 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_9, "驻留时间");
        lv_obj_set_x(lv_label_9, 12);
        lv_obj_set_y(lv_label_9, 290);
        lv_obj_set_style_text_color(lv_label_9, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_dwell_time = lv_textarea_create(lv_obj_2);
        lv_obj_set_name(cal_dwell_time, "cal_dwell_time");
        lv_obj_set_x(cal_dwell_time, 118);
        lv_obj_set_y(cal_dwell_time, 282);
        lv_obj_set_width(cal_dwell_time, 94);
        lv_obj_set_height(cal_dwell_time, 34);
        lv_textarea_set_one_line(cal_dwell_time, true);
        lv_textarea_set_text(cal_dwell_time, "25 ms");
        lv_obj_add_style(cal_dwell_time, &result_cal_field, 0);

        lv_obj_t * cal_relay_label = lv_label_create(lv_obj_2);
        lv_obj_set_name(cal_relay_label, "cal_relay_label");
        lv_label_set_text(cal_relay_label, "继电器: P2-18 / P2-16");
        lv_obj_set_x(cal_relay_label, 12);
        lv_obj_set_y(cal_relay_label, 330);
        lv_obj_set_width(cal_relay_label, 200);
        lv_obj_set_style_text_color(cal_relay_label, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_obj_3 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_3, 240);
        lv_obj_set_y(lv_obj_3, 58);
        lv_obj_set_width(lv_obj_3, 360);
        lv_obj_set_height(lv_obj_3, 358);
        lv_obj_set_flag(lv_obj_3, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_3, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_3, &result_cal_panel, 0);
        lv_obj_t * lv_label_10 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_10, "校准会话");
        lv_obj_set_x(lv_label_10, 12);
        lv_obj_set_y(lv_label_10, 10);
        lv_obj_set_style_text_color(lv_label_10, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_11 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_11, "步骤");
        lv_obj_set_x(lv_label_11, 12);
        lv_obj_set_y(lv_label_11, 36);
        lv_obj_set_style_text_color(lv_label_11, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_step_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(cal_step_label, "cal_step_label");
        lv_label_set_text(cal_step_label, "空闲 / 等待启动");
        lv_obj_set_x(cal_step_label, 60);
        lv_obj_set_y(cal_step_label, 36);
        lv_obj_set_width(cal_step_label, 214);
        lv_obj_set_style_text_color(cal_step_label, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_label_12 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_12, "点数");
        lv_obj_set_x(lv_label_12, 278);
        lv_obj_set_y(lv_label_12, 36);
        lv_obj_set_style_text_color(lv_label_12, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_points_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(cal_points_label, "cal_points_label");
        lv_label_set_text(cal_points_label, "0 / --");
        lv_obj_set_x(cal_points_label, 312);
        lv_obj_set_y(cal_points_label, 36);
        lv_obj_set_width(cal_points_label, 40);

        lv_obj_t * cal_progress_bar = lv_bar_create(lv_obj_3);
        lv_obj_set_name(cal_progress_bar, "cal_progress_bar");
        lv_obj_set_x(cal_progress_bar, 12);
        lv_obj_set_y(cal_progress_bar, 58);
        lv_obj_set_width(cal_progress_bar, 336);
        lv_obj_set_height(cal_progress_bar, 14);
        lv_bar_set_min_value(cal_progress_bar, 0);
        lv_bar_set_max_value(cal_progress_bar, 100);
        lv_bar_set_value(cal_progress_bar, 0, false);

        lv_obj_t * lv_obj_4 = lv_obj_create(lv_obj_3);
        lv_obj_set_x(lv_obj_4, 12);
        lv_obj_set_y(lv_obj_4, 86);
        lv_obj_set_width(lv_obj_4, 336);
        lv_obj_set_height(lv_obj_4, 176);
        lv_obj_set_flag(lv_obj_4, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_4, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_4, &result_cal_field, 0);
        lv_obj_t * lv_label_13 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_13, "校准数据");
        lv_obj_set_x(lv_label_13, 10);
        lv_obj_set_y(lv_label_13, 8);
        lv_obj_set_style_text_color(lv_label_13, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_14 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_14, "DAC 增益 / 偏置");
        lv_obj_set_x(lv_label_14, 10);
        lv_obj_set_y(lv_label_14, 36);
        lv_obj_set_width(lv_label_14, 194);
        lv_obj_set_style_text_color(lv_label_14, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_15 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_15, "未实现");
        lv_obj_set_x(lv_label_15, 208);
        lv_obj_set_y(lv_label_15, 36);
        lv_obj_set_width(lv_label_15, 114);
        lv_obj_set_style_text_align(lv_label_15, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * lv_label_16 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_16, "ADC 增益 / 偏置");
        lv_obj_set_x(lv_label_16, 10);
        lv_obj_set_y(lv_label_16, 62);
        lv_obj_set_width(lv_label_16, 194);
        lv_obj_set_style_text_color(lv_label_16, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_17 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_17, "未实现");
        lv_obj_set_x(lv_label_17, 208);
        lv_obj_set_y(lv_label_17, 62);
        lv_obj_set_width(lv_label_17, 114);
        lv_obj_set_style_text_align(lv_label_17, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * lv_label_18 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_18, "频率响应");
        lv_obj_set_x(lv_label_18, 10);
        lv_obj_set_y(lv_label_18, 88);
        lv_obj_set_width(lv_label_18, 194);
        lv_obj_set_style_text_color(lv_label_18, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_19 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_19, "未实现");
        lv_obj_set_x(lv_label_19, 208);
        lv_obj_set_y(lv_label_19, 88);
        lv_obj_set_width(lv_label_19, 114);
        lv_obj_set_style_text_align(lv_label_19, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * lv_label_20 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_20, "通道幅相");
        lv_obj_set_x(lv_label_20, 10);
        lv_obj_set_y(lv_label_20, 114);
        lv_obj_set_width(lv_label_20, 194);
        lv_obj_set_style_text_color(lv_label_20, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_21 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_21, "未实现");
        lv_obj_set_x(lv_label_21, 208);
        lv_obj_set_y(lv_label_21, 114);
        lv_obj_set_width(lv_label_21, 114);
        lv_obj_set_style_text_align(lv_label_21, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * lv_label_22 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_22, "安全幅度限值");
        lv_obj_set_x(lv_label_22, 10);
        lv_obj_set_y(lv_label_22, 140);
        lv_obj_set_width(lv_label_22, 194);
        lv_obj_set_style_text_color(lv_label_22, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_23 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_23, "暂定");
        lv_obj_set_x(lv_label_23, 208);
        lv_obj_set_y(lv_label_23, 140);
        lv_obj_set_width(lv_label_23, 114);
        lv_obj_set_style_text_align(lv_label_23, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(lv_label_23, lv_color_hex(0xff9f43), 0);

        lv_obj_t * lv_label_24 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_24, "错误");
        lv_obj_set_x(lv_label_24, 12);
        lv_obj_set_y(lv_label_24, 274);
        lv_obj_set_style_text_color(lv_label_24, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_error_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(cal_error_label, "cal_error_label");
        lv_label_set_text(cal_error_label, "--");
        lv_obj_set_x(cal_error_label, 70);
        lv_obj_set_y(cal_error_label, 274);
        lv_obj_set_width(cal_error_label, 278);

        lv_obj_t * lv_obj_5 = lv_obj_create(lv_obj_3);
        lv_obj_set_x(lv_obj_5, 12);
        lv_obj_set_y(lv_obj_5, 294);
        lv_obj_set_width(lv_obj_5, 336);
        lv_obj_set_height(lv_obj_5, 54);
        lv_obj_set_flag(lv_obj_5, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_5, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_5, &result_cal_field, 0);
        lv_obj_t * cal_storage_label = lv_label_create(lv_obj_5);
        lv_obj_set_name(cal_storage_label, "cal_storage_label");
        lv_label_set_text(cal_storage_label, "QSPI 未就绪");
        lv_obj_set_x(cal_storage_label, 10);
        lv_obj_set_y(cal_storage_label, 2);
        lv_obj_set_width(cal_storage_label, 316);
        lv_obj_set_style_text_color(cal_storage_label, lv_color_hex(0xff9f43), 0);

        lv_obj_t * cal_profile_label = lv_label_create(lv_obj_5);
        lv_obj_set_name(cal_profile_label, "cal_profile_label");
        lv_label_set_text(cal_profile_label, "生效: 旧   待用: 无");
        lv_obj_set_x(cal_profile_label, 10);
        lv_obj_set_y(cal_profile_label, 24);
        lv_obj_set_width(cal_profile_label, 316);
        lv_obj_set_style_text_color(cal_profile_label, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_obj_6 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_6, 608);
        lv_obj_set_y(lv_obj_6, 58);
        lv_obj_set_width(lv_obj_6, 184);
        lv_obj_set_height(lv_obj_6, 358);
        lv_obj_set_flag(lv_obj_6, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_6, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_6, &result_cal_panel, 0);
        lv_obj_t * lv_label_25 = lv_label_create(lv_obj_6);
        lv_label_set_text(lv_label_25, "校准控制");
        lv_obj_set_x(lv_label_25, 14);
        lv_obj_set_y(lv_label_25, 10);
        lv_obj_set_style_text_color(lv_label_25, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_button_0 = lv_button_create(lv_obj_6);
        lv_obj_set_x(lv_button_0, 8);
        lv_obj_set_y(lv_button_0, 32);
        lv_obj_set_width(lv_button_0, 168);
        lv_obj_set_height(lv_button_0, 32);
        lv_obj_add_style(lv_button_0, &result_cal_primary, 0);
        lv_obj_t * lv_label_26 = lv_label_create(lv_button_0);
        lv_label_set_text(lv_label_26, "主页");
        lv_obj_set_align(lv_label_26, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_0, LV_EVENT_CLICKED, screen_control_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * cal_start_button = lv_button_create(lv_obj_6);
        lv_obj_set_name(cal_start_button, "cal_start_button");
        lv_obj_set_x(cal_start_button, 8);
        lv_obj_set_y(cal_start_button, 70);
        lv_obj_set_width(cal_start_button, 168);
        lv_obj_set_height(cal_start_button, 34);
        lv_obj_add_style(cal_start_button, &result_cal_safe, 0);
        lv_obj_t * lv_label_27 = lv_label_create(cal_start_button);
        lv_label_set_text(lv_label_27, "启动");
        lv_obj_set_align(lv_label_27, LV_ALIGN_CENTER);

        lv_obj_t * cal_stop_button = lv_button_create(lv_obj_6);
        lv_obj_set_name(cal_stop_button, "cal_stop_button");
        lv_obj_set_x(cal_stop_button, 8);
        lv_obj_set_y(cal_stop_button, 110);
        lv_obj_set_width(cal_stop_button, 168);
        lv_obj_set_height(cal_stop_button, 34);
        lv_obj_add_style(cal_stop_button, &result_cal_stop, 0);
        lv_obj_t * lv_label_28 = lv_label_create(cal_stop_button);
        lv_label_set_text(lv_label_28, "停止");
        lv_obj_set_align(lv_label_28, LV_ALIGN_CENTER);

        lv_obj_t * cal_export_button = lv_button_create(lv_obj_6);
        lv_obj_set_name(cal_export_button, "cal_export_button");
        lv_obj_set_x(cal_export_button, 8);
        lv_obj_set_y(cal_export_button, 152);
        lv_obj_set_width(cal_export_button, 168);
        lv_obj_set_height(cal_export_button, 34);
        lv_obj_add_style(cal_export_button, &result_cal_field, 0);
        lv_obj_t * lv_label_29 = lv_label_create(cal_export_button);
        lv_label_set_text(lv_label_29, "导出数据");
        lv_obj_set_align(lv_label_29, LV_ALIGN_CENTER);

        lv_obj_t * cal_import_button = lv_button_create(lv_obj_6);
        lv_obj_set_name(cal_import_button, "cal_import_button");
        lv_obj_set_x(cal_import_button, 8);
        lv_obj_set_y(cal_import_button, 192);
        lv_obj_set_width(cal_import_button, 168);
        lv_obj_set_height(cal_import_button, 34);
        lv_obj_add_style(cal_import_button, &result_cal_field, 0);
        lv_obj_t * lv_label_30 = lv_label_create(cal_import_button);
        lv_label_set_text(lv_label_30, "导入结果");
        lv_obj_set_align(lv_label_30, LV_ALIGN_CENTER);

        lv_obj_t * cal_confirm_button = lv_button_create(lv_obj_6);
        lv_obj_set_name(cal_confirm_button, "cal_confirm_button");
        lv_obj_set_x(cal_confirm_button, 8);
        lv_obj_set_y(cal_confirm_button, 234);
        lv_obj_set_width(cal_confirm_button, 80);
        lv_obj_set_height(cal_confirm_button, 34);
        lv_obj_add_style(cal_confirm_button, &result_cal_safe, 0);
        lv_obj_t * lv_label_31 = lv_label_create(cal_confirm_button);
        lv_label_set_text(lv_label_31, "确认");
        lv_obj_set_align(lv_label_31, LV_ALIGN_CENTER);

        lv_obj_t * cal_cancel_button = lv_button_create(lv_obj_6);
        lv_obj_set_name(cal_cancel_button, "cal_cancel_button");
        lv_obj_set_x(cal_cancel_button, 96);
        lv_obj_set_y(cal_cancel_button, 234);
        lv_obj_set_width(cal_cancel_button, 80);
        lv_obj_set_height(cal_cancel_button, 34);
        lv_obj_add_style(cal_cancel_button, &result_cal_stop, 0);
        lv_obj_t * lv_label_32 = lv_label_create(cal_cancel_button);
        lv_label_set_text(lv_label_32, "取消");
        lv_obj_set_align(lv_label_32, LV_ALIGN_CENTER);

        lv_obj_t * cal_apply_button = lv_button_create(lv_obj_6);
        lv_obj_set_name(cal_apply_button, "cal_apply_button");
        lv_obj_set_x(cal_apply_button, 8);
        lv_obj_set_y(cal_apply_button, 278);
        lv_obj_set_width(cal_apply_button, 168);
        lv_obj_set_height(cal_apply_button, 38);
        lv_obj_add_style(cal_apply_button, &result_cal_pending, 0);
        lv_obj_t * lv_label_33 = lv_label_create(cal_apply_button);
        lv_label_set_text(lv_label_33, "立即应用");
        lv_obj_set_align(lv_label_33, LV_ALIGN_CENTER);

        lv_obj_t * lv_label_34 = lv_label_create(lv_obj_6);
        lv_label_set_text(lv_label_34, "应用后:");
        lv_obj_set_x(lv_label_34, 12);
        lv_obj_set_y(lv_label_34, 324);
        lv_obj_set_style_text_color(lv_label_34, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * cal_apply_hint = lv_label_create(lv_obj_6);
        lv_obj_set_name(cal_apply_hint, "cal_apply_hint");
        lv_label_set_text(cal_apply_hint, "撤销应用");
        lv_obj_set_x(cal_apply_hint, 100);
        lv_obj_set_y(cal_apply_hint, 324);
        lv_obj_set_width(cal_apply_hint, 72);
        lv_obj_set_style_text_align(cal_apply_hint, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(cal_apply_hint, lv_color_hex(0xffd43b), 0);

        lv_obj_t * lv_obj_7 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_7, 0);
        lv_obj_set_y(lv_obj_7, 424);
        lv_obj_set_flag(lv_obj_7, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_7, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_7, &style_nav, 0);
        lv_obj_t * lv_button_1 = lv_button_create(lv_obj_7);
        lv_obj_set_x(lv_button_1, 16);
        lv_obj_set_y(lv_button_1, 8);
        lv_obj_set_width(lv_button_1, 180);
        lv_obj_set_height(lv_button_1, 40);
        lv_obj_add_style(lv_button_1, &style_nav_button, 0);
        lv_obj_t * lv_label_35 = lv_label_create(lv_button_1);
        lv_label_set_text(lv_label_35, "监测");
        lv_obj_set_align(lv_label_35, LV_ALIGN_CENTER);

        lv_obj_add_screen_load_event(lv_button_1, LV_EVENT_CLICKED, screen_monitor, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * lv_button_2 = lv_button_create(lv_obj_7);
        lv_obj_set_x(lv_button_2, 212);
        lv_obj_set_y(lv_button_2, 8);
        lv_obj_set_width(lv_button_2, 180);
        lv_obj_set_height(lv_button_2, 40);
        lv_obj_add_style(lv_button_2, &style_nav_button, 0);
        lv_obj_t * lv_label_36 = lv_label_create(lv_button_2);
        lv_label_set_text(lv_label_36, "测试");
        lv_obj_set_align(lv_label_36, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_2, LV_EVENT_CLICKED, screen_test_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * lv_button_3 = lv_button_create(lv_obj_7);
        lv_obj_set_x(lv_button_3, 408);
        lv_obj_set_y(lv_button_3, 8);
        lv_obj_set_width(lv_button_3, 180);
        lv_obj_set_height(lv_button_3, 40);
        lv_obj_add_style(lv_button_3, &style_nav_button, 0);
        lv_obj_add_style(lv_button_3, &style_nav_button_active, 0);
        lv_obj_t * lv_label_37 = lv_label_create(lv_button_3);
        lv_label_set_text(lv_label_37, "校准");
        lv_obj_set_align(lv_label_37, LV_ALIGN_CENTER);

        lv_obj_t * lv_button_4 = lv_button_create(lv_obj_7);
        lv_obj_set_x(lv_button_4, 604);
        lv_obj_set_y(lv_button_4, 8);
        lv_obj_set_width(lv_button_4, 180);
        lv_obj_set_height(lv_button_4, 40);
        lv_obj_add_style(lv_button_4, &style_nav_button, 0);
        lv_obj_t * lv_label_38 = lv_label_create(lv_button_4);
        lv_label_set_text(lv_label_38, "系统");
        lv_obj_set_align(lv_label_38, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_4, LV_EVENT_CLICKED, screen_system_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

