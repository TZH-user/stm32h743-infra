/**
  ******************************************************************************
  * @file    app_signal_result_view.c
  * @brief   E7 两路结果、质量字段和错误原因显示（UTF-8）。
  ******************************************************************************
  */

#include "app_signal_result_view.h"

#include "app_dac_output.h"
#include "app_phase_lock.h"
#include "signal_harmonic.h"

#include <stdlib.h>
#include <string.h>

static lv_obj_t *s_type_label[SIGNAL_JOINT_COMPONENT_COUNT];
static lv_obj_t *s_frequency_label[SIGNAL_JOINT_COMPONENT_COUNT];
static lv_obj_t *s_amplitude_label[SIGNAL_JOINT_COMPONENT_COUNT];
static lv_obj_t *s_votes_label[SIGNAL_JOINT_COMPONENT_COUNT];
static lv_obj_t *s_valid_frames_label;
static lv_obj_t *s_confidence_label;
static lv_obj_t *s_residual_label;
static lv_obj_t *s_session_time_label;
static lv_obj_t *s_model_diagnostic_label;
static lv_obj_t *s_error_detail_label;

static bool app_result_is_single_candidate_diagnostic(
    const AppSignalResultViewData *data)
{
    uint32_t model_mask = 1UL << SIGNAL_DETECT_ERR_MODEL_AMBIGUOUS;
    return !data->vote_valid
        && ((data->reason_mask & model_mask) != 0U)
        && (data->candidate_count == 1U)
        && (data->wave_types[0] != SIGNAL_WAVE_UNKNOWN)
        && (data->component_frequency_hz[0] > 0U);
}

static lv_obj_t *app_result_find_named(lv_obj_t *parent, const char *name)
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
        lv_obj_t *result = app_result_find_named(
            lv_obj_get_child(parent, (int32_t)index), name);
        if (result != NULL)
        {
            return result;
        }
    }
    return NULL;
}

void AppSignalResultView_Bind(lv_obj_t *screen)
{
    s_type_label[0] = app_result_find_named(screen, "ui_signal_a_type");
    s_type_label[1] = app_result_find_named(screen, "ui_signal_b_type");
    s_frequency_label[0] = app_result_find_named(
        screen, "ui_signal_a_frequency");
    s_frequency_label[1] = app_result_find_named(
        screen, "ui_signal_b_frequency");
    s_amplitude_label[0] = app_result_find_named(
        screen, "ui_signal_a_amplitude");
    s_amplitude_label[1] = app_result_find_named(
        screen, "ui_signal_b_amplitude");
    s_votes_label[0] = app_result_find_named(screen, "ui_signal_a_votes");
    s_votes_label[1] = app_result_find_named(screen, "ui_signal_b_votes");
    s_valid_frames_label = app_result_find_named(screen, "ui_valid_frames");
    s_confidence_label = app_result_find_named(screen, "ui_confidence");
    s_residual_label = app_result_find_named(screen, "ui_residual");
    s_session_time_label = app_result_find_named(screen, "ui_session_time");
    s_model_diagnostic_label = app_result_find_named(
        screen, "ui_model_diagnostic");
    s_error_detail_label = app_result_find_named(screen, "ui_error_detail");
}

