/**
 * @file screen_control_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_control_gen.h"
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

lv_obj_t * screen_control_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
    if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_control_#");

        lv_obj_add_style(lv_obj_0, &style_screen, 0);
        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_1, 0);
        lv_obj_set_y(lv_obj_1, 0);
        lv_obj_add_style(lv_obj_1, &style_header, 0);
        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_1);
        lv_label_set_text(lv_label_0, "Control / Acq & Analysis");
        lv_obj_set_x(lv_label_0, 16);
        lv_obj_set_y(lv_label_0, 16);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_1);
        lv_label_set_text(lv_label_1, "Changes take effect after Apply");
        lv_obj_set_x(lv_label_1, 500);
        lv_obj_set_y(lv_label_1, 16);
        lv_obj_set_style_text_color(lv_label_1, COLOR_TEXT_MUTED, 0);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 16);
        lv_obj_set_y(lv_obj_2, 64);
        lv_obj_set_width(lv_obj_2, 370);
        lv_obj_set_height(lv_obj_2, 142);
        lv_obj_add_style(lv_obj_2, &style_card, 0);
        lv_obj_t * lv_label_2 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_2, "Sample rate");
        lv_obj_set_x(lv_label_2, 4);
        lv_obj_set_y(lv_label_2, 2);

        lv_obj_t * lv_label_3 = lv_label_create(lv_obj_2);
        lv_label_bind_text(lv_label_3, &sample_rate, NULL);
        lv_obj_set_x(lv_label_3, 4);
        lv_obj_set_y(lv_label_3, 42);

        lv_obj_t * lv_button_0 = lv_button_create(lv_obj_2);
        lv_obj_set_x(lv_button_0, 220);
        lv_obj_set_y(lv_button_0, 36);
        lv_obj_set_width(lv_button_0, 120);
        lv_obj_set_height(lv_button_0, 42);
        lv_obj_add_style(lv_button_0, &style_secondary_button, 0);
        lv_obj_t * lv_label_4 = lv_label_create(lv_button_0);
        lv_label_set_text(lv_label_4, "Select");
        lv_obj_set_align(lv_label_4, LV_ALIGN_CENTER);

        lv_obj_t * lv_obj_3 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_3, 414);
        lv_obj_set_y(lv_obj_3, 64);
        lv_obj_set_width(lv_obj_3, 370);
        lv_obj_set_height(lv_obj_3, 142);
        lv_obj_add_style(lv_obj_3, &style_card, 0);
        lv_obj_t * lv_label_5 = lv_label_create(lv_obj_3);
        lv_label_set_text(lv_label_5, "FFT size");
        lv_obj_set_x(lv_label_5, 4);
        lv_obj_set_y(lv_label_5, 2);

        lv_obj_t * lv_label_6 = lv_label_create(lv_obj_3);
        lv_label_bind_text(lv_label_6, &fft_size, NULL);
        lv_obj_set_x(lv_label_6, 4);
        lv_obj_set_y(lv_label_6, 42);

        lv_obj_t * lv_button_1 = lv_button_create(lv_obj_3);
        lv_obj_set_x(lv_button_1, 220);
        lv_obj_set_y(lv_button_1, 36);
        lv_obj_set_width(lv_button_1, 120);
        lv_obj_set_height(lv_button_1, 42);
        lv_obj_add_style(lv_button_1, &style_secondary_button, 0);
        lv_obj_t * lv_label_7 = lv_label_create(lv_button_1);
        lv_label_set_text(lv_label_7, "Select");
        lv_obj_set_align(lv_label_7, LV_ALIGN_CENTER);

        lv_obj_t * lv_obj_4 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_4, 16);
        lv_obj_set_y(lv_obj_4, 214);
        lv_obj_set_width(lv_obj_4, 370);
        lv_obj_set_height(lv_obj_4, 142);
        lv_obj_add_style(lv_obj_4, &style_card, 0);
        lv_obj_t * lv_label_8 = lv_label_create(lv_obj_4);
        lv_label_set_text(lv_label_8, "Band");
        lv_obj_set_x(lv_label_8, 4);
        lv_obj_set_y(lv_label_8, 2);

        lv_obj_t * lv_label_9 = lv_label_create(lv_obj_4);
        lv_label_bind_text(lv_label_9, &frequency_band, NULL);
        lv_obj_set_x(lv_label_9, 4);
        lv_obj_set_y(lv_label_9, 42);

        lv_obj_t * lv_button_2 = lv_button_create(lv_obj_4);
        lv_obj_set_x(lv_button_2, 220);
        lv_obj_set_y(lv_button_2, 36);
        lv_obj_set_width(lv_button_2, 120);
        lv_obj_set_height(lv_button_2, 42);
        lv_obj_add_style(lv_button_2, &style_secondary_button, 0);
        lv_obj_t * lv_label_10 = lv_label_create(lv_button_2);
        lv_label_set_text(lv_label_10, "Edit");
        lv_obj_set_align(lv_label_10, LV_ALIGN_CENTER);

        lv_obj_t * lv_obj_5 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_5, 414);
        lv_obj_set_y(lv_obj_5, 214);
        lv_obj_set_width(lv_obj_5, 370);
        lv_obj_set_height(lv_obj_5, 142);
        lv_obj_add_style(lv_obj_5, &style_card, 0);
        lv_obj_t * lv_label_11 = lv_label_create(lv_obj_5);
        lv_label_set_text(lv_label_11, "Capture & Apply");
        lv_obj_set_x(lv_label_11, 4);
        lv_obj_set_y(lv_label_11, 2);

        lv_obj_t * lv_label_12 = lv_label_create(lv_obj_5);
        lv_label_set_text(lv_label_12, "Timeout: 1000 ms");
        lv_obj_set_x(lv_label_12, 4);
        lv_obj_set_y(lv_label_12, 42);
        lv_obj_set_style_text_color(lv_label_12, COLOR_TEXT_MUTED, 0);

        lv_obj_t * lv_button_3 = lv_button_create(lv_obj_5);
        lv_obj_set_x(lv_button_3, 180);
        lv_obj_set_y(lv_button_3, 34);
        lv_obj_set_width(lv_button_3, 160);
        lv_obj_set_height(lv_button_3, 48);
        lv_obj_add_style(lv_button_3, &style_primary_button, 0);
        lv_obj_t * lv_label_13 = lv_label_create(lv_button_3);
        lv_label_set_text(lv_label_13, "Apply");
        lv_obj_set_align(lv_label_13, LV_ALIGN_CENTER);

        lv_obj_t * lv_obj_6 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_6, 0);
        lv_obj_set_y(lv_obj_6, 424);
        lv_obj_add_style(lv_obj_6, &style_nav, 0);
        lv_obj_t * lv_button_4 = lv_button_create(lv_obj_6);
        lv_obj_set_x(lv_button_4, 16);
        lv_obj_set_y(lv_button_4, 8);
        lv_obj_set_width(lv_button_4, 180);
        lv_obj_set_height(lv_button_4, 40);
        lv_obj_add_style(lv_button_4, &style_nav_button, 0);
        lv_obj_t * lv_label_14 = lv_label_create(lv_button_4);
        lv_label_set_text(lv_label_14, "Monitor");
        lv_obj_set_align(lv_label_14, LV_ALIGN_CENTER);

        lv_obj_add_screen_load_event(lv_button_4, LV_EVENT_CLICKED, screen_monitor, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * lv_button_5 = lv_button_create(lv_obj_6);
        lv_obj_set_x(lv_button_5, 212);
        lv_obj_set_y(lv_button_5, 8);
        lv_obj_set_width(lv_button_5, 180);
        lv_obj_set_height(lv_button_5, 40);
        lv_obj_add_style(lv_button_5, &style_nav_button, 0);
        lv_obj_t * lv_label_15 = lv_label_create(lv_button_5);
        lv_label_set_text(lv_label_15, "Test");
        lv_obj_set_align(lv_label_15, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_5, LV_EVENT_CLICKED, screen_test_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * lv_button_6 = lv_button_create(lv_obj_6);
        lv_obj_set_x(lv_button_6, 408);
        lv_obj_set_y(lv_button_6, 8);
        lv_obj_set_width(lv_button_6, 180);
        lv_obj_set_height(lv_button_6, 40);
        lv_obj_add_style(lv_button_6, &style_nav_button, 0);
        lv_obj_t * lv_label_16 = lv_label_create(lv_button_6);
        lv_label_set_text(lv_label_16, "Calib");
        lv_obj_set_align(lv_label_16, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_6, LV_EVENT_CLICKED, screen_calibration_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * lv_button_7 = lv_button_create(lv_obj_6);
        lv_obj_set_x(lv_button_7, 604);
        lv_obj_set_y(lv_button_7, 8);
        lv_obj_set_width(lv_button_7, 180);
        lv_obj_set_height(lv_button_7, 40);
        lv_obj_add_style(lv_button_7, &style_nav_button, 0);
        lv_obj_t * lv_label_17 = lv_label_create(lv_button_7);
        lv_label_set_text(lv_label_17, "System");
        lv_obj_set_align(lv_label_17, LV_ALIGN_CENTER);

        lv_obj_add_screen_create_event(lv_button_7, LV_EVENT_CLICKED, screen_system_create, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

