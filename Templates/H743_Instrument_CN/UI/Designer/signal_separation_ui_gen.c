/**
 * @file signal_separation_ui_gen.c
 */

/*********************
 *      INCLUDES
 *********************/

#include "signal_separation_ui_gen.h"

#if LV_USE_XML
#endif /* LV_USE_XML */

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void check_font(lv_font_t ** font, const char * name);

/**********************
 *  STATIC VARIABLES
 **********************/

static uint32_t signal_separation_ui_target = SIGNAL_SEPARATION_UI_TARGET_ALL;

/*----------------
 * Translations
 *----------------*/

/**********************
 *  GLOBAL VARIABLES
 **********************/

/*--------------------
 *  Permanent screens
 *-------------------*/

lv_obj_t * screen_monitor = NULL;

/*----------------
 * Fonts
 *----------------*/



/*----------------
 * Images
 *----------------*/



/*----------------
 * Global styles
 *----------------*/

lv_style_t style_screen;
lv_style_t style_header;
lv_style_t style_card;
lv_style_t style_nav;
lv_style_t style_nav_button;
lv_style_t style_nav_button_active;
lv_style_t style_primary_button;
lv_style_t style_secondary_button;
lv_style_t style_danger_button;
lv_style_t style_text_muted;

/*----------------
 * Subjects
 *----------------*/

lv_subject_t analysis_state;
lv_subject_t peak_frequency;
lv_subject_t peak_level;
lv_subject_t sample_rate;
lv_subject_t fft_size;
lv_subject_t frequency_band;
lv_subject_t successful_runs;
lv_subject_t failed_runs;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void signal_separation_ui_init_gen(const char * asset_path)
{
    char buf[256];


    /*----------------
     * Fonts
     *----------------*/




    /*----------------
     * Images
     *----------------*/



    /*----------------
     * Global styles
     *----------------*/

    static bool style_inited = false;

    if (!style_inited) {
        #if SIGNAL_SEPARATION_UI_CHECK_COMPILE_TARGET(SIGNAL_SEPARATION_UI_TARGET_ALL)
        if (signal_separation_ui_check_target(SIGNAL_SEPARATION_UI_TARGET_ALL)) {
            lv_style_init(&style_screen);
            lv_style_set_bg_color(&style_screen, COLOR_BG);
            lv_style_set_bg_opa(&style_screen, (255 * 100 / 100));
            lv_style_set_text_color(&style_screen, COLOR_TEXT);
            lv_style_set_border_width(&style_screen, 0);
            lv_style_set_radius(&style_screen, 0);
            lv_style_set_pad_all(&style_screen, 0);

            lv_style_init(&style_header);
            lv_style_set_width(&style_header, 800);
            lv_style_set_height(&style_header, 48);
            lv_style_set_bg_color(&style_header, COLOR_SURFACE);
            lv_style_set_bg_opa(&style_header, (255 * 100 / 100));
            lv_style_set_border_width(&style_header, 0);
            lv_style_set_radius(&style_header, 0);
            lv_style_set_pad_all(&style_header, 0);

            lv_style_init(&style_card);
            lv_style_set_bg_color(&style_card, COLOR_SURFACE);
            lv_style_set_bg_opa(&style_card, (255 * 100 / 100));
            lv_style_set_border_color(&style_card, COLOR_BORDER);
            lv_style_set_border_width(&style_card, 1);
            lv_style_set_radius(&style_card, 10);
            lv_style_set_pad_all(&style_card, 12);

            lv_style_init(&style_nav);
            lv_style_set_width(&style_nav, 800);
            lv_style_set_height(&style_nav, 56);
            lv_style_set_bg_color(&style_nav, COLOR_SURFACE);
            lv_style_set_bg_opa(&style_nav, (255 * 100 / 100));
            lv_style_set_border_color(&style_nav, COLOR_BORDER);
            lv_style_set_border_side(&style_nav, LV_BORDER_SIDE_TOP);
            lv_style_set_border_width(&style_nav, 1);
            lv_style_set_radius(&style_nav, 0);
            lv_style_set_pad_all(&style_nav, 0);

            lv_style_init(&style_nav_button);
            lv_style_set_bg_color(&style_nav_button, COLOR_SURFACE_ALT);
            lv_style_set_bg_opa(&style_nav_button, (255 * 100 / 100));
            lv_style_set_border_width(&style_nav_button, 0);
            lv_style_set_radius(&style_nav_button, 8);
            lv_style_set_text_color(&style_nav_button, COLOR_TEXT_MUTED);

            lv_style_init(&style_nav_button_active);
            lv_style_set_bg_color(&style_nav_button_active, COLOR_PRIMARY);
            lv_style_set_text_color(&style_nav_button_active, COLOR_TEXT);

            lv_style_init(&style_primary_button);
            lv_style_set_bg_color(&style_primary_button, COLOR_PRIMARY);
            lv_style_set_bg_opa(&style_primary_button, (255 * 100 / 100));
            lv_style_set_border_width(&style_primary_button, 0);
            lv_style_set_radius(&style_primary_button, 8);
            lv_style_set_text_color(&style_primary_button, COLOR_TEXT);

            lv_style_init(&style_secondary_button);
            lv_style_set_bg_color(&style_secondary_button, COLOR_SURFACE_ALT);
            lv_style_set_bg_opa(&style_secondary_button, (255 * 100 / 100));
            lv_style_set_border_color(&style_secondary_button, COLOR_BORDER);
            lv_style_set_border_width(&style_secondary_button, 1);
            lv_style_set_radius(&style_secondary_button, 8);
            lv_style_set_text_color(&style_secondary_button, COLOR_TEXT);

            lv_style_init(&style_danger_button);
            lv_style_set_bg_color(&style_danger_button, COLOR_DANGER);
            lv_style_set_bg_opa(&style_danger_button, (255 * 100 / 100));
            lv_style_set_border_width(&style_danger_button, 0);
            lv_style_set_radius(&style_danger_button, 8);
            lv_style_set_text_color(&style_danger_button, COLOR_TEXT);

            lv_style_init(&style_text_muted);
            lv_style_set_text_color(&style_text_muted, COLOR_TEXT_MUTED);

        }
        #endif
        style_inited = true;
    }

    /*----------------
     * Subjects
     *----------------*/
    static char analysis_state_buf[UI_SUBJECT_STRING_LENGTH];
    static char analysis_state_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&analysis_state,
                           analysis_state_buf,
                           analysis_state_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "就绪"
                          );
    static char peak_frequency_buf[UI_SUBJECT_STRING_LENGTH];
    static char peak_frequency_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&peak_frequency,
                           peak_frequency_buf,
                           peak_frequency_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "50.02"
                          );
    static char peak_level_buf[UI_SUBJECT_STRING_LENGTH];
    static char peak_level_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&peak_level,
                           peak_level_buf,
                           peak_level_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "-12.35"
                          );
    static char sample_rate_buf[UI_SUBJECT_STRING_LENGTH];
    static char sample_rate_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&sample_rate,
                           sample_rate_buf,
                           sample_rate_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "200 kS/s"
                          );
    static char fft_size_buf[UI_SUBJECT_STRING_LENGTH];
    static char fft_size_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&fft_size,
                           fft_size_buf,
                           fft_size_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "4096"
                          );
    static char frequency_band_buf[UI_SUBJECT_STRING_LENGTH];
    static char frequency_band_prev_buf[UI_SUBJECT_STRING_LENGTH];
    lv_subject_init_string(&frequency_band,
                           frequency_band_buf,
                           frequency_band_prev_buf,
                           UI_SUBJECT_STRING_LENGTH,
                           "0 - 100 kHz"
                          );
    lv_subject_init_int(&successful_runs, 12);
    lv_subject_set_min_value_int(&successful_runs, 0);
    lv_subject_init_int(&failed_runs, 0);
    lv_subject_set_min_value_int(&failed_runs, 0);

    /*----------------
     * Translations
     *----------------*/