static void app_result_render_components(const AppSignalResultViewData *data)
{
    char buffer[48];
    bool single_candidate = app_result_is_single_candidate_diagnostic(data);

    for (uint32_t index = 0U;
         index < SIGNAL_JOINT_COMPONENT_COUNT;
         ++index)
    {
        bool show_diagnostic = single_candidate && (index == 0U);
        bool show_component = data->vote_valid || show_diagnostic;

        if (s_type_label[index] != NULL)
        {
            if (show_diagnostic)
            {
                lv_snprintf(buffer, sizeof(buffer), "%s (E5)",
                            SignalHarmonic_WaveTypeString(
                                data->wave_types[index]));
                lv_label_set_text(s_type_label[index], buffer);
            }
            else
            {
                lv_label_set_text(
                    s_type_label[index],
                    data->vote_valid
                        ? SignalHarmonic_WaveTypeString(
                            data->wave_types[index])
                        : "UNKNOWN");
            }
        }

        if (s_frequency_label[index] != NULL)
        {
            if (show_component)
            {
                uint32_t frequency_hz = data->component_frequency_hz[index];
                lv_snprintf(buffer, sizeof(buffer), "%lu.%03lu",
                            (unsigned long)(frequency_hz / 1000U),
                            (unsigned long)(frequency_hz % 1000U));
            }
            else
            {
                lv_snprintf(buffer, sizeof(buffer), "--");
            }
            lv_label_set_text(s_frequency_label[index], buffer);
        }

        if (s_amplitude_label[index] != NULL)
        {
            if (show_component)
            {
                uint32_t amplitude_q100 =
                    data->component_amplitude_q100[index];
                lv_snprintf(buffer, sizeof(buffer), "%lu.%02lu",
                            (unsigned long)(amplitude_q100 / 100U),
                            (unsigned long)(amplitude_q100 % 100U));
            }
            else
            {
                lv_snprintf(buffer, sizeof(buffer), "--");
            }
            lv_label_set_text(s_amplitude_label[index], buffer);
        }

        if (s_votes_label[index] != NULL)
        {
            if (show_diagnostic)
            {
                lv_snprintf(buffer, sizeof(buffer), "E5 last frame");
            }
            else
            {
                lv_snprintf(
                    buffer, sizeof(buffer), "F %lu/%lu  T %lu/%lu",
                    (unsigned long)data->component_frequency_votes[index],
                    (unsigned long)data->valid_frames,
                    (unsigned long)data->component_type_votes[index],
                    (unsigned long)data->valid_frames);
            }
            lv_label_set_text(s_votes_label[index], buffer);
        }
    }
}

static void app_result_render_quality(const AppSignalResultViewData *data)
{
    char buffer[48];

    if (s_valid_frames_label != NULL)
    {
        lv_snprintf(buffer, sizeof(buffer), "%lu/%lu",
                    (unsigned long)data->valid_frames,
                    (unsigned long)data->requested_frames);
        lv_label_set_text(s_valid_frames_label, buffer);
    }
    if (s_confidence_label != NULL)
    {
        lv_snprintf(buffer, sizeof(buffer), "%lu.%03lu",
                    (unsigned long)(data->confidence_q1000 / 1000U),
                    (unsigned long)(data->confidence_q1000 % 1000U));
        lv_label_set_text(s_confidence_label, buffer);
    }
    if (s_residual_label != NULL)
    {
        lv_snprintf(buffer, sizeof(buffer), "%lu.%05lu",
                    (unsigned long)(data->residual_q100000 / 100000U),
                    (unsigned long)(data->residual_q100000 % 100000U));
        lv_label_set_text(s_residual_label, buffer);
    }
    if (s_session_time_label != NULL)
    {
        lv_snprintf(buffer, sizeof(buffer), "%lu ms",
                    (unsigned long)data->session_time_ms);
        lv_label_set_text(s_session_time_label, buffer);
    }
    if (s_model_diagnostic_label != NULL)
    {
        lv_snprintf(buffer, sizeof(buffer), "Cand %lu  Pair %lu  Model %lu",
                    (unsigned long)data->candidate_count,
                    (unsigned long)data->evaluated_pairs,
                    (unsigned long)data->evaluated_models);
        lv_label_set_text(s_model_diagnostic_label, buffer);
    }
}

