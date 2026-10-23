/**
  ******************************************************************************
  * @file    signal_vote.c
  * @brief   E7 八帧众数/中位数投票，不包含采集或任务调度（UTF-8）。
  ******************************************************************************
  */

#include "signal_vote.h"

#include "signal_detect_cfg.h"

#include <math.h>
#include <string.h>

_Static_assert(SIGNAL_ANALYSIS_FRAME_COUNT <= 32U,
               "E7 valid frame mask supports at most 32 frames");
_Static_assert(SIGNAL_MIN_VALID_FRAME_COUNT <= SIGNAL_ANALYSIS_FRAME_COUNT,
               "E7 minimum valid frames exceeds session frame count");

typedef struct
{
    SignalJointResult frames[SIGNAL_ANALYSIS_FRAME_COUNT];
    uint32_t valid_count;
    uint32_t processed_count;
    uint32_t valid_mask;
    uint32_t invalid_reason_mask;
    uint32_t analysis_time_us;
    SignalDetectError last_error;
    SignalDetectState state;
} SignalVoteContext;

static SignalVoteContext s_vote;
volatile SignalVoteResult g_signal_vote_result;
volatile SignalVoteDiagnostics g_signal_vote_diagnostics;

static void signal_vote_record_diagnostics(const SignalVoteResult *result)
{
    SignalVoteDiagnostics diagnostics = g_signal_vote_diagnostics;
    ++diagnostics.session_count;
    if (result->valid)
    {
        ++diagnostics.valid_session_count;
    }
    else
    {
        ++diagnostics.invalid_session_count;
    }

    bool inconsistent = false;
    if (diagnostics.session_count == 1U)
    {
        diagnostics.minimum_valid_frames = result->valid_frames;
        diagnostics.minimum_confidence = result->total_confidence;
        diagnostics.maximum_confidence = result->total_confidence;
        for (uint32_t index = 0U;
             index < SIGNAL_JOINT_COMPONENT_COUNT;
             ++index)
        {
            diagnostics.reference_frequency_hz[index] = (uint32_t)(
                result->components[index].frequency_hz + 0.5f);
            diagnostics.reference_type[index] = result->components[index].type;
        }
    }
    else
    {
        for (uint32_t index = 0U;
             index < SIGNAL_JOINT_COMPONENT_COUNT;
             ++index)
        {
            uint32_t frequency_hz = (uint32_t)(
                result->components[index].frequency_hz + 0.5f);
            if ((frequency_hz
                 != diagnostics.reference_frequency_hz[index])
                || (result->components[index].type
                    != diagnostics.reference_type[index]))
            {
                inconsistent = true;
            }
        }
        if (result->valid_frames < diagnostics.minimum_valid_frames)
        {
            diagnostics.minimum_valid_frames = result->valid_frames;
        }
        if (result->total_confidence < diagnostics.minimum_confidence)
        {
            diagnostics.minimum_confidence = result->total_confidence;
        }
        if (result->total_confidence > diagnostics.maximum_confidence)
        {
            diagnostics.maximum_confidence = result->total_confidence;
        }
    }

    if (inconsistent)
    {
        ++diagnostics.inconsistent_session_count;
    }
    if (result->session_time_ms > diagnostics.maximum_session_time_ms)
    {
        diagnostics.maximum_session_time_ms = result->session_time_ms;
    }
    g_signal_vote_diagnostics = diagnostics;
}

static void signal_vote_sort_float(float *values, uint32_t count)
{
    for (uint32_t index = 1U; index < count; ++index)
    {
        float value = values[index];
        uint32_t position = index;
        while ((position > 0U) && (values[position - 1U] > value))
        {
            values[position] = values[position - 1U];
            --position;
        }
        values[position] = value;
    }
}

static float signal_vote_median(float *values, uint32_t count)
{
    if ((values == NULL) || (count == 0U))
    {
        return 0.0f;
    }

    signal_vote_sort_float(values, count);
    if ((count & 1U) != 0U)
    {
        return values[count / 2U];
    }
    return 0.5f * (values[(count / 2U) - 1U] + values[count / 2U]);
}

static uint32_t signal_vote_frequency_mode(uint32_t component_index,
                                           uint32_t *votes)
{
    uint32_t best_frequency_hz = 0U;
    uint32_t best_votes = 0U;

    for (uint32_t first = 0U; first < s_vote.valid_count; ++first)
    {
        uint32_t frequency_hz = (uint32_t)(
            s_vote.frames[first].components[component_index].frequency_hz
            + 0.5f);
        uint32_t count = 0U;
        for (uint32_t second = 0U; second < s_vote.valid_count; ++second)
        {
            uint32_t compared_hz = (uint32_t)(
                s_vote.frames[second].components[component_index].frequency_hz
                + 0.5f);
            if (compared_hz == frequency_hz)
            {
                ++count;
            }
        }

        if ((count > best_votes)
            || ((count == best_votes) && (frequency_hz < best_frequency_hz)))
        {
            best_frequency_hz = frequency_hz;
            best_votes = count;
        }
    }

    *votes = best_votes;
    return best_frequency_hz;
}

