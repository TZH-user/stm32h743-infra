/**
 * @file screen_spectrum_gen.c
 * @brief Template source file for LVGL objects
 */

/*********************
 *      INCLUDES
 *********************/

#include "screen_spectrum_gen.h"
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

lv_obj_t * screen_spectrum_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static lv_style_t scope_full_panel;
    static lv_style_t scope_full_plot;
    static lv_style_t scope_full_key;
    static lv_style_t scope_full_trace_line;
    static lv_style_t scope_full_trace_point;

    static bool style_inited = false;

    if (!style_inited) {
        #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
        if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
            lv_style_init(&scope_full_panel);
            lv_style_set_bg_color(&scope_full_panel, lv_color_hex(0x16191d));
            lv_style_set_bg_opa(&scope_full_panel, (255 * 100 / 100));
            lv_style_set_border_color(&scope_full_panel, lv_color_hex(0x3b4148));
            lv_style_set_border_width(&scope_full_panel, 1);
            lv_style_set_radius(&scope_full_panel, 2);
            lv_style_set_pad_all(&scope_full_panel, 0);

            lv_style_init(&scope_full_plot);
            lv_style_set_bg_color(&scope_full_plot, lv_color_hex(0x030405));
            lv_style_set_bg_opa(&scope_full_plot, (255 * 100 / 100));
            lv_style_set_border_color(&scope_full_plot, lv_color_hex(0x59616a));
            lv_style_set_border_width(&scope_full_plot, 1);
            lv_style_set_radius(&scope_full_plot, 0);
            lv_style_set_pad_all(&scope_full_plot, 4);
            lv_style_set_line_color(&scope_full_plot, lv_color_hex(0x354047));

            lv_style_init(&scope_full_key);
            lv_style_set_bg_color(&scope_full_key, lv_color_hex(0x272c32));
            lv_style_set_bg_opa(&scope_full_key, (255 * 100 / 100));
            lv_style_set_border_color(&scope_full_key, lv_color_hex(0x626b75));
            lv_style_set_border_width(&scope_full_key, 1);
            lv_style_set_radius(&scope_full_key, 3);
            lv_style_set_text_color(&scope_full_key, lv_color_hex(0xf2f3f4));

            lv_style_init(&scope_full_trace_line);
            lv_style_set_line_width(&scope_full_trace_line, 2);

            lv_style_init(&scope_full_trace_point);
            lv_style_set_width(&scope_full_trace_point, 2);
            lv_style_set_height(&scope_full_trace_point, 2);
            lv_style_set_radius(&scope_full_trace_point, 1);
            lv_style_set_border_width(&scope_full_trace_point, 0);
            lv_style_set_bg_opa(&scope_full_trace_point, (255 * 100 / 100));

        }
        #endif
        style_inited = true;
    }


    lv_obj_t * the_root = NULL;

    #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
    if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "screen_spectrum_#");

        lv_obj_add_style(lv_obj_0, &style_screen, 0);
        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_1, 0);
        lv_obj_set_y(lv_obj_1, 0);
        lv_obj_set_width(lv_obj_1, 800);
        lv_obj_set_height(lv_obj_1, 50);
        lv_obj_add_style(lv_obj_1, &scope_full_panel, 0);
        lv_obj_t * lv_button_0 = lv_button_create(lv_obj_1);
        lv_obj_set_x(lv_button_0, 8);
        lv_obj_set_y(lv_button_0, 7);
        lv_obj_set_width(lv_button_0, 88);
        lv_obj_set_height(lv_button_0, 36);
        lv_obj_add_style(lv_button_0, &scope_full_key, 0);
        lv_obj_t * lv_label_0 = lv_label_create(lv_button_0);
        lv_label_set_text(lv_label_0, "返回");
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);

        lv_obj_add_screen_load_event(lv_button_0, LV_EVENT_CLICKED, screen_monitor, LV_SCREEN_LOAD_ANIM_NONE, 0, 0);

        lv_obj_t * ui_title = lv_label_create(lv_obj_1);
        lv_obj_set_name(ui_title, "ui_title");
        lv_label_set_text(ui_title, "完整波形");
        lv_obj_set_x(ui_title, 112);
        lv_obj_set_y(ui_title, 16);
        lv_obj_set_style_text_color(ui_title, lv_color_hex(0xffdf3f), 0);

        lv_obj_t * ui_mode_dropdown = lv_dropdown_create(lv_obj_1);
        lv_obj_set_name(ui_mode_dropdown, "ui_mode_dropdown");
        lv_obj_set_x(ui_mode_dropdown, 282);
        lv_obj_set_y(ui_mode_dropdown, 7);
        lv_obj_set_width(ui_mode_dropdown, 210);
        lv_obj_set_height(ui_mode_dropdown, 36);
        lv_dropdown_set_options(ui_mode_dropdown, "FFT 频谱\n实时波形");
        lv_dropdown_set_selected(ui_mode_dropdown, 1);
        lv_obj_add_style(ui_mode_dropdown, &scope_full_key, 0);

        lv_obj_t * ui_spectrum_info = lv_label_create(lv_obj_1);
        lv_obj_set_name(ui_spectrum_info, "ui_spectrum_info");
        lv_label_set_text(ui_spectrum_info, "128 点连续 ADC 采样");
        lv_obj_set_x(ui_spectrum_info, 520);
        lv_obj_set_y(ui_spectrum_info, 16);
        lv_obj_set_style_text_color(ui_spectrum_info, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * ui_chart = lv_chart_create(lv_obj_0);
        lv_obj_set_name(ui_chart, "ui_chart");
        lv_obj_set_x(ui_chart, 8);
        lv_obj_set_y(ui_chart, 60);
        lv_obj_set_width(ui_chart, 784);
        lv_obj_set_height(ui_chart, 348);
        lv_chart_set_type(ui_chart, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(ui_chart, 32);
        lv_chart_set_hor_div_line_count(ui_chart, 8);
        lv_chart_set_ver_div_line_count(ui_chart, 10);
        lv_obj_add_style(ui_chart, &scope_full_plot, 0);
        lv_obj_add_style(ui_chart, &scope_full_trace_line, LV_PART_ITEMS);
        lv_obj_add_style(ui_chart, &scope_full_trace_point, LV_PART_INDICATOR);
        lv_chart_set_axis_min_value(ui_chart, LV_CHART_AXIS_PRIMARY_Y, 0);
        lv_chart_set_axis_max_value(ui_chart, LV_CHART_AXIS_PRIMARY_Y, 100);
        lv_chart_series_t * lv_chart_series_0 = lv_chart_add_series(ui_chart, lv_color_hex(0xffdf3f), LV_CHART_AXIS_PRIMARY_Y);
        static const int32_t ui_chart_values_0[] = {50, 65, 76, 80, 73, 59, 42, 27, 20, 25, 39, 56, 71, 79, 78, 66, 50, 34, 22, 21, 31, 46, 63, 75, 80, 74, 61, 44, 29, 21, 24, 37};
        lv_chart_set_series_values(ui_chart, lv_chart_series_0, ui_chart_values_0, 32);

        lv_obj_t * lv_obj_2 = lv_obj_create(lv_obj_0);
        lv_obj_set_x(lv_obj_2, 8);
        lv_obj_set_y(lv_obj_2, 416);
        lv_obj_set_width(lv_obj_2, 784);
        lv_obj_set_height(lv_obj_2, 56);
        lv_obj_add_style(lv_obj_2, &scope_full_panel, 0);
        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_1, "CH1");
        lv_obj_set_x(lv_label_1, 12);
        lv_obj_set_y(lv_label_1, 18);
        lv_obj_set_style_text_color(lv_label_1, lv_color_hex(0xffdf3f), 0);

        lv_obj_t * lv_label_2 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_2, "1.00 V/div");
        lv_obj_set_x(lv_label_2, 52);
        lv_obj_set_y(lv_label_2, 18);

        lv_obj_t * lv_label_3 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_3, "H");
        lv_obj_set_x(lv_label_3, 206);
        lv_obj_set_y(lv_label_3, 18);
        lv_obj_set_style_text_color(lv_label_3, lv_color_hex(0x80d6ff), 0);

        lv_obj_t * lv_label_4 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_4, "20.0 us/div");
        lv_obj_set_x(lv_label_4, 230);
        lv_obj_set_y(lv_label_4, 18);

        lv_obj_t * lv_label_5 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_5, "触发");
        lv_obj_set_x(lv_label_5, 396);
        lv_obj_set_y(lv_label_5, 18);
        lv_obj_set_style_text_color(lv_label_5, lv_color_hex(0x72eaa5), 0);

        lv_obj_t * lv_label_6 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_6, "CH1 上升沿");
        lv_obj_set_x(lv_label_6, 442);
        lv_obj_set_y(lv_label_6, 18);

        lv_obj_t * lv_label_7 = lv_label_create(lv_obj_2);
        lv_label_set_text(lv_label_7, "实时快照");
        lv_obj_set_x(lv_label_7, 650);
        lv_obj_set_y(lv_label_7, 18);
        lv_obj_set_style_text_color(lv_label_7, lv_color_hex(0x72eaa5), 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

