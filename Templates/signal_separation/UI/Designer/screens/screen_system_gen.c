/**
 * @file screen_system_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_system_gen.h"
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

lv_obj_t * screen_system_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
    if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_system_#");

        lv_obj_add_style(lv_obj_0, &style_screen, 0);
        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_1, 0);
        lv_obj_set_y(lv_obj_1, 0);
        lv_obj_add_style(lv_obj_1, &style_header, 0);
        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_1);
        lv_label_set_text(lv_label_0, "System / Diagnostics");
        lv_obj_set_x(lv_label_0, 16);
        lv_obj_set_y(lv_label_0, 16);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 16);
        lv_obj_set_y(lv_obj_2, 64);
        lv_obj_set_width(lv_obj_2, 500);
        lv_obj_set_height(lv_obj_2, 344);
        lv_obj_add_style(lv_obj_2, &style_card, 0);
        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_1, "Runtime Diagnostics");
        lv_obj_set_x(lv_label_1, 4);
        lv_obj_set_y(lv_label_1, 2);

        lv_obj_t * lv_label_2 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_2, "Analysis state");
        lv_obj_set_x(lv_label_2, 12);
        lv_obj_set_y(lv_label_2, 48);
        lv_obj_set_style_text_color(lv_label_2, COLOR_TEXT_MUTED, 0);

        lv_obj_t * lv_label_3 = lv_label_create(lv_obj_2);
        lv_label_bind_text(lv_label_3, &analysis_state, NULL);
        lv_obj_set_x(lv_label_3, 300);
        lv_obj_set_y(lv_label_3, 48);

        lv_obj_t * lv_label_4 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_4, "Actual sample rate");
        lv_obj_set_x(lv_label_4, 12);
        lv_obj_set_y(lv_label_4, 88);
        lv_obj_set_style_text_color(lv_label_4, COLOR_TEXT_MUTED, 0);

        lv_obj_t * lv_label_5 = lv_label_create(lv_obj_2);
        lv_label_bind_text(lv_label_5, &sample_rate, NULL);
        lv_obj_set_x(lv_label_5, 300);
        lv_obj_set_y(lv_label_5, 88);

        lv_obj_t * lv_label_6 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_6, "FFT size");
        lv_obj_set_x(lv_label_6, 12);
        lv_obj_set_y(lv_label_6, 128);
        lv_obj_set_style_text_color(lv_label_6, COLOR_TEXT_MUTED, 0);

        lv_obj_t * lv_label_7 = lv_label_create(lv_obj_2);
        lv_label_bind_text(lv_label_7, &fft_size, NULL);
        lv_obj_set_x(lv_label_7, 300);
        lv_obj_set_y(lv_label_7, 128);

        lv_obj_t * lv_label_8 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_8, "Successful runs");
        lv_obj_set_x(lv_label_8, 12);
        lv_obj_set_y(lv_label_8, 168);
        lv_obj_set_style_text_color(lv_label_8, COLOR_TEXT_MUTED, 0);

        lv_obj_t * lv_label_9 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_9, "12");
        lv_obj_set_x(lv_label_9, 300);
        lv_obj_set_y(lv_label_9, 168);

        lv_obj_t * lv_label_10 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_10, "Failed runs");
        lv_obj_set_x(lv_label_10, 12);
        lv_obj_set_y(lv_label_10, 208);
        lv_obj_set_style_text_color(lv_label_10, COLOR_TEXT_MUTED, 0);

        lv_obj_t * lv_label_11 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_11, "0");
        lv_obj_set_x(lv_label_11, 300);
        lv_obj_set_y(lv_label_11, 208);

        lv_obj_t * lv_label_12 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_12, "Touch input");
        lv_obj_set_x(lv_label_12, 12);
        lv_obj_set_y(lv_label_12, 248);
        lv_obj_set_style_text_color(lv_label_12, COLOR_TEXT_MUTED, 0);

        lv_obj_t * lv_label_13 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_13, "ONLINE");
        lv_obj_set_x(lv_label_13, 300);
        lv_obj_set_y(lv_label_13, 248);
        lv_obj_set_style_text_color(lv_label_13, COLOR_SUCCESS, 0);

        lv_obj_t * lv_obj_3 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_3, 528);
        lv_obj_set_y(lv_obj_3, 64);
        lv_obj_set_width(lv_obj_3, 256);
        lv_obj_set_height(lv_obj_3, 344);
        lv_obj_add_style(lv_obj_3, &style_card, 0);
        lv_obj_t * lv_label_14 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_14, "Quick Tools");
        lv_obj_set_x(lv_label_14, 4);
        lv_obj_set_y(lv_label_14, 2);

        lv_obj_t * lv_button_0 = lv_button_create(lv_obj_3);
        lv_obj_set_x(lv_button_0, 4);
        lv_obj_set_y(lv_button_0, 48);
        lv_obj_set_width(lv_button_0, 228);
        lv_obj_set_height(lv_button_0, 48);
        lv_obj_add_style(lv_button_0, &style_secondary_button, 0);
        lv_obj_t * lv_label_15 = lv_label_create(lv_button_0);
        lv_label_set_text(lv_label_15, "Touch Test");
        lv_obj_set_align(lv_label_15, LV_ALIGN_CENTER);

        lv_obj_t * lv_button_1 = lv_button_create(lv_obj_3);
        lv_obj_set_x(lv_button_1, 4);
        lv_obj_set_y(lv_button_1, 108);
        lv_obj_set_width(lv_button_1, 228);
        lv_obj_set_height(lv_button_1, 48);
        lv_obj_add_style(lv_button_1, &style_secondary_button, 0);
        lv_obj_t * lv_label_16 = lv_label_create(lv_button_1);
        lv_label_set_text(lv_label_16, "Display Test");
        lv_obj_set_align(lv_label_16, LV_ALIGN_CENTER);

        lv_obj_t * lv_button_2 = lv_button_create(lv_obj_3);
        lv_obj_set_x(lv_button_2, 4);
        lv_obj_set_y(lv_button_2, 168);
        lv_obj_set_width(lv_button_2, 228);
        lv_obj_set_height(lv_button_2, 48);
        lv_obj_add_style(lv_button_2, &style_secondary_button, 0);
        lv_obj_t * lv_label_17 = lv_label_create(lv_button_2);
        lv_label_set_text(lv_label_17, "Memory Status");
        lv_obj_set_align(lv_label_17, LV_ALIGN_CENTER);

        lv_obj_t * lv_label_18 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_18, "UI Template v0.1");
        lv_obj_set_x(lv_label_18, 4);
        lv_obj_set_y(lv_label_18, 276);
        lv_obj_set_style_text_color(lv_label_18, COLOR_TEXT_MUTED, 0);

        lv_obj_t * lv_obj_4 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_4, 0);
        lv_obj_set_y(lv_obj_4, 424);
        lv_obj_add_style(lv_obj_4, &style_nav, 0);
        lv_obj_t * lv_button_3 = lv_button_create(lv_obj_4);
        lv_obj_set_x(lv_button_3, 16);
        lv_obj_set_y(lv_button_3, 8);
        lv_obj_set_width(lv_button_3, 180);
        lv_obj_set_height(lv_button_3, 40);
        lv_obj_add_style(lv_button_3, &style_nav_button, 0);
        lv_obj_t * lv_label_19 = lv_label_create(lv_button_3);
        lv_label_set_text(lv_label_19, "Monitor");
        lv_obj_set_align(lv_label_19, LV_ALIGN_CENTER);

        lv_obj_add_screen_load_event(lv_button_3, LV_EVENT_CLICKED, screen_monitor, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * lv_button_4 = lv_button_create(lv_obj_4);
        lv_obj_set_x(lv_button_4, 212);
        lv_obj_set_y(lv_button_4, 8);
        lv_obj_set_width(lv_button_4, 180);
        lv_obj_set_height(lv_button_4, 40);
        lv_obj_add_style(lv_button_4, &style_nav_button, 0);
        lv_obj_t * lv_label_20 = lv_label_create(lv_button_4);
        lv_label_set_text(lv_label_20, "Test");
        lv_obj_set_align(lv_label_20, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_4, LV_EVENT_CLICKED, screen_test_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * lv_button_5 = lv_button_create(lv_obj_4);
        lv_obj_set_x(lv_button_5, 408);
        lv_obj_set_y(lv_button_5, 8);
        lv_obj_set_width(lv_button_5, 180);
        lv_obj_set_height(lv_button_5, 40);
        lv_obj_add_style(lv_button_5, &style_nav_button, 0);
        lv_obj_t * lv_label_21 = lv_label_create(lv_button_5);
        lv_label_set_text(lv_label_21, "Calibration");
        lv_obj_set_align(lv_label_21, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_5, LV_EVENT_CLICKED, screen_calibration_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * lv_button_6 = lv_button_create(lv_obj_4);
        lv_obj_set_x(lv_button_6, 604);
        lv_obj_set_y(lv_button_6, 8);
        lv_obj_set_width(lv_button_6, 180);
        lv_obj_set_height(lv_button_6, 40);
        lv_obj_add_style(lv_button_6, &style_nav_button, 0);
        lv_obj_add_style(lv_button_6, &style_nav_button_active, 0);
        lv_obj_t * lv_label_22 = lv_label_create(lv_button_6);
        lv_label_set_text(lv_label_22, "System");
        lv_obj_set_align(lv_label_22, LV_ALIGN_CENTER);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

