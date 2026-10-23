/**
 * @file app_metric_card_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "app_metric_card_gen.h"
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

lv_obj_t * app_metric_card_create(lv_obj_t * parent, const char * title, const char * value, const char * unit, lv_color_t accent)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t style_metric_card;

    static bool style_inited = false;

    if (!style_inited) {
        #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
        if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
            lv_style_init(&style_metric_card);
            lv_style_set_width(&style_metric_card, 240);
            lv_style_set_height(&style_metric_card, 88);
            lv_style_set_bg_color(&style_metric_card, COLOR_SURFACE);
            lv_style_set_bg_opa(&style_metric_card, (255 * 100 / 100));
            lv_style_set_border_color(&style_metric_card, accent);
            lv_style_set_border_width(&style_metric_card, 2);
            lv_style_set_radius(&style_metric_card, 10);
            lv_style_set_pad_all(&style_metric_card, 0);

        }
        #endif
        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
    if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "app_metric_card_#");

        lv_obj_remove_style_all(lv_obj_0);
        lv_obj_add_style(lv_obj_0, &style_metric_card, 0);
        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_0, title);
        lv_obj_set_x(lv_label_0, 14);
        lv_obj_set_y(lv_label_0, 12);
        lv_obj_set_style_text_color(lv_label_0, COLOR_TEXT_MUTED, 0);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_1, value);
        lv_obj_set_x(lv_label_1, 14);
        lv_obj_set_y(lv_label_1, 42);
        lv_obj_set_style_text_color(lv_label_1, COLOR_TEXT, 0);

        lv_obj_t * lv_label_2 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_2, unit);
        lv_obj_set_align(lv_label_2, LV_ALIGN_BOTTOM_RIGHT);
        lv_obj_set_x(lv_label_2, -14);
        lv_obj_set_y(lv_label_2, -14);
        lv_obj_set_style_text_color(lv_label_2, COLOR_TEXT_MUTED, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

