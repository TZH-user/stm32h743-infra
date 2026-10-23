/**
 * @file signal_separation_ui.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "signal_separation_ui.h"


/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void signal_separation_ui_init(const char * asset_path)
{
    LV_LOG("Initializing custom C code using LVGL v%d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    signal_separation_ui_init_gen(asset_path);

#if defined(LV_EDITOR_PREVIEW) && LV_USE_THEME_DEFAULT
    /* 预览运行时的 LV_FONT_DEFAULT 是 Montserrat,无中文字形;实机则由
     * lv_conf.h 将默认字体指向 font_cn_16。这里仅在预览侧用同一份字库
     * 重建默认主题(其余参数与 lv_display_create 的默认值一致),
     * 使预览与实机渲染一致。 */
    LV_FONT_DECLARE(font_cn_16);
    lv_display_t * disp = lv_display_get_default();
    if(disp != NULL) {
        lv_theme_t * theme = lv_theme_default_init(disp,
                                                   lv_palette_main(LV_PALETTE_BLUE),
                                                   lv_palette_main(LV_PALETTE_RED),
                                                   LV_THEME_DEFAULT_DARK,
                                                   &font_cn_16);
        lv_display_set_theme(disp, theme);
    }
#endif

    /* Add your own custom code here if needed */
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
