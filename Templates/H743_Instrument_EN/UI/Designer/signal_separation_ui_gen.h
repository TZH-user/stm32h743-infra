/**
 * @file signal_separation_ui_gen.h
 */

#ifndef SIGNAL_SEPARATION_UI_GEN_H
#define SIGNAL_SEPARATION_UI_GEN_H

#ifndef UI_SUBJECT_STRING_LENGTH
#define UI_SUBJECT_STRING_LENGTH 256
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
    #include "lvgl_private.h"
#else
    #include "lvgl/lvgl.h"
    #include "lvgl/lvgl_private.h"
#endif

#ifdef LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif



/* Prototypes for target functions, needed by responsive const definitions */

void signal_separation_ui_set_target(uint32_t target);
uint32_t signal_separation_ui_get_target(void);
bool signal_separation_ui_check_target(uint32_t target);

/*********************
 *      DEFINES
 *********************/

#define SIGNAL_SEPARATION_UI_TARGET_UNDEFINED         (0 << 1)
#define SIGNAL_SEPARATION_UI_TARGET_H743_RGB_800X480  (1 << 1)
#define SIGNAL_SEPARATION_UI_TARGET_ALL               0x0FFFFFFF

/* By default compile for all targets, allowing to switch to any targets at runtime */
#ifndef SIGNAL_SEPARATION_UI_COMPILE_TARGET
#define SIGNAL_SEPARATION_UI_COMPILE_TARGET SIGNAL_SEPARATION_UI_TARGET_ALL
#endif

#define SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(target) (SIGNAL_SEPARATION_UI_COMPILE_TARGET & (target) ? 1 : 0)

#define COLOR_BG lv_color_hex(0x050607)
#define COLOR_SURFACE lv_color_hex(0x15171a)
#define COLOR_SURFACE_ALT lv_color_hex(0x22252a)
#define COLOR_BORDER lv_color_hex(0x474b50)
#define COLOR_TEXT lv_color_hex(0xf4f7fb)
#define COLOR_TEXT_MUTED lv_color_hex(0x93a4b8)
#define COLOR_PRIMARY lv_color_hex(0xffd43b)
#define COLOR_SUCCESS lv_color_hex(0x57e38c)
#define COLOR_WARNING lv_color_hex(0xff9f43)
#define COLOR_DANGER lv_color_hex(0xe15454)


#ifndef LV_XML_EVAL_STRING_BUF_SIZE
    #define LV_XML_EVAL_STRING_BUF_SIZE 256
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL VARIABLES
 **********************/

/*-------------------
 * Permanent screens
 *------------------*/

extern lv_obj_t * screen_monitor;

/*----------------
 * Global styles
 *----------------*/

extern lv_style_t style_screen;
extern lv_style_t style_header;
extern lv_style_t style_card;
extern lv_style_t style_nav;
extern lv_style_t style_nav_button;
extern lv_style_t style_nav_button_active;
extern lv_style_t style_primary_button;
extern lv_style_t style_secondary_button;
extern lv_style_t style_danger_button;
extern lv_style_t style_text_muted;

/*----------------
 * Fonts
 *----------------*/




/*----------------
 * Images
 *----------------*/



/*----------------
 * Subjects
 *----------------*/

extern lv_subject_t analysis_state;
extern lv_subject_t peak_frequency;
extern lv_subject_t peak_level;
extern lv_subject_t sample_rate;
extern lv_subject_t fft_size;
extern lv_subject_t frequency_band;
extern lv_subject_t successful_runs;
extern lv_subject_t failed_runs;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/*----------------
 * Event Callbacks
 *----------------*/

/**
 * Initialize the component library
 */

void signal_separation_ui_init_gen(const char * asset_path);

/**********************
 *      MACROS
 **********************/

/**********************
 *   POST INCLUDES
 **********************/

/*Include all the widgets, components and screens of this library*/
#include "components/app_metric_card_gen.h"
#include "screens/screen_calibration_gen.h"
#include "screens/screen_control_gen.h"
#include "screens/screen_monitor_gen.h"
#include "screens/screen_spectrum_gen.h"
#include "screens/screen_system_gen.h"
#include "screens/screen_test_gen.h"

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*SIGNAL_SEPARATION_UI_GEN_H*/