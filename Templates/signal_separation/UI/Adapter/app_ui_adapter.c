/**
  ******************************************************************************
  * @file    app_ui_adapter.c
  * @brief   单帧 ADC 波形、板内 UI FFT 与示波器页面适配（UTF-8）。
  ******************************************************************************
  */

#include "app_ui_adapter.h"

#include "app_main_task.h"
#include "app_calibration_service.h"
#include "app_output_task.h"
#include "app_dac_output_config.h"
#include "app_phase_lock_task.h"
#include "app_signal_result_view.h"
#include "arm_math.h"
#include "signal_separation_ui.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define APP_UI_POLL_PERIOD_MS          50U
#define APP_UI_WAVEFORM_POINTS         128U
#define APP_UI_FFT_SIZE                128U
#define APP_UI_SPECTRUM_POINTS         (APP_UI_FFT_SIZE / 2U)
#define APP_UI_ADC_MAX_CODE            4095U
#define APP_UI_CHART_MAX_VALUE         100
#define APP_UI_SPECTRUM_FLOOR_DB       (-100.0f)
#define APP_UI_PI_F                    3.14159265358979323846f

typedef enum
{
    APP_UI_DISPLAY_FFT = 0,
    APP_UI_DISPLAY_WAVEFORM = 1
} AppUiDisplayMode;

typedef struct
{
    bool valid;
    bool formal_fft_valid;
    bool formal_dual_valid;
    bool formal_harmonic_valid;
    bool formal_joint_valid;
    uint16_t waveform[APP_UI_WAVEFORM_POINTS];
    int32_t spectrum[APP_UI_SPECTRUM_POINTS];
    uint16_t waveform_point_count;
    uint16_t spectrum_point_count;
    uint32_t sample_rate_hz;
    uint32_t formal_candidate_count;
    uint32_t formal_dual_tone_count;
    uint32_t formal_harmonic_count;
    AppSignalResultViewData result;
    uint32_t formal_peak_frequency_hz;
    int32_t formal_peak_snr_q100;
    uint32_t peak_frequency_hz;
    int32_t peak_db_q100;
} AppUiFrameSnapshot;

static AppUiFrameSnapshot s_snapshot;
static volatile uint32_t s_snapshot_sequence = 0U;
static float32_t s_fft_input[APP_UI_FFT_SIZE];
static float32_t s_fft_output[APP_UI_FFT_SIZE];
static int32_t s_fft_cache[APP_UI_SPECTRUM_POINTS];
static uint32_t s_fft_cache_sequence = 0U;
static uint32_t s_fft_cache_peak_frequency_hz = 0U;
static int32_t s_fft_cache_peak_db_q100 = -10000;
static uint16_t s_fft_cache_point_count = 0U;

static bool s_initialized = false;
static uint32_t s_last_poll_tick = 0U;
static uint32_t s_last_rendered_sequence = 0U;
static AppUiDisplayMode s_display_mode = APP_UI_DISPLAY_WAVEFORM;

static lv_obj_t *s_bound_screen = NULL;
static lv_obj_t *s_active_chart = NULL;
static lv_chart_series_t *s_active_series = NULL;
static lv_obj_t *s_active_dropdown = NULL;
static lv_obj_t *s_active_title = NULL;
static lv_obj_t *s_peak_frequency_label = NULL;
static lv_obj_t *s_peak_level_label = NULL;
static lv_obj_t *s_peak_level_unit_label = NULL;
static lv_obj_t *s_spectrum_info_label = NULL;
static lv_obj_t *s_phase_decrease_button = NULL;
static lv_obj_t *s_phase_value_label = NULL;
static lv_obj_t *s_phase_increase_button = NULL;
static lv_obj_t *s_start_button = NULL;
static lv_obj_t *s_stop_button = NULL;
/* 一次分析结果已显示时为 true，用于 TRACKING/ANALYSIS DONE 状态跟随。 */
static bool s_vote_state_shown = false;
static bool s_vote_tracking_shown = false;
static lv_obj_t *app_ui_find_named(lv_obj_t *parent, const char *name);
static bool app_ui_bind_screen(lv_obj_t *screen);
static void app_ui_bind_active_screen(void);
static void app_ui_mode_changed(lv_event_t *event);
static void app_ui_start_clicked(lv_event_t *event);
static void app_ui_stop_clicked(lv_event_t *event);
static void app_ui_phase_decrease_clicked(lv_event_t *event);
static void app_ui_phase_increase_clicked(lv_event_t *event);
static void app_ui_refresh_phase_control(void);
static void app_ui_update_mode_labels(void);
static bool app_ui_copy_snapshot(AppUiFrameSnapshot *snapshot,
                                 uint32_t *sequence);
static void app_ui_prepare_spectrum(AppUiFrameSnapshot *snapshot,
                                    uint32_t sequence);
static void app_ui_update_subjects(const AppUiFrameSnapshot *snapshot);
static void app_ui_render_snapshot(const AppUiFrameSnapshot *snapshot,
                                   uint32_t sequence);

/*@brief:统一控制 LV_STATE_DISABLED，避免在各处重复 if/else。 */
static void app_ui_set_disabled(lv_obj_t *object, bool disabled)
{
    if (object == NULL)
    {
        return;
    }
    if (disabled)
    {
        lv_obj_add_state(object, LV_STATE_DISABLED);
    }
    else
    {
        lv_obj_remove_state(object, LV_STATE_DISABLED);
    }
}

/*
 * P2-01 整改：Control/System 页面尚未实现业务逻辑的按钮必须呈现
 * disabled 状态，不允许保持普通可点击外观。逻辑落地后从表中移除。
 */
static const char *const s_placeholder_button_labels[] = {
    "Select",
    "Edit",
    "APPLY",
    "Touch Test",
    "Display Test",
    "Memory Status",
};

/*@brief:递归禁用所有占位按钮（按钮内标签文本匹配占位表）。 */
static void app_ui_disable_placeholder_buttons(lv_obj_t *object)
{
    if (object == NULL)
    {
        return;
    }

    if (lv_obj_check_type(object, &lv_button_class))
    {
        uint32_t child_count = lv_obj_get_child_count(object);
        for (uint32_t index = 0U; index < child_count; ++index)
        {
            lv_obj_t *child = lv_obj_get_child(object, (int32_t)index);
            if (!lv_obj_check_type(child, &lv_label_class))
            {
                continue;
            }
            const char *text = lv_label_get_text(child);
            if (text == NULL)
            {
                continue;
            }
            for (uint32_t label_index = 0U;
                 label_index < (sizeof(s_placeholder_button_labels)
                                / sizeof(s_placeholder_button_labels[0]));
                 ++label_index)
            {
                if (strcmp(text,
                           s_placeholder_button_labels[label_index]) == 0)
                {
                    lv_obj_add_state(object, LV_STATE_DISABLED);
                    return;
                }
            }
        }
    }

    uint32_t child_count = lv_obj_get_child_count(object);
    for (uint32_t index = 0U; index < child_count; ++index)
    {
        app_ui_disable_placeholder_buttons(
            lv_obj_get_child(object, (int32_t)index));
    }
}

/*
 * P1-02 整改：RUN/STOP 按钮使能由业务状态统一驱动：
 * RUN  仅在分析空闲且校准未独占时可点；
 * STOP 仅在分析进行中或持续锁相输出时可点。
 */
static void app_ui_refresh_run_buttons(void)
{
    bool analysis_busy = AppMain_IsRunning();
    bool tracking = AppPhaseLockTask_IsRunning();
    bool calibration_locked = AppCalibrationService_IsExclusive();

    app_ui_set_disabled(s_start_button,
                        analysis_busy || calibration_locked);
    app_ui_set_disabled(s_stop_button,
                        !analysis_busy && !tracking);
}

/*
 * P1-01 整改：一次分析完成后，持续锁相输出期间显示 TRACKING，
 * 锁相停止后回落为 ANALYSIS DONE，不再统一显示误导性的 FINISHED。
 */
static void app_ui_refresh_tracking_state(void)
{
    if (!s_vote_state_shown)
    {
        return;
    }

    bool tracking = AppPhaseLockTask_IsRunning();
    if (tracking != s_vote_tracking_shown)
    {
        lv_subject_copy_string(&analysis_state,
                               tracking ? "TRACKING" : "ANALYSIS DONE");
        s_vote_tracking_shown = tracking;
    }
}

/*@brief:递归按稳定对象名查找控件，避免依赖模板内部节点序号。 */
static lv_obj_t *app_ui_find_named(lv_obj_t *parent, const char *name)
{
    if ((parent == NULL) || (name == NULL))
    {
        return NULL;
    }

    const char *object_name = lv_obj_get_name(parent);
    if ((object_name != NULL) && (strcmp(object_name, name) == 0))
    {
        return parent;
    }

    uint32_t child_count = lv_obj_get_child_count(parent);
    for (uint32_t index = 0U; index < child_count; ++index)
    {
        lv_obj_t *result = app_ui_find_named(
            lv_obj_get_child(parent, (int32_t)index), name);
        if (result != NULL)
        {
            return result;
        }
    }
    return NULL;
}

/*@brief:绑定主页面或动态创建的全屏页面，并恢复当前显示模式。 */
static bool app_ui_bind_screen(lv_obj_t *screen)
{
    if (screen == NULL)
    {
        return false;
    }

    /* 任意页面进入时先禁用未实现的占位按钮（Control/System 等）。 */
    app_ui_disable_placeholder_buttons(screen);

    s_active_chart = app_ui_find_named(screen, "ui_chart");
    s_active_dropdown = app_ui_find_named(screen, "ui_mode_dropdown");
    s_active_title = app_ui_find_named(screen, "ui_title");
    s_peak_frequency_label = app_ui_find_named(screen, "ui_peak_frequency");
    s_peak_level_label = app_ui_find_named(screen, "ui_peak_level");
    s_peak_level_unit_label = app_ui_find_named(screen,
                                                 "ui_peak_level_unit");
    s_spectrum_info_label = app_ui_find_named(screen, "ui_spectrum_info");
    s_phase_decrease_button = app_ui_find_named(screen,
                                                 "ui_phase_decrease");
    s_phase_value_label = app_ui_find_named(screen, "ui_phase_value");
    s_phase_increase_button = app_ui_find_named(screen,
                                                 "ui_phase_increase");
    AppSignalResultView_Bind(screen);
    s_start_button = app_ui_find_named(screen, "ui_start_button");
    s_stop_button = app_ui_find_named(screen, "ui_stop_button");
    lv_obj_t *start_button = s_start_button;
    lv_obj_t *stop_button = s_stop_button;

    if ((s_active_chart == NULL)
        || (s_active_dropdown == NULL)
        || (s_active_title == NULL)
        || !lv_obj_check_type(s_active_chart, &lv_chart_class))
    {
        s_bound_screen = screen;
        s_active_series = NULL;
        return false;
    }

    s_active_series = lv_chart_get_series_next(s_active_chart, NULL);
    if (s_active_series == NULL)
    {
        s_bound_screen = screen;
        return false;
    }

    s_bound_screen = screen;
    lv_dropdown_set_selected(s_active_dropdown,
                             (uint32_t)s_display_mode);
    lv_obj_remove_event_cb(s_active_dropdown, app_ui_mode_changed);
    lv_obj_add_event_cb(s_active_dropdown,
                        app_ui_mode_changed,
                        LV_EVENT_VALUE_CHANGED,
                        NULL);
    if (start_button != NULL)
    {
        lv_obj_remove_event_cb(start_button, app_ui_start_clicked);
        lv_obj_add_event_cb(start_button,
                            app_ui_start_clicked,
                            LV_EVENT_CLICKED,
                            NULL);
    }
    if (stop_button != NULL)
    {
        lv_obj_remove_event_cb(stop_button, app_ui_stop_clicked);
        lv_obj_add_event_cb(stop_button,
                            app_ui_stop_clicked,
                            LV_EVENT_CLICKED,
                             NULL);
    }
    if (s_phase_decrease_button != NULL)
    {
        lv_obj_remove_event_cb(s_phase_decrease_button,
                               app_ui_phase_decrease_clicked);
        lv_obj_add_event_cb(s_phase_decrease_button,
                            app_ui_phase_decrease_clicked,
                            LV_EVENT_CLICKED,
                            NULL);
    }
    if (s_phase_increase_button != NULL)
    {
        lv_obj_remove_event_cb(s_phase_increase_button,
                               app_ui_phase_increase_clicked);
        lv_obj_add_event_cb(s_phase_increase_button,
                            app_ui_phase_increase_clicked,
                            LV_EVENT_CLICKED,
                            NULL);
    }
    s_last_rendered_sequence = 0U;
    app_ui_update_mode_labels();
    app_ui_refresh_phase_control();
    app_ui_refresh_run_buttons();
    return true;
}

/*@brief:页面切换后重新绑定，避免持有已销毁页面的控件指针。 */
static void app_ui_bind_active_screen(void)
{
    lv_obj_t *active_screen = lv_screen_active();
    if ((active_screen == NULL) || (active_screen == s_bound_screen))
    {
        return;
    }

    (void)app_ui_bind_screen(active_screen);
}

static void app_ui_mode_changed(lv_event_t *event)
{
    lv_obj_t *dropdown = lv_event_get_target_obj(event);
    s_display_mode = (lv_dropdown_get_selected(dropdown) == 0U)
        ? APP_UI_DISPLAY_FFT
        : APP_UI_DISPLAY_WAVEFORM;
    s_last_rendered_sequence = 0U;
    app_ui_update_mode_labels();
}

static void app_ui_start_clicked(lv_event_t *event)
{
    (void)event;
    s_vote_state_shown = false;
    lv_subject_copy_string(&analysis_state,
                           AppMain_RequestStart() ? "RUNNING" : "BUSY");
    app_ui_refresh_run_buttons();
}

static void app_ui_stop_clicked(lv_event_t *event)
{
    (void)event;
    if (AppMain_RequestStop())
    {
        s_vote_state_shown = false;
        lv_subject_copy_string(&analysis_state, "STOPPING");
    }
    app_ui_refresh_run_buttons();
}

static void app_ui_refresh_phase_control(void)
{
    uint16_t phase_deg = AppPhaseLockTask_GetOutputPhaseDifference();
    bool phase_editable = !AppPhaseLockTask_IsRunning();
    if (s_phase_value_label != NULL)
    {
        char buffer[16];
        lv_snprintf(buffer, sizeof(buffer), "%u deg",
                    (unsigned int)phase_deg);
        lv_label_set_text(s_phase_value_label, buffer);
    }
    if (s_phase_decrease_button != NULL)
    {
        if (!phase_editable
            || (phase_deg <= APP_DAC_OUTPUT_PHASE_MIN_DEG))
        {
            lv_obj_add_state(s_phase_decrease_button, LV_STATE_DISABLED);
        }
        else
        {
            lv_obj_remove_state(s_phase_decrease_button, LV_STATE_DISABLED);
        }
    }
    if (s_phase_increase_button != NULL)
    {
        if (!phase_editable
            || (phase_deg >= APP_DAC_OUTPUT_PHASE_MAX_DEG))
        {
            lv_obj_add_state(s_phase_increase_button, LV_STATE_DISABLED);
        }
        else
        {
            lv_obj_remove_state(s_phase_increase_button, LV_STATE_DISABLED);
        }
    }
}

static void app_ui_phase_decrease_clicked(lv_event_t *event)
{
    (void)event;
    if (AppPhaseLockTask_IsRunning())
    {
        app_ui_refresh_phase_control();
        return;
    }

    uint16_t phase_deg = AppPhaseLockTask_GetOutputPhaseDifference();
    if (phase_deg >= (APP_DAC_OUTPUT_PHASE_MIN_DEG
                      + APP_DAC_OUTPUT_PHASE_STEP_DEG))
    {
        (void)AppPhaseLockTask_SetOutputPhaseDifference(
            phase_deg - APP_DAC_OUTPUT_PHASE_STEP_DEG);
    }
    app_ui_refresh_phase_control();
}

static void app_ui_phase_increase_clicked(lv_event_t *event)
{
    (void)event;
    if (AppPhaseLockTask_IsRunning())
    {
        app_ui_refresh_phase_control();
        return;
    }

    uint16_t phase_deg = AppPhaseLockTask_GetOutputPhaseDifference();
    if (phase_deg <= (APP_DAC_OUTPUT_PHASE_MAX_DEG
                      - APP_DAC_OUTPUT_PHASE_STEP_DEG))
    {
        (void)AppPhaseLockTask_SetOutputPhaseDifference(
            phase_deg + APP_DAC_OUTPUT_PHASE_STEP_DEG);
    }
    app_ui_refresh_phase_control();
}

/*@brief:明确区分板内 UI FFT 与串口单帧波形，避免误解输出路径。 */
static void app_ui_update_mode_labels(void)
{
    if (s_active_title == NULL)
    {
        return;
    }

    const char *screen_name = (s_bound_screen != NULL)
        ? lv_obj_get_name(s_bound_screen)
        : NULL;
    bool full_page = (screen_name != NULL)
        && (strncmp(screen_name, "screen_spectrum", 15U) == 0);

    if (s_display_mode == APP_UI_DISPLAY_WAVEFORM)
    {
        lv_label_set_text(s_active_title,
                          full_page
                              ? "FULL MIXED WAVEFORM"
                              : "MIXED INPUT WAVEFORM");
        if (s_spectrum_info_label != NULL)
        {
            lv_label_set_text(s_spectrum_info_label,
                              "128 samples / formal E3 readout");
        }
    }
    else
    {
        lv_label_set_text(s_active_title,
                          full_page
                              ? "FULL SPECTRUM PREVIEW"
                              : "SPECTRUM PREVIEW");
        if (s_spectrum_info_label != NULL)
        {
            lv_label_set_text(s_spectrum_info_label,
                              "128-point preview / formal E3 readout");
        }
    }
}

/*@brief:使用序列锁复制 MainTask 发布的快照，不在 MainTask 中调用 LVGL。 */
static bool app_ui_copy_snapshot(AppUiFrameSnapshot *snapshot,
                                 uint32_t *sequence)
{
    if ((snapshot == NULL) || (sequence == NULL))
    {
        return false;
    }

    for (uint32_t attempt = 0U; attempt < 3U; ++attempt)
    {
        uint32_t before = s_snapshot_sequence;
        if ((before & 1U) != 0U)
        {
            continue;
        }

        __DMB();
        memcpy(snapshot, &s_snapshot, sizeof(*snapshot));
        __DMB();
        uint32_t after = s_snapshot_sequence;
        if ((before == after) && ((after & 1U) == 0U))
        {
            *sequence = after;
            return snapshot->valid;
        }
    }
    return false;
}

/*@brief:在 DisplayTask 中为同一帧只计算一次 128 点 UI FFT。 */
static void app_ui_prepare_spectrum(AppUiFrameSnapshot *snapshot,
                                    uint32_t sequence)
{
    if ((snapshot == NULL) || !snapshot->valid)
    {
        return;
    }

    if (s_fft_cache_sequence != sequence)
    {
        float32_t mean = 0.0f;
        for (uint32_t index = 0U; index < APP_UI_FFT_SIZE; ++index)
        {
            mean += (float32_t)snapshot->waveform[index];
        }
        mean /= (float32_t)APP_UI_FFT_SIZE;

        for (uint32_t index = 0U; index < APP_UI_FFT_SIZE; ++index)
        {
            float32_t window = 0.5f - 0.5f * cosf(
                (2.0f * APP_UI_PI_F * (float32_t)index)
                / (float32_t)(APP_UI_FFT_SIZE - 1U));
            s_fft_input[index] =
                (((float32_t)snapshot->waveform[index] - mean) / 2048.0f)
                * window;
        }

        arm_rfft_fast_instance_f32 rfft;
        bool fft_ready = (arm_rfft_fast_init_f32(&rfft, APP_UI_FFT_SIZE)
                          == ARM_MATH_SUCCESS);
        uint32_t peak_bin = 0U;
        float32_t peak_db = APP_UI_SPECTRUM_FLOOR_DB;
        s_fft_cache_point_count = 0U;

        if (fft_ready)
        {
            arm_rfft_fast_f32(&rfft, s_fft_input, s_fft_output, 0U);
            s_fft_cache[0] = 0;
            for (uint32_t bin = 1U; bin < APP_UI_SPECTRUM_POINTS; ++bin)
            {
                float32_t real = s_fft_output[2U * bin];
                float32_t imaginary = s_fft_output[2U * bin + 1U];
                float32_t magnitude = sqrtf(
                    real * real + imaginary * imaginary)
                    * (4.0f / (float32_t)APP_UI_FFT_SIZE);
                if (magnitude < 0.000001f)
                {
                    magnitude = 0.000001f;
                }

                float32_t db = 20.0f * log10f(magnitude);
                if (db > 0.0f)
                {
                    db = 0.0f;
                }
                if (db < APP_UI_SPECTRUM_FLOOR_DB)
                {
                    db = APP_UI_SPECTRUM_FLOOR_DB;
                }
                if (db > peak_db)
                {
                    peak_db = db;
                    peak_bin = bin;
                }

                s_fft_cache[bin] = (int32_t)(
                    ((db - APP_UI_SPECTRUM_FLOOR_DB)
                     * (float32_t)APP_UI_CHART_MAX_VALUE)
                    / (-APP_UI_SPECTRUM_FLOOR_DB));
            }
            s_fft_cache_point_count = APP_UI_SPECTRUM_POINTS;
        }

        s_fft_cache_peak_frequency_hz = (uint32_t)(
            ((uint64_t)peak_bin * snapshot->sample_rate_hz
             + (APP_UI_FFT_SIZE / 2U))
            / APP_UI_FFT_SIZE);
        s_fft_cache_peak_db_q100 = (int32_t)(peak_db * 100.0f);
        s_fft_cache_sequence = sequence;
    }

    snapshot->spectrum_point_count = s_fft_cache_point_count;
    memcpy(snapshot->spectrum,
           s_fft_cache,
           sizeof(s_fft_cache));
    snapshot->peak_frequency_hz = snapshot->formal_fft_valid
        ? snapshot->formal_peak_frequency_hz
        : s_fft_cache_peak_frequency_hz;
    snapshot->peak_db_q100 = snapshot->formal_fft_valid
        ? snapshot->formal_peak_snr_q100
        : s_fft_cache_peak_db_q100;
}

/*@brief:刷新 subjects 和主页面读数；所有 LVGL 调用均留在 DisplayTask。 */
static void app_ui_update_subjects(const AppUiFrameSnapshot *snapshot)
{
    char buffer[48];

    /* 默认清除跟踪态标志，仅投票有效分支重新置位。 */
    s_vote_state_shown = false;

    if (snapshot->result.vote_present)
    {
        if (snapshot->result.vote_valid)
        {
            /*
             * 一次分析成功后 MainTask 会请求持续锁相输出：
             * 锁相运行期间显示 TRACKING，锁相停止后为 ANALYSIS DONE。
             */
            s_vote_tracking_shown = AppPhaseLockTask_IsRunning();
            s_vote_state_shown = true;
            lv_snprintf(buffer,
                        sizeof(buffer),
                        "%s",
                        s_vote_tracking_shown
                            ? "TRACKING" : "ANALYSIS DONE");
        }
        else
        {
            uint32_t model_ambiguous_mask =
                1UL << (uint32_t)SIGNAL_DETECT_ERR_MODEL_AMBIGUOUS;
            if (((snapshot->result.reason_mask & model_ambiguous_mask) != 0U)
                && (snapshot->formal_candidate_count
                    < SIGNAL_JOINT_COMPONENT_COUNT))
            {
                lv_snprintf(buffer, sizeof(buffer), "ERROR: ONE SIGNAL");
            }
            else
            {
                lv_snprintf(buffer, sizeof(buffer), "ERROR");
            }
        }
        lv_subject_copy_string(&analysis_state, buffer);
    }
    else if (snapshot->formal_joint_valid || snapshot->formal_harmonic_valid)
    {
        if (snapshot->formal_harmonic_count >= 2U)
        {
            lv_snprintf(buffer,
                        sizeof(buffer),
                        "%s + %s",
                        SignalHarmonic_WaveTypeString(
                            snapshot->result.wave_types[0]),
                        SignalHarmonic_WaveTypeString(
                            snapshot->result.wave_types[1]));
        }
        else
        {
            lv_snprintf(buffer,
                        sizeof(buffer),
                        "%s",
                        SignalHarmonic_WaveTypeString(
                            snapshot->result.wave_types[0]));
        }
        lv_subject_copy_string(&analysis_state, buffer);
    }
    else if (snapshot->formal_dual_valid)
    {
        lv_snprintf(buffer,
                    sizeof(buffer),
                    "E4 PASS %lu",
                    (unsigned long)snapshot->formal_dual_tone_count);
        lv_subject_copy_string(&analysis_state, buffer);
    }
    else if (snapshot->formal_fft_valid)
    {
        lv_snprintf(buffer,
                    sizeof(buffer),
                    "E3 PASS %lu",
                    (unsigned long)snapshot->formal_candidate_count);
        lv_subject_copy_string(&analysis_state, buffer);
    }
    else
    {
        lv_subject_copy_string(&analysis_state, "E3 NO PEAK");
    }

    uint32_t frequency_hundredths_khz =
        (snapshot->peak_frequency_hz + 5U) / 10U;
    lv_snprintf(buffer,
                sizeof(buffer),
                "%lu.%02lu",
                (unsigned long)(frequency_hundredths_khz / 100U),
                (unsigned long)(frequency_hundredths_khz % 100U));
    lv_subject_copy_string(&peak_frequency, buffer);
    if (s_peak_frequency_label != NULL)
    {
        lv_label_set_text(s_peak_frequency_label, buffer);
    }

    int32_t level_q100 = snapshot->peak_db_q100;
    bool negative = (level_q100 < 0);
    if (negative)
    {
        level_q100 = -level_q100;
    }
    lv_snprintf(buffer,
                sizeof(buffer),
                "%s%ld.%02ld",
                negative ? "-" : "",
                (long)(level_q100 / 100),
                (long)(level_q100 % 100));
    lv_subject_copy_string(&peak_level, buffer);
    if (s_peak_level_label != NULL)
    {
        lv_label_set_text(s_peak_level_label, buffer);
    }
    if (s_peak_level_unit_label != NULL)
    {
        lv_label_set_text(s_peak_level_unit_label,
                          snapshot->formal_fft_valid ? "dB" : "dBFS");
    }

    lv_snprintf(buffer,
                sizeof(buffer),
                "%lu kS/s",
                (unsigned long)((snapshot->sample_rate_hz + 500U) / 1000U));
    lv_subject_copy_string(&sample_rate, buffer);
    lv_subject_copy_string(&fft_size,
                           snapshot->formal_fft_valid ? "4096" : "128");

    lv_snprintf(buffer,
                sizeof(buffer),
                "0 - %lu kHz",
                (unsigned long)(snapshot->sample_rate_hz / 2000U));
    lv_subject_copy_string(&frequency_band, buffer);
    lv_subject_set_int(&successful_runs,
                       snapshot->result.vote_valid ? 1 : 0);
    lv_subject_set_int(&failed_runs,
                       snapshot->result.vote_valid ? 0 : 1);
    AppSignalResultView_Render(&snapshot->result);
}

static void app_ui_render_snapshot(const AppUiFrameSnapshot *snapshot,
                                   uint32_t sequence)
{
    if ((s_active_chart == NULL) || (s_active_series == NULL)
        || (snapshot == NULL) || (sequence == s_last_rendered_sequence))
    {
        return;
    }

    static int32_t chart_values[APP_UI_WAVEFORM_POINTS];
    uint16_t point_count = 0U;

    if (s_display_mode == APP_UI_DISPLAY_WAVEFORM)
    {
        point_count = snapshot->waveform_point_count;
        for (uint16_t index = 0U; index < point_count; ++index)
        {
            chart_values[index] = (int32_t)(
                ((uint32_t)snapshot->waveform[index]
                 * APP_UI_CHART_MAX_VALUE
                 + (APP_UI_ADC_MAX_CODE / 2U))
                / APP_UI_ADC_MAX_CODE);
        }
    }
    else
    {
        point_count = snapshot->spectrum_point_count;
        for (uint16_t index = 0U; index < point_count; ++index)
        {
            chart_values[index] = snapshot->spectrum[index];
        }
    }

    if (point_count == 0U)
    {
        return;
    }

    lv_chart_set_point_count(s_active_chart, point_count);
    lv_chart_set_series_values(s_active_chart,
                               s_active_series,
                               chart_values,
                               point_count);
    lv_chart_refresh(s_active_chart);
    app_ui_update_subjects(snapshot);
    s_last_rendered_sequence = sequence;
}

bool AppUiAdapter_SubmitAdcFrame(const uint16_t *samples,
                                 uint32_t sample_count,
                                 uint32_t sample_rate_hz,
                                 const SignalFftResult *fft_result,
                                 const SignalDualSineResult *dual_result,
                                 const SignalHarmonicResult *harmonic_result,
                                 const SignalJointResult *joint_result,
                                 const SignalVoteResult *vote_result)
{
    if ((samples == NULL) || (sample_count < APP_UI_WAVEFORM_POINTS)
        || (sample_rate_hz == 0U))
    {
        return false;
    }

    uint32_t begin_sequence = (s_snapshot_sequence + 1U) | 1U;
    s_snapshot_sequence = begin_sequence;
    __DMB();

    uint32_t first = (sample_count - APP_UI_WAVEFORM_POINTS) / 2U;
    for (uint32_t index = 0U; index < APP_UI_WAVEFORM_POINTS; ++index)
    {
        uint16_t sample = samples[first + index] & APP_UI_ADC_MAX_CODE;
        s_snapshot.waveform[index] = sample;
    }

    s_snapshot.waveform_point_count = APP_UI_WAVEFORM_POINTS;
    s_snapshot.spectrum_point_count = 0U;
    s_snapshot.sample_rate_hz = sample_rate_hz;
    s_snapshot.formal_fft_valid = (fft_result != NULL)
        && fft_result->valid
        && (fft_result->selected_candidate_count > 0U);
    s_snapshot.formal_candidate_count = s_snapshot.formal_fft_valid
        ? fft_result->selected_candidate_count : 0U;
    s_snapshot.formal_dual_valid = (dual_result != NULL)
        && dual_result->valid;
    s_snapshot.formal_dual_tone_count = s_snapshot.formal_dual_valid
        ? SIGNAL_DUAL_SINE_TONE_COUNT
        : 0U;
    s_snapshot.formal_harmonic_valid = (harmonic_result != NULL)
        && harmonic_result->valid
        && (harmonic_result->component_count > 0U);
    s_snapshot.formal_joint_valid = (joint_result != NULL)
        && joint_result->valid;
    s_snapshot.result.vote_present = (vote_result != NULL);
    s_snapshot.result.vote_valid = s_snapshot.result.vote_present
        && vote_result->valid;
    s_snapshot.result.vote_error = s_snapshot.result.vote_present
        ? vote_result->error : SIGNAL_DETECT_ERR_NONE;
    s_snapshot.result.requested_frames = s_snapshot.result.vote_present
        ? vote_result->requested_frames : 0U;
    s_snapshot.result.valid_frames = s_snapshot.result.vote_present
        ? vote_result->valid_frames : 0U;
    s_snapshot.result.reason_mask = s_snapshot.result.vote_present
        ? vote_result->invalid_reason_mask : 0U;
    s_snapshot.result.residual_q100000 = s_snapshot.result.vote_present
        ? (uint32_t)(vote_result->median_residual * 100000.0f + 0.5f)
        : 0U;
    s_snapshot.result.confidence_q1000 = s_snapshot.result.vote_present
        ? (uint32_t)(vote_result->total_confidence * 1000.0f + 0.5f)
        : 0U;
    s_snapshot.result.session_time_ms = s_snapshot.result.vote_present
        ? vote_result->session_time_ms : 0U;
    s_snapshot.result.candidate_count = s_snapshot.formal_candidate_count;
    s_snapshot.result.evaluated_pairs = (joint_result != NULL)
        ? joint_result->evaluated_pairs : 0U;
    s_snapshot.result.evaluated_models = (joint_result != NULL)
        ? joint_result->evaluated_models : 0U;
    s_snapshot.formal_harmonic_count = s_snapshot.result.vote_valid
        ? SIGNAL_JOINT_COMPONENT_COUNT
        : (s_snapshot.formal_joint_valid
        ? SIGNAL_JOINT_COMPONENT_COUNT
        : (s_snapshot.formal_harmonic_valid
           ? harmonic_result->component_count : 0U));
    for (uint32_t index = 0U;
         index < SIGNAL_HARMONIC_MAX_COMPONENTS;
         ++index)
    {
        if (s_snapshot.result.vote_valid)
        {
            s_snapshot.result.wave_types[index] =
                vote_result->components[index].type;
        }
        else if (s_snapshot.formal_joint_valid)
        {
            s_snapshot.result.wave_types[index] =
                joint_result->components[index].type;
        }
        else
        {
            s_snapshot.result.wave_types[index] =
                (s_snapshot.formal_harmonic_valid
                 && (index < harmonic_result->component_count))
                 ? harmonic_result->components[index].type
                 : SIGNAL_WAVE_UNKNOWN;
        }

        if ((index < SIGNAL_JOINT_COMPONENT_COUNT)
            && s_snapshot.result.vote_valid)
        {
            const SignalVoteComponent *component =
                &vote_result->components[index];
            s_snapshot.result.component_frequency_hz[index] =
                (uint32_t)(component->frequency_hz + 0.5f);
            s_snapshot.result.component_amplitude_q100[index] =
                (uint32_t)(component->amplitude_code * 100.0f + 0.5f);
            s_snapshot.result.component_frequency_votes[index] =
                component->frequency_votes;
            s_snapshot.result.component_type_votes[index] =
                component->type_votes;
        }
        else if (index < SIGNAL_JOINT_COMPONENT_COUNT)
        {
            if (s_snapshot.formal_harmonic_valid
                && (index < harmonic_result->component_count))
            {
                const SignalHarmonicComponent *component =
                    &harmonic_result->components[index];
                s_snapshot.result.component_frequency_hz[index] =
                    (uint32_t)(component->frequency_hz + 0.5f);
                s_snapshot.result.component_amplitude_q100[index] =
                    (uint32_t)(component->feature.a1 * 100.0f + 0.5f);
            }
            else
            {
                s_snapshot.result.component_frequency_hz[index] = 0U;
                s_snapshot.result.component_amplitude_q100[index] = 0U;
            }
            s_snapshot.result.component_frequency_votes[index] = 0U;
            s_snapshot.result.component_type_votes[index] = 0U;
        }
    }
    s_snapshot.formal_peak_frequency_hz = s_snapshot.result.vote_valid
        ? s_snapshot.result.component_frequency_hz[0]
        : (s_snapshot.formal_fft_valid
           ? (uint32_t)(fft_result->selected[0].measured_frequency_hz + 0.5f)
           : 0U);
    s_snapshot.formal_peak_snr_q100 = s_snapshot.formal_fft_valid
        ? (int32_t)(fft_result->selected[0].snr_db * 100.0f
                    + ((fft_result->selected[0].snr_db >= 0.0f)
                       ? 0.5f : -0.5f))
        : -10000;
    s_snapshot.peak_frequency_hz = 0U;
    s_snapshot.peak_db_q100 = -10000;
    s_snapshot.valid = true;

    __DMB();
    s_snapshot_sequence = begin_sequence + 1U;
    return true;
}

void AppUiAdapter_NotifyPageChanged(void)
{
    if (!s_initialized)
    {
        return;
    }

    /* 页面切换后立即重绑定，串口 PAGE+TAP 序列不依赖 50ms 轮询命中。 */
    app_ui_bind_active_screen();
}

bool AppUiAdapter_Init(lv_obj_t *monitor_screen)
{
    s_initialized = app_ui_bind_screen(monitor_screen);
    if (!s_initialized)
    {
        /* P2-04：Monitor 绑定失败会连带停用全部页面逻辑，必须可见。 */
        (void)AppOutput_Logf("[UI] BIND MONITOR FAIL\r\n");
    }
    s_last_poll_tick = lv_tick_get() - APP_UI_POLL_PERIOD_MS;
    lv_subject_copy_string(&analysis_state, "WAIT");

    if (s_initialized)
    {
        AppUiAdapter_Run();
    }
    return s_initialized;
}

void AppUiAdapter_Run(void)
{
    if (!s_initialized)
    {
        return;
    }

    uint32_t now = lv_tick_get();
    if ((now - s_last_poll_tick) < APP_UI_POLL_PERIOD_MS)
    {
        return;
    }
    s_last_poll_tick = now;

    app_ui_bind_active_screen();
    app_ui_refresh_phase_control();
    app_ui_refresh_run_buttons();
    app_ui_refresh_tracking_state();
    AppSignalResultView_RefreshPhaseLock();

    AppUiFrameSnapshot snapshot;
    uint32_t sequence = 0U;
    if (app_ui_copy_snapshot(&snapshot, &sequence))
    {
        app_ui_prepare_spectrum(&snapshot, sequence);
        app_ui_render_snapshot(&snapshot, sequence);
    }
}