static SignalWaveType signal_vote_type_mode(uint32_t component_index,
                                            uint32_t *votes)
{
    uint32_t sine_votes = 0U;
    uint32_t triangle_votes = 0U;
    for (uint32_t index = 0U; index < s_vote.valid_count; ++index)
    {
        SignalWaveType type =
            s_vote.frames[index].components[component_index].type;
        if (type == SIGNAL_WAVE_SINE)
        {
            ++sine_votes;
        }
        else if (type == SIGNAL_WAVE_TRIANGLE)
        {
            ++triangle_votes;
        }
    }

    if (triangle_votes > sine_votes)
    {
        *votes = triangle_votes;
        return SIGNAL_WAVE_TRIANGLE;
    }
    if (sine_votes > triangle_votes)
    {
        *votes = sine_votes;
        return SIGNAL_WAVE_SINE;
    }

    *votes = sine_votes;
    return SIGNAL_WAVE_UNKNOWN;
}

void SignalVote_Begin(void)
{
    memset(&s_vote, 0, sizeof(s_vote));
    s_vote.state = SIGNAL_DETECT_ARMED;

    SignalVoteResult reset = {0};
    reset.state = SIGNAL_DETECT_ARMED;
    reset.requested_frames = SIGNAL_ANALYSIS_FRAME_COUNT;
    g_signal_vote_result = reset;
}

void SignalVote_SetState(SignalDetectState state)
{
    s_vote.state = state;
    g_signal_vote_result.state = state;
}

void SignalVote_AddFrame(const SignalJointResult *result,
                         uint32_t analysis_time_us)
{
    uint32_t frame_index = s_vote.processed_count;
    ++s_vote.processed_count;
    s_vote.analysis_time_us += analysis_time_us;

    if ((result == NULL) || !result->valid
        || (s_vote.valid_count >= SIGNAL_ANALYSIS_FRAME_COUNT))
    {
        s_vote.last_error = SIGNAL_DETECT_ERR_MODEL_AMBIGUOUS;
        s_vote.invalid_reason_mask |=
            (1UL << SIGNAL_DETECT_ERR_MODEL_AMBIGUOUS);
        return;
    }

    for (uint32_t component = 0U;
         component < SIGNAL_JOINT_COMPONENT_COUNT;
         ++component)
    {
        const SignalJointComponent *item = &result->components[component];
        if (!isfinite(item->frequency_hz)
            || !isfinite(item->amplitude_code)
            || !isfinite(item->phase_rad))
        {
            s_vote.last_error = SIGNAL_DETECT_ERR_INTERNAL;
            s_vote.invalid_reason_mask |=
                (1UL << SIGNAL_DETECT_ERR_INTERNAL);
            return;
        }
    }

    s_vote.frames[s_vote.valid_count] = *result;
    ++s_vote.valid_count;
    if (frame_index < 32U)
    {
        s_vote.valid_mask |= (1UL << frame_index);
    }
}

void SignalVote_AddInvalidFrame(SignalDetectError error)
{
    ++s_vote.processed_count;
    s_vote.last_error = error;
    if ((uint32_t)error < 32U)
    {
        s_vote.invalid_reason_mask |= (1UL << (uint32_t)error);
    }
}

