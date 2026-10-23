/**
 * @file screen_test_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_test_gen.h"
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

lv_obj_t * screen_test_create(void)
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
        lv_obj_set_name_static(lv_obj_0, "screen_test_#");

        lv_obj_add_style(lv_obj_0, &style_screen, 0);
        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_1, 0);
        lv_obj_set_y(lv_obj_1, 0);
        lv_obj_set_width(lv_obj_1, 800);
        lv_obj_set_height(lv_obj_1, 50);
        lv_obj_set_flag(lv_obj_1, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_1, LV_SCROLLBAR_MODE_OFF);
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
        lv_label_set_text(lv_label_1, "AD / DA TEST");
        lv_obj_set_x(lv_label_1, 112);
        lv_obj_set_y(lv_label_1, 16);
        lv_obj_set_style_text_color(lv_label_1, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * test_internal_button = lv_button_create(lv_obj_1);
        lv_obj_set_name(test_internal_button, "test_internal_button");
        lv_obj_set_x(test_internal_button, 328);
        lv_obj_set_y(test_internal_button, 7);
        lv_obj_set_width(test_internal_button, 116);
        lv_obj_set_height(test_internal_button, 36);
        lv_obj_add_style(test_internal_button, &cal_primary, 0);
        lv_obj_t * lv_label_2 = lv_label_create(test_internal_button);
        lv_label_set_text(lv_label_2, "INTERNAL");
        lv_obj_set_align(lv_label_2, LV_ALIGN_CENTER);

        lv_obj_t * test_external_button = lv_button_create(lv_obj_1);
        lv_obj_set_name(test_external_button, "test_external_button");
        lv_obj_set_x(test_external_button, 452);
        lv_obj_set_y(test_external_button, 7);
        lv_obj_set_width(test_external_button, 116);
        lv_obj_set_height(test_external_button, 36);
        lv_obj_add_style(test_external_button, &cal_field, 0);
        lv_obj_t * lv_label_3 = lv_label_create(test_external_button);
        lv_label_set_text(lv_label_3, "EXTERNAL");
        lv_obj_set_align(lv_label_3, LV_ALIGN_CENTER);

        lv_obj_t * test_exclusive_label = lv_label_create(lv_obj_1);
        lv_obj_set_name(test_exclusive_label, "test_exclusive_label");
        lv_label_set_text(test_exclusive_label, "EXCLUSIVE");
        lv_obj_set_x(test_exclusive_label, 610);
        lv_obj_set_y(test_exclusive_label, 16);
        lv_obj_set_style_text_color(test_exclusive_label, lv_color_hex(0xffd43b), 0);

        lv_obj_t * test_safe_label = lv_label_create(lv_obj_1);
        lv_obj_set_name(test_safe_label, "test_safe_label");
        lv_label_set_text(test_safe_label, "SAFE");
        lv_obj_set_x(test_safe_label, 742);
        lv_obj_set_y(test_safe_label, 16);
        lv_obj_set_style_text_color(test_safe_label, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 8);
        lv_obj_set_y(lv_obj_2, 58);
        lv_obj_set_width(lv_obj_2, 190);
        lv_obj_set_height(lv_obj_2, 414);
        lv_obj_set_flag(lv_obj_2, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_2, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_2, &cal_panel, 0);
        lv_obj_t * lv_label_4 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_4, "TEST SETUP");
        lv_obj_set_x(lv_label_4, 12);
        lv_obj_set_y(lv_label_4, 12);
        lv_obj_set_style_text_color(lv_label_4, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_5 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_5, "ADDA SOURCE");
        lv_obj_set_x(lv_label_5, 12);
        lv_obj_set_y(lv_label_5, 46);
        lv_obj_set_style_text_color(lv_label_5, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * test_source_dropdown = lv_dropdown_create(lv_obj_2);
        lv_obj_set_name(test_source_dropdown, "test_source_dropdown");
        lv_obj_set_x(test_source_dropdown, 12);
        lv_obj_set_y(test_source_dropdown, 66);
        lv_obj_set_width(test_source_dropdown, 166);
        lv_obj_set_height(test_source_dropdown, 38);
        lv_dropdown_set_options(test_source_dropdown, "DAC1 -> ADC\nDAC2 -> ADC\nExternal Input");
        lv_dropdown_set_selected(test_source_dropdown, 0);
        lv_obj_add_style(test_source_dropdown, &cal_field, 0);

        lv_obj_t * lv_label_6 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_6, "TEST ITEM");
        lv_obj_set_x(lv_label_6, 12);
        lv_obj_set_y(lv_label_6, 120);
        lv_obj_set_style_text_color(lv_label_6, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * test_item_dropdown = lv_dropdown_create(lv_obj_2);
        lv_obj_set_name(test_item_dropdown, "test_item_dropdown");
        lv_obj_set_x(test_item_dropdown, 12);
        lv_obj_set_y(test_item_dropdown, 140);
        lv_obj_set_width(test_item_dropdown, 166);
        lv_obj_set_height(test_item_dropdown, 38);
        lv_dropdown_set_options(test_item_dropdown, "DC / AC Self Test");
        lv_dropdown_set_selected(test_item_dropdown, 0);
        lv_obj_add_style(test_item_dropdown, &cal_field, 0);

        lv_obj_t * lv_label_7 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_7, "SAFETY PROFILE");
        lv_obj_set_x(lv_label_7, 12);
        lv_obj_set_y(lv_label_7, 198);
        lv_obj_set_style_text_color(lv_label_7, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_8 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_8, "DAC RANGE");
        lv_obj_set_x(lv_label_8, 12);
        lv_obj_set_y(lv_label_8, 228);
        lv_obj_set_style_text_color(lv_label_8, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_9 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_9, "0.00 .. 3.00 V");
        lv_obj_set_x(lv_label_9, 12);
        lv_obj_set_y(lv_label_9, 248);
        lv_obj_set_width(lv_label_9, 166);
        lv_obj_set_style_text_color(lv_label_9, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * lv_label_10 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_10, "RAMP");
        lv_obj_set_x(lv_label_10, 12);
        lv_obj_set_y(lv_label_10, 278);
        lv_obj_set_style_text_color(lv_label_10, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_11 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_11, "50 mV / 25 ms");
        lv_obj_set_x(lv_label_11, 12);
        lv_obj_set_y(lv_label_11, 298);
        lv_obj_set_width(lv_label_11, 166);
        lv_obj_set_style_text_color(lv_label_11, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * lv_label_12 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_12, "ADC CODE RANGE");
        lv_obj_set_x(lv_label_12, 12);
        lv_obj_set_y(lv_label_12, 328);
        lv_obj_set_style_text_color(lv_label_12, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_13 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_13, "0 .. 4095");
        lv_obj_set_x(lv_label_13, 12);
        lv_obj_set_y(lv_label_13, 348);
        lv_obj_set_width(lv_label_13, 166);
        lv_obj_set_style_text_color(lv_label_13, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_relay_label = lv_label_create(lv_obj_2);
        lv_obj_set_name(test_relay_label, "test_relay_label");
        lv_label_set_text(test_relay_label, "Relay: P2-18 / P2-16");
        lv_obj_set_x(test_relay_label, 12);
        lv_obj_set_y(test_relay_label, 386);
        lv_obj_set_width(test_relay_label, 166);
        lv_obj_set_style_text_color(test_relay_label, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_obj_3 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_3, 206);
        lv_obj_set_y(lv_obj_3, 58);
        lv_obj_set_width(lv_obj_3, 418);
        lv_obj_set_height(lv_obj_3, 414);
        lv_obj_set_flag(lv_obj_3, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_3, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_3, &cal_panel, 0);
        lv_obj_t * lv_label_14 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_14, "DC / AC OUTPUT SELF TEST");
        lv_obj_set_x(lv_label_14, 12);
        lv_obj_set_y(lv_label_14, 10);
        lv_obj_set_style_text_color(lv_label_14, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_15 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_15, "STEP");
        lv_obj_set_x(lv_label_15, 12);
        lv_obj_set_y(lv_label_15, 34);
        lv_obj_set_style_text_color(lv_label_15, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * test_step_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_step_label, "test_step_label");
        lv_label_set_text(test_step_label, "IDLE / WAITING FOR START");
        lv_obj_set_x(test_step_label, 60);
        lv_obj_set_y(test_step_label, 34);
        lv_obj_set_width(test_step_label, 240);
        lv_obj_set_style_text_color(test_step_label, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_label_16 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_16, "PTS");
        lv_obj_set_x(lv_label_16, 320);
        lv_obj_set_y(lv_label_16, 34);
        lv_obj_set_style_text_color(lv_label_16, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * test_points_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_points_label, "test_points_label");
        lv_label_set_text(test_points_label, "0 / 6");
        lv_obj_set_x(test_points_label, 354);
        lv_obj_set_y(test_points_label, 34);
        lv_obj_set_width(test_points_label, 52);
        lv_obj_set_style_text_color(test_points_label, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_progress_bar = lv_bar_create(lv_obj_3);
        lv_obj_set_name(test_progress_bar, "test_progress_bar");
        lv_obj_set_x(test_progress_bar, 12);
        lv_obj_set_y(test_progress_bar, 54);
        lv_obj_set_width(test_progress_bar, 394);
        lv_obj_set_height(test_progress_bar, 12);
        lv_bar_set_min_value(test_progress_bar, 0);
        lv_bar_set_max_value(test_progress_bar, 100);
        lv_bar_set_value(test_progress_bar, 0, false);

        lv_obj_t * lv_label_17 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_17, "TEST");
        lv_obj_set_x(lv_label_17, 12);
        lv_obj_set_y(lv_label_17, 78);
        lv_obj_set_width(lv_label_17, 64);
        lv_obj_set_style_text_color(lv_label_17, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_18 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_18, "OUT");
        lv_obj_set_x(lv_label_18, 82);
        lv_obj_set_y(lv_label_18, 78);
        lv_obj_set_width(lv_label_18, 42);
        lv_obj_set_style_text_color(lv_label_18, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_19 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_19, "AMP");
        lv_obj_set_x(lv_label_19, 132);
        lv_obj_set_y(lv_label_19, 78);
        lv_obj_set_width(lv_label_19, 70);
        lv_obj_set_style_text_color(lv_label_19, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_20 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_20, "NOISE");
        lv_obj_set_x(lv_label_20, 210);
        lv_obj_set_y(lv_label_20, 78);
        lv_obj_set_width(lv_label_20, 64);
        lv_obj_set_style_text_color(lv_label_20, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_21 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_21, "RESULT");
        lv_obj_set_x(lv_label_21, 282);
        lv_obj_set_y(lv_label_21, 78);
        lv_obj_set_width(lv_label_21, 112);
        lv_obj_set_style_text_color(lv_label_21, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_22 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_22, "DC 1/4");
        lv_obj_set_x(lv_label_22, 12);
        lv_obj_set_y(lv_label_22, 102);
        lv_obj_set_width(lv_label_22, 64);
        lv_obj_set_style_text_color(lv_label_22, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_out_dc_q = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_out_dc_q, "test_self_out_dc_q");
        lv_label_set_text(test_self_out_dc_q, "--");
        lv_obj_set_x(test_self_out_dc_q, 82);
        lv_obj_set_y(test_self_out_dc_q, 102);
        lv_obj_set_width(test_self_out_dc_q, 42);
        lv_obj_set_style_text_color(test_self_out_dc_q, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_amp_dc_q = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_amp_dc_q, "test_self_amp_dc_q");
        lv_label_set_text(test_self_amp_dc_q, "--");
        lv_obj_set_x(test_self_amp_dc_q, 132);
        lv_obj_set_y(test_self_amp_dc_q, 102);
        lv_obj_set_width(test_self_amp_dc_q, 70);
        lv_obj_set_style_text_color(test_self_amp_dc_q, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_noise_dc_q = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_noise_dc_q, "test_self_noise_dc_q");
        lv_label_set_text(test_self_noise_dc_q, "--");
        lv_obj_set_x(test_self_noise_dc_q, 210);
        lv_obj_set_y(test_self_noise_dc_q, 102);
        lv_obj_set_width(test_self_noise_dc_q, 64);
        lv_obj_set_style_text_color(test_self_noise_dc_q, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_result_dc_q = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_result_dc_q, "test_self_result_dc_q");
        lv_label_set_text(test_self_result_dc_q, "NOT RUN");
        lv_obj_set_x(test_self_result_dc_q, 282);
        lv_obj_set_y(test_self_result_dc_q, 102);
        lv_obj_set_width(test_self_result_dc_q, 112);
        lv_obj_set_style_text_color(test_self_result_dc_q, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_23 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_23, "DC 1/2");
        lv_obj_set_x(lv_label_23, 12);
        lv_obj_set_y(lv_label_23, 126);
        lv_obj_set_width(lv_label_23, 64);
        lv_obj_set_style_text_color(lv_label_23, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_out_dc_h = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_out_dc_h, "test_self_out_dc_h");
        lv_label_set_text(test_self_out_dc_h, "--");
        lv_obj_set_x(test_self_out_dc_h, 82);
        lv_obj_set_y(test_self_out_dc_h, 126);
        lv_obj_set_width(test_self_out_dc_h, 42);
        lv_obj_set_style_text_color(test_self_out_dc_h, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_amp_dc_h = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_amp_dc_h, "test_self_amp_dc_h");
        lv_label_set_text(test_self_amp_dc_h, "--");
        lv_obj_set_x(test_self_amp_dc_h, 132);
        lv_obj_set_y(test_self_amp_dc_h, 126);
        lv_obj_set_width(test_self_amp_dc_h, 70);
        lv_obj_set_style_text_color(test_self_amp_dc_h, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_noise_dc_h = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_noise_dc_h, "test_self_noise_dc_h");
        lv_label_set_text(test_self_noise_dc_h, "--");
        lv_obj_set_x(test_self_noise_dc_h, 210);
        lv_obj_set_y(test_self_noise_dc_h, 126);
        lv_obj_set_width(test_self_noise_dc_h, 64);
        lv_obj_set_style_text_color(test_self_noise_dc_h, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_result_dc_h = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_result_dc_h, "test_self_result_dc_h");
        lv_label_set_text(test_self_result_dc_h, "NOT RUN");
        lv_obj_set_x(test_self_result_dc_h, 282);
        lv_obj_set_y(test_self_result_dc_h, 126);
        lv_obj_set_width(test_self_result_dc_h, 112);
        lv_obj_set_style_text_color(test_self_result_dc_h, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_24 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_24, "DC FULL");
        lv_obj_set_x(lv_label_24, 12);
        lv_obj_set_y(lv_label_24, 150);
        lv_obj_set_width(lv_label_24, 64);
        lv_obj_set_style_text_color(lv_label_24, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_out_dc_f = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_out_dc_f, "test_self_out_dc_f");
        lv_label_set_text(test_self_out_dc_f, "--");
        lv_obj_set_x(test_self_out_dc_f, 82);
        lv_obj_set_y(test_self_out_dc_f, 150);
        lv_obj_set_width(test_self_out_dc_f, 42);
        lv_obj_set_style_text_color(test_self_out_dc_f, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_amp_dc_f = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_amp_dc_f, "test_self_amp_dc_f");
        lv_label_set_text(test_self_amp_dc_f, "--");
        lv_obj_set_x(test_self_amp_dc_f, 132);
        lv_obj_set_y(test_self_amp_dc_f, 150);
        lv_obj_set_width(test_self_amp_dc_f, 70);
        lv_obj_set_style_text_color(test_self_amp_dc_f, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_noise_dc_f = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_noise_dc_f, "test_self_noise_dc_f");
        lv_label_set_text(test_self_noise_dc_f, "--");
        lv_obj_set_x(test_self_noise_dc_f, 210);
        lv_obj_set_y(test_self_noise_dc_f, 150);
        lv_obj_set_width(test_self_noise_dc_f, 64);
        lv_obj_set_style_text_color(test_self_noise_dc_f, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_result_dc_f = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_result_dc_f, "test_self_result_dc_f");
        lv_label_set_text(test_self_result_dc_f, "NOT RUN");
        lv_obj_set_x(test_self_result_dc_f, 282);
        lv_obj_set_y(test_self_result_dc_f, 150);
        lv_obj_set_width(test_self_result_dc_f, 112);
        lv_obj_set_style_text_color(test_self_result_dc_f, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_25 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_25, "AC 1/4");
        lv_obj_set_x(lv_label_25, 12);
        lv_obj_set_y(lv_label_25, 178);
        lv_obj_set_width(lv_label_25, 64);
        lv_obj_set_style_text_color(lv_label_25, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_out_ac_q = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_out_ac_q, "test_self_out_ac_q");
        lv_label_set_text(test_self_out_ac_q, "--");
        lv_obj_set_x(test_self_out_ac_q, 82);
        lv_obj_set_y(test_self_out_ac_q, 178);
        lv_obj_set_width(test_self_out_ac_q, 42);
        lv_obj_set_style_text_color(test_self_out_ac_q, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_amp_ac_q = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_amp_ac_q, "test_self_amp_ac_q");
        lv_label_set_text(test_self_amp_ac_q, "--");
        lv_obj_set_x(test_self_amp_ac_q, 132);
        lv_obj_set_y(test_self_amp_ac_q, 178);
        lv_obj_set_width(test_self_amp_ac_q, 70);
        lv_obj_set_style_text_color(test_self_amp_ac_q, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_noise_ac_q = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_noise_ac_q, "test_self_noise_ac_q");
        lv_label_set_text(test_self_noise_ac_q, "--");
        lv_obj_set_x(test_self_noise_ac_q, 210);
        lv_obj_set_y(test_self_noise_ac_q, 178);
        lv_obj_set_width(test_self_noise_ac_q, 64);
        lv_obj_set_style_text_color(test_self_noise_ac_q, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_result_ac_q = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_result_ac_q, "test_self_result_ac_q");
        lv_label_set_text(test_self_result_ac_q, "NOT RUN");
        lv_obj_set_x(test_self_result_ac_q, 282);
        lv_obj_set_y(test_self_result_ac_q, 178);
        lv_obj_set_width(test_self_result_ac_q, 112);
        lv_obj_set_style_text_color(test_self_result_ac_q, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_26 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_26, "AC 1/2");
        lv_obj_set_x(lv_label_26, 12);
        lv_obj_set_y(lv_label_26, 202);
        lv_obj_set_width(lv_label_26, 64);
        lv_obj_set_style_text_color(lv_label_26, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_out_ac_h = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_out_ac_h, "test_self_out_ac_h");
        lv_label_set_text(test_self_out_ac_h, "--");
        lv_obj_set_x(test_self_out_ac_h, 82);
        lv_obj_set_y(test_self_out_ac_h, 202);
        lv_obj_set_width(test_self_out_ac_h, 42);
        lv_obj_set_style_text_color(test_self_out_ac_h, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_amp_ac_h = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_amp_ac_h, "test_self_amp_ac_h");
        lv_label_set_text(test_self_amp_ac_h, "--");
        lv_obj_set_x(test_self_amp_ac_h, 132);
        lv_obj_set_y(test_self_amp_ac_h, 202);
        lv_obj_set_width(test_self_amp_ac_h, 70);
        lv_obj_set_style_text_color(test_self_amp_ac_h, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_noise_ac_h = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_noise_ac_h, "test_self_noise_ac_h");
        lv_label_set_text(test_self_noise_ac_h, "--");
        lv_obj_set_x(test_self_noise_ac_h, 210);
        lv_obj_set_y(test_self_noise_ac_h, 202);
        lv_obj_set_width(test_self_noise_ac_h, 64);
        lv_obj_set_style_text_color(test_self_noise_ac_h, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_result_ac_h = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_result_ac_h, "test_self_result_ac_h");
        lv_label_set_text(test_self_result_ac_h, "NOT RUN");
        lv_obj_set_x(test_self_result_ac_h, 282);
        lv_obj_set_y(test_self_result_ac_h, 202);
        lv_obj_set_width(test_self_result_ac_h, 112);
        lv_obj_set_style_text_color(test_self_result_ac_h, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_27 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_27, "AC FULL");
        lv_obj_set_x(lv_label_27, 12);
        lv_obj_set_y(lv_label_27, 226);
        lv_obj_set_width(lv_label_27, 64);
        lv_obj_set_style_text_color(lv_label_27, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_out_ac_f = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_out_ac_f, "test_self_out_ac_f");
        lv_label_set_text(test_self_out_ac_f, "--");
        lv_obj_set_x(test_self_out_ac_f, 82);
        lv_obj_set_y(test_self_out_ac_f, 226);
        lv_obj_set_width(test_self_out_ac_f, 42);
        lv_obj_set_style_text_color(test_self_out_ac_f, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_amp_ac_f = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_amp_ac_f, "test_self_amp_ac_f");
        lv_label_set_text(test_self_amp_ac_f, "--");
        lv_obj_set_x(test_self_amp_ac_f, 132);
        lv_obj_set_y(test_self_amp_ac_f, 226);
        lv_obj_set_width(test_self_amp_ac_f, 70);
        lv_obj_set_style_text_color(test_self_amp_ac_f, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_noise_ac_f = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_noise_ac_f, "test_self_noise_ac_f");
        lv_label_set_text(test_self_noise_ac_f, "--");
        lv_obj_set_x(test_self_noise_ac_f, 210);
        lv_obj_set_y(test_self_noise_ac_f, 226);
        lv_obj_set_width(test_self_noise_ac_f, 64);
        lv_obj_set_style_text_color(test_self_noise_ac_f, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * test_self_result_ac_f = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_self_result_ac_f, "test_self_result_ac_f");
        lv_label_set_text(test_self_result_ac_f, "NOT RUN");
        lv_obj_set_x(test_self_result_ac_f, 282);
        lv_obj_set_y(test_self_result_ac_f, 226);
        lv_obj_set_width(test_self_result_ac_f, 112);
        lv_obj_set_style_text_color(test_self_result_ac_f, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_28 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_28, "ADC WAVEFORM / RAW CODE");
        lv_obj_set_x(lv_label_28, 12);
        lv_obj_set_y(lv_label_28, 254);
        lv_obj_set_style_text_color(lv_label_28, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_29 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_29, "4095");
        lv_obj_set_x(lv_label_29, 0);
        lv_obj_set_y(lv_label_29, 278);
        lv_obj_set_width(lv_label_29, 42);
        lv_obj_set_style_text_color(lv_label_29, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_30 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_30, "2048");
        lv_obj_set_x(lv_label_30, 0);
        lv_obj_set_y(lv_label_30, 322);
        lv_obj_set_width(lv_label_30, 42);
        lv_obj_set_style_text_color(lv_label_30, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_label_31 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_31, "0");
        lv_obj_set_x(lv_label_31, 24);
        lv_obj_set_y(lv_label_31, 366);
        lv_obj_set_width(lv_label_31, 18);
        lv_obj_set_style_text_color(lv_label_31, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * test_adc_chart = lv_chart_create(lv_obj_3);
        lv_obj_set_name(test_adc_chart, "test_adc_chart");
        lv_obj_set_x(test_adc_chart, 44);
        lv_obj_set_y(test_adc_chart, 276);
        lv_obj_set_width(test_adc_chart, 362);
        lv_obj_set_height(test_adc_chart, 104);
        lv_chart_set_type(test_adc_chart, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(test_adc_chart, 32);
        lv_chart_set_hor_div_line_count(test_adc_chart, 4);
        lv_chart_set_ver_div_line_count(test_adc_chart, 8);
        lv_obj_add_style(test_adc_chart, &cal_plot, 0);
        lv_obj_add_style(test_adc_chart, &cal_trace, LV_PART_ITEMS);
        lv_chart_set_axis_min_value(test_adc_chart, LV_CHART_AXIS_PRIMARY_Y, 0);
        lv_chart_set_axis_max_value(test_adc_chart, LV_CHART_AXIS_PRIMARY_Y, 4095);
        lv_chart_series_t * lv_chart_series_0 = lv_chart_add_series(test_adc_chart, lv_color_hex(0x80d6ff), LV_CHART_AXIS_PRIMARY_Y);
        static const int32_t test_adc_chart_values_0[] = {2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048};
        lv_chart_set_series_values(test_adc_chart, lv_chart_series_0, test_adc_chart_values_0, 32);

        lv_obj_t * lv_label_32 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_32, "0 ms");
        lv_obj_set_x(lv_label_32, 44);
        lv_obj_set_y(lv_label_32, 386);
        lv_obj_set_width(lv_label_32, 60);
        lv_obj_set_style_text_color(lv_label_32, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * test_adc_x_mid_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_adc_x_mid_label, "test_adc_x_mid_label");
        lv_label_set_text(test_adc_x_mid_label, "-- ms");
        lv_obj_set_x(test_adc_x_mid_label, 184);
        lv_obj_set_y(test_adc_x_mid_label, 386);
        lv_obj_set_width(test_adc_x_mid_label, 80);
        lv_obj_set_style_text_align(test_adc_x_mid_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(test_adc_x_mid_label, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * test_adc_x_end_label = lv_label_create(lv_obj_3);
        lv_obj_set_name(test_adc_x_end_label, "test_adc_x_end_label");
        lv_label_set_text(test_adc_x_end_label, "-- ms");
        lv_obj_set_x(test_adc_x_end_label, 326);
        lv_obj_set_y(test_adc_x_end_label, 386);
        lv_obj_set_width(test_adc_x_end_label, 80);
        lv_obj_set_style_text_align(test_adc_x_end_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(test_adc_x_end_label, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * lv_obj_4 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_4, 632);
        lv_obj_set_y(lv_obj_4, 58);
        lv_obj_set_width(lv_obj_4, 160);
        lv_obj_set_height(lv_obj_4, 414);
        lv_obj_set_flag(lv_obj_4, LV_OBJ_FLAG_SCROLLABLE, false);
        lv_obj_set_scrollbar_mode(lv_obj_4, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(lv_obj_4, &cal_panel, 0);
        lv_obj_t * lv_label_33 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_33, "TEST CONTROL");
        lv_obj_set_x(lv_label_33, 12);
        lv_obj_set_y(lv_label_33, 12);
        lv_obj_set_style_text_color(lv_label_33, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_button_1 = lv_button_create(lv_obj_4);
        lv_obj_set_x(lv_button_1, 8);
        lv_obj_set_y(lv_button_1, 38);
        lv_obj_set_width(lv_button_1, 144);
        lv_obj_set_height(lv_button_1, 36);
        lv_obj_add_style(lv_button_1, &cal_primary, 0);
        lv_obj_t * lv_label_34 = lv_label_create(lv_button_1);
        lv_label_set_text(lv_label_34, "HOME");
        lv_obj_set_align(lv_label_34, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_1, LV_EVENT_CLICKED, screen_control_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * test_start_button = lv_button_create(lv_obj_4);
        lv_obj_set_name(test_start_button, "test_start_button");
        lv_obj_set_x(test_start_button, 8);
        lv_obj_set_y(test_start_button, 82);
        lv_obj_set_width(test_start_button, 144);
        lv_obj_set_height(test_start_button, 38);
        lv_obj_add_style(test_start_button, &cal_safe, 0);
        lv_obj_t * lv_label_35 = lv_label_create(test_start_button);
        lv_label_set_text(lv_label_35, "START");
        lv_obj_set_align(lv_label_35, LV_ALIGN_CENTER);

        lv_obj_t * test_stop_button = lv_button_create(lv_obj_4);
        lv_obj_set_name(test_stop_button, "test_stop_button");
        lv_obj_set_x(test_stop_button, 8);
        lv_obj_set_y(test_stop_button, 126);
        lv_obj_set_width(test_stop_button, 144);
        lv_obj_set_height(test_stop_button, 38);
        lv_obj_add_style(test_stop_button, &cal_stop, 0);
        lv_obj_t * lv_label_36 = lv_label_create(test_stop_button);
        lv_label_set_text(lv_label_36, "STOP");
        lv_obj_set_align(lv_label_36, LV_ALIGN_CENTER);

        lv_obj_t * test_adc_toggle_button = lv_button_create(lv_obj_4);
        lv_obj_set_name(test_adc_toggle_button, "test_adc_toggle_button");
        lv_obj_set_x(test_adc_toggle_button, 8);
        lv_obj_set_y(test_adc_toggle_button, 180);
        lv_obj_set_width(test_adc_toggle_button, 144);
        lv_obj_set_height(test_adc_toggle_button, 42);
        lv_obj_add_style(test_adc_toggle_button, &cal_pending, 0);
        lv_obj_t * test_adc_toggle_label = lv_label_create(test_adc_toggle_button);
        lv_obj_set_name(test_adc_toggle_label, "test_adc_toggle_label");
        lv_label_set_text(test_adc_toggle_label, "ADC DISPLAY OFF");
        lv_obj_set_align(test_adc_toggle_label, LV_ALIGN_CENTER);

        lv_obj_t * lv_label_37 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_37, "SAMPLE RATE");
        lv_obj_set_x(lv_label_37, 12);
        lv_obj_set_y(lv_label_37, 240);
        lv_obj_set_style_text_color(lv_label_37, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * test_adc_rate_label = lv_label_create(lv_obj_4);
        lv_obj_set_name(test_adc_rate_label, "test_adc_rate_label");
        lv_label_set_text(test_adc_rate_label, "-- MS/s");
        lv_obj_set_x(test_adc_rate_label, 12);
        lv_obj_set_y(test_adc_rate_label, 260);
        lv_obj_set_width(test_adc_rate_label, 136);
        lv_obj_set_style_text_color(test_adc_rate_label, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * lv_label_38 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_38, "SAMPLE POINTS");
        lv_obj_set_x(lv_label_38, 12);
        lv_obj_set_y(lv_label_38, 288);
        lv_obj_set_style_text_color(lv_label_38, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * test_adc_points_label = lv_label_create(lv_obj_4);
        lv_obj_set_name(test_adc_points_label, "test_adc_points_label");
        lv_label_set_text(test_adc_points_label, "--");
        lv_obj_set_x(test_adc_points_label, 12);
        lv_obj_set_y(test_adc_points_label, 308);
        lv_obj_set_width(test_adc_points_label, 136);
        lv_obj_set_style_text_color(test_adc_points_label, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * lv_label_39 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_39, "TIME SPAN");
        lv_obj_set_x(lv_label_39, 12);
        lv_obj_set_y(lv_label_39, 336);
        lv_obj_set_style_text_color(lv_label_39, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * test_adc_span_label = lv_label_create(lv_obj_4);
        lv_obj_set_name(test_adc_span_label, "test_adc_span_label");
        lv_label_set_text(test_adc_span_label, "DISPLAY OFF");
        lv_obj_set_x(test_adc_span_label, 12);
        lv_obj_set_y(test_adc_span_label, 356);
        lv_obj_set_width(test_adc_span_label, 136);
        lv_obj_set_style_text_color(test_adc_span_label, lv_color_hex(0xd5dbe0), 0);

        lv_obj_t * lv_label_40 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_40, "ERROR");
        lv_obj_set_x(lv_label_40, 12);
        lv_obj_set_y(lv_label_40, 382);
        lv_obj_set_style_text_color(lv_label_40, lv_color_hex(0x9aa4ad), 0);

        lv_obj_t * test_error_label = lv_label_create(lv_obj_4);
        lv_obj_set_name(test_error_label, "test_error_label");
        lv_label_set_text(test_error_label, "--");
        lv_obj_set_x(test_error_label, 62);
        lv_obj_set_y(test_error_label, 382);
        lv_obj_set_width(test_error_label, 86);
        lv_obj_set_style_text_color(test_error_label, lv_color_hex(0xff9f43), 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