static void app_result_render_error(const AppSignalResultViewData *data)
{
    if (s_error_detail_label == NULL)
    {
        return;
    }

    char buffer[64];
    uint32_t model_mask = 1UL << SIGNAL_DETECT_ERR_MODEL_AMBIGUOUS;
    uint32_t low_mask = 1UL << SIGNAL_DETECT_ERR_LOW_AMPLITUDE;
    uint32_t clip_mask = 1UL << SIGNAL_DETECT_ERR_CLIPPING;
    uint32_t timeout_mask = 1UL << SIGNAL_DETECT_ERR_CAPTURE_TIMEOUT;
    uint32_t color = 0xff7373;
    bool single_candidate = app_result_is_single_candidate_diagnostic(data);

    if (data->vote_valid)
    {
        lv_snprintf(buffer, sizeof(buffer), "Result valid - A < B");
        color = 0x72eaa5;
    }
    else if (((data->reason_mask & model_mask) != 0U)
             && (data->candidate_count < SIGNAL_JOINT_COMPONENT_COUNT))
    {
        if (single_candidate)
        {
            uint32_t frequency_hz = data->component_frequency_hz[0];
            lv_snprintf(
                buffer, sizeof(buffer),
                "Detected %s %lu.%03lu kHz - second input missing",
                SignalHarmonic_WaveTypeString(data->wave_types[0]),
                (unsigned long)(frequency_hz / 1000U),
                (unsigned long)(frequency_hz % 1000U));
        }
        else
        {
            lv_snprintf(buffer, sizeof(buffer),
                        "Only %lu FFT candidates - check both inputs",
                        (unsigned long)data->candidate_count);
        }
        color = 0xffb454;
    }
    else if ((data->reason_mask & model_mask) != 0U)
    {
        lv_snprintf(buffer, sizeof(buffer),
                    "Model uncertain - check type or level");
        color = 0xffb454;
    }
    else if ((data->reason_mask & low_mask) != 0U)
    {
        lv_snprintf(buffer, sizeof(buffer),
                    "Amplitude too low - check input level");
        color = 0xffb454;
    }
    else if ((data->reason_mask & clip_mask) != 0U)
    {
        lv_snprintf(buffer, sizeof(buffer),
                    "Signal clipped - reduce input level");
    }
    else if ((data->reason_mask & timeout_mask) != 0U)
    {
        lv_snprintf(buffer, sizeof(buffer),
                    "Acquisition timeout - check AD9226 clock");
    }
    else
    {
        lv_snprintf(buffer, sizeof(buffer), "Error: %s",
                    SignalVote_ErrorString(data->vote_error));
    }

    lv_obj_set_style_text_color(s_error_detail_label,
                                lv_color_hex(color), 0);
    lv_label_set_text(s_error_detail_label, buffer);
}

void AppSignalResultView_Render(const AppSignalResultViewData *data)
{
    if ((data == NULL) || !data->vote_present)
    {
        return;
    }

    app_result_render_components(data);
    app_result_render_quality(data);
    app_result_render_error(data);
}

void AppSignalResultView_RefreshPhaseLock(void)
{
    static bool was_running = false;
    bool running = g_app_phase_lock_status.running;
    if (!running)
    {
        if (was_running && (s_error_detail_label != NULL))
        {
            lv_obj_set_style_text_color(s_error_detail_label,
                                        lv_color_hex(0xffc857), 0);
            lv_label_set_text(s_error_detail_label, "Output stopped");
        }
        was_running = false;
        return;
    }

    char buffer[48];
    for (uint32_t channel = 0U;
         channel < SIGNAL_JOINT_COMPONENT_COUNT;
         ++channel)
    {
        float tracked_frequency_hz =
            g_app_phase_lock_status.channels[channel].tracked_frequency_hz;
        float phase_error_rad =
            g_app_phase_lock_status.channels[channel].phase_error_rad;
        bool locked = g_app_phase_lock_status.channels[channel].locked;
        uint32_t frequency_hz = (uint32_t)(tracked_frequency_hz + 0.5f);
        int32_t phase_error_mdeg = (int32_t)(phase_error_rad
                                            * 57295.7795131f);

        if (s_frequency_label[channel] != NULL)
        {
            lv_snprintf(buffer, sizeof(buffer), "%lu.%03lu",
                        (unsigned long)(frequency_hz / 1000U),
                        (unsigned long)(frequency_hz % 1000U));
            lv_label_set_text(s_frequency_label[channel], buffer);
        }
        if (s_votes_label[channel] != NULL)
        {
            long phase_error_abs_mdeg = labs(phase_error_mdeg);
            lv_snprintf(buffer, sizeof(buffer), "%s E=%s%ld.%01ld",
                        locked ? "LOCK" : "TRK",
                        (phase_error_mdeg < 0) ? "-" : "",
                        phase_error_abs_mdeg / 1000L,
                        (phase_error_abs_mdeg % 1000L) / 100L);
            lv_label_set_text(s_votes_label[channel], buffer);
        }
    }

    if (s_error_detail_label != NULL)
    {
        bool all_locked = g_app_phase_lock_status.all_locked;
        lv_obj_set_style_text_color(
            s_error_detail_label,
            lv_color_hex(all_locked ? 0x72eaa5 : 0xffc857), 0);
        if (all_locked && g_app_dac_output_status.phase_control_active)
        {
            lv_snprintf(buffer, sizeof(buffer), "Locked B'-A' %u°",
                        (unsigned int)
                            g_app_dac_output_status.phase_difference_deg);
            lv_label_set_text(s_error_detail_label, buffer);
        }
        else
        {
            lv_label_set_text(s_error_detail_label,
                              all_locked
                                  ? "Output phase locked"
                                  : "Output tracking");
        }
    }
    was_running = true;
}