bool SignalVote_Finalize(uint32_t session_time_ms,
                         uint32_t busy_start_requests,
                         uint32_t stop_requests,
                         SignalVoteResult *result)
{
    if (result == NULL)
    {
        return false;
    }

    SignalVote_SetState(SIGNAL_DETECT_FINALIZE);
    SignalVoteResult final = {0};
    final.state = SIGNAL_DETECT_FINALIZE;
    final.requested_frames = SIGNAL_ANALYSIS_FRAME_COUNT;
    final.processed_frames = s_vote.processed_count;
    final.valid_frames = s_vote.valid_count;
    final.invalid_frames = s_vote.processed_count - s_vote.valid_count;
    final.valid_frame_mask = s_vote.valid_mask;
    final.invalid_reason_mask = s_vote.invalid_reason_mask;
    final.busy_start_requests = busy_start_requests;
    final.stop_requests = stop_requests;
    final.analysis_time_us = s_vote.analysis_time_us;
    final.session_time_ms = session_time_ms;

    if (stop_requests > 0U)
    {
        final.error = SIGNAL_DETECT_ERR_STOPPED;
    }
    else if (s_vote.valid_count < SIGNAL_MIN_VALID_FRAME_COUNT)
    {
        final.error = SIGNAL_DETECT_ERR_NOT_ENOUGH_VALID_FRAMES;
    }
    else
    {
        float residuals[SIGNAL_ANALYSIS_FRAME_COUNT];
        float model_confidences[SIGNAL_ANALYSIS_FRAME_COUNT];
        for (uint32_t index = 0U; index < s_vote.valid_count; ++index)
        {
            residuals[index] = s_vote.frames[index].best_residual;
            model_confidences[index] = s_vote.frames[index].confidence;
        }
        final.median_residual = signal_vote_median(
            residuals, s_vote.valid_count);
        final.median_model_confidence = signal_vote_median(
            model_confidences, s_vote.valid_count);
        final.sample_rate_hz = s_vote.frames[0].sample_rate_hz;

        uint32_t representative = 0U;
        float nearest_residual = INFINITY;
        float minimum_consistency = 1.0f;
        for (uint32_t component = 0U;
             component < SIGNAL_JOINT_COMPONENT_COUNT;
             ++component)
        {
            SignalVoteComponent *voted = &final.components[component];
            voted->frequency_hz = (float)signal_vote_frequency_mode(
                component, &voted->frequency_votes);
            voted->type = signal_vote_type_mode(component,
                                                &voted->type_votes);

            float amplitudes[SIGNAL_ANALYSIS_FRAME_COUNT];
            for (uint32_t index = 0U; index < s_vote.valid_count; ++index)
            {
                amplitudes[index] =
                    s_vote.frames[index].components[component].amplitude_code;
                float distance = fabsf(
                    s_vote.frames[index].best_residual
                    - final.median_residual);
                if (distance < nearest_residual)
                {
                    nearest_residual = distance;
                    representative = index;
                }
            }
            voted->amplitude_code = signal_vote_median(
                amplitudes, s_vote.valid_count);

            float frequency_consistency = (float)voted->frequency_votes
                / (float)s_vote.valid_count;
            float type_consistency = (float)voted->type_votes
                / (float)s_vote.valid_count;
            if (frequency_consistency < minimum_consistency)
            {
                minimum_consistency = frequency_consistency;
            }
            if (type_consistency < minimum_consistency)
            {
                minimum_consistency = type_consistency;
            }

            if ((voted->frequency_votes <= (s_vote.valid_count / 2U))
                || (voted->type_votes <= (s_vote.valid_count / 2U))
                || (voted->type == SIGNAL_WAVE_UNKNOWN))
            {
                final.error = SIGNAL_DETECT_ERR_MODEL_AMBIGUOUS;
            }
        }

        for (uint32_t component = 0U;
             component < SIGNAL_JOINT_COMPONENT_COUNT;
             ++component)
        {
            final.components[component].phase_rad =
                s_vote.frames[representative].components[component].phase_rad;
        }

        float valid_ratio = (float)s_vote.valid_count
            / (float)SIGNAL_ANALYSIS_FRAME_COUNT;
        final.total_confidence = valid_ratio
            * minimum_consistency
            * final.median_model_confidence;
    }

    final.valid = (final.error == SIGNAL_DETECT_ERR_NONE);
    final.state = final.valid ? SIGNAL_DETECT_FINISHED : SIGNAL_DETECT_ERROR;
    *result = final;
    g_signal_vote_result = final;
    signal_vote_record_diagnostics(&final);
    s_vote.state = final.state;
    return final.valid;
}

const char *SignalVote_ErrorString(SignalDetectError error)
{
    switch (error)
    {
        case SIGNAL_DETECT_ERR_NONE:
            return "NONE";
        case SIGNAL_DETECT_ERR_FRAME_SIZE:
            return "FRAME_SIZE";
        case SIGNAL_DETECT_ERR_LOW_AMPLITUDE:
            return "LOW_AMPLITUDE";
        case SIGNAL_DETECT_ERR_CLIPPING:
            return "CLIPPING";
        case SIGNAL_DETECT_ERR_NOT_ENOUGH_VALID_FRAMES:
            return "NOT_ENOUGH_VALID_FRAMES";
        case SIGNAL_DETECT_ERR_MODEL_AMBIGUOUS:
            return "MODEL_AMBIGUOUS";
        case SIGNAL_DETECT_ERR_CAPTURE_TIMEOUT:
            return "CAPTURE_TIMEOUT";
        case SIGNAL_DETECT_ERR_STOPPED:
            return "STOPPED";
        default:
            return "INTERNAL";
    }
}