#if LV_USE_XML
    /* Register widgets */


    /* Register fonts */

    /* Register subjects */
    lv_xml_register_subject(NULL, "analysis_state", &analysis_state);
    lv_xml_register_subject(NULL, "peak_frequency", &peak_frequency);
    lv_xml_register_subject(NULL, "peak_level", &peak_level);
    lv_xml_register_subject(NULL, "sample_rate", &sample_rate);
    lv_xml_register_subject(NULL, "fft_size", &fft_size);
    lv_xml_register_subject(NULL, "frequency_band", &frequency_band);
    lv_xml_register_subject(NULL, "successful_runs", &successful_runs);
    lv_xml_register_subject(NULL, "failed_runs", &failed_runs);

    /* Register callbacks */
#endif

    /* Register all the global assets so that they won't be created again when globals.xml is parsed.
     * While running in the editor skip this step to update the preview when the XML changes */
#if LV_USE_XML && !defined(LV_EDITOR_PREVIEW)
    /* Register images */
#endif

#if LV_USE_XML == 0
    /*--------------------
     *  Permanent screens
     *-------------------*/
    /* If XML is enabled it's assumed that the permanent screens are created
     * manually from XML using lv_xml_create() */
    /* To allow screens to reference each other, create them all before calling the sceen create functions */
    screen_monitor = lv_obj_create(NULL);

    screen_monitor_create();
#endif
}

void signal_separation_ui_set_target(uint32_t target)
{
    signal_separation_ui_target = target;
}

uint32_t signal_separation_ui_get_target(void)
{
    return signal_separation_ui_target;
}

bool signal_separation_ui_check_target(uint32_t target)
{
    return (signal_separation_ui_target & target) ? true : false;
}

/* Callbacks */

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void check_font(lv_font_t ** font, const char * name)
{
    if (!(*font)) {
        *font = (lv_font_t *)LV_FONT_DEFAULT;
        LV_LOG_WARN("font `%s` was not set. Using `LV_FONT_DEFAULT` instead", name);
    }
}