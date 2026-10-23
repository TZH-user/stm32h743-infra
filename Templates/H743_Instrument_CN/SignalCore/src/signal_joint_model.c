/**
  ******************************************************************************
  * @file    signal_joint_model.c
  * @brief   E6 双分量正弦/三角联合模型与残差选择（UTF-8）。
  ******************************************************************************
  */

#include "signal_joint_model.h"

#include "app_analysis_config.h"
#include "app_trig_lookup.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define SIGNAL_JOINT_PI_F                 3.14159265358979323846f
#define SIGNAL_JOINT_MAX_HARMONICS        4U
#define SIGNAL_JOINT_REFINEMENT_PASSES    2U

typedef struct
{
    float cosine[SIGNAL_JOINT_MAX_HARMONICS];
    float sine[SIGNAL_JOINT_MAX_HARMONICS];
    float cosine_step[SIGNAL_JOINT_MAX_HARMONICS];
    float sine_step[SIGNAL_JOINT_MAX_HARMONICS];
    float ratio[SIGNAL_JOINT_MAX_HARMONICS];
    float amplitude;
    uint32_t count;
} SignalJointOscillator;

volatile SignalJointResult g_signal_joint_result;

/* E6 由 MainTask 单次串行调用，静态工作区避免占用任务栈。 */
static SignalJointResult s_joint_best;
static SignalJointOscillator s_joint_oscillator_a;
static SignalJointOscillator s_joint_oscillator_b;

static float signal_joint_clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

/*
 * 三角波采用 1/3/5/7 次有限奇次谐波模板。相位按 harmonic*phase
 * 递推，1/h^2 比例中的交替方向自然包含在相位关系中。
 */
static bool signal_joint_oscillator_init(
    SignalJointOscillator *oscillator,
    const SignalJointComponent *component,
    uint32_t sample_rate_hz)
{
    static const uint32_t harmonics[SIGNAL_JOINT_MAX_HARMONICS] = {
        1U, 3U, 5U, 7U,
    };

    if ((oscillator == NULL) || (component == NULL)
        || (sample_rate_hz == 0U)
        || !isfinite(component->frequency_hz)
        || !isfinite(component->amplitude_code)
        || !isfinite(component->phase_rad)
        || (component->frequency_hz <= 0.0f))
    {
        return false;
    }

    memset(oscillator, 0, sizeof(*oscillator));
    oscillator->amplitude = component->amplitude_code;
    uint32_t requested_count = (component->type == SIGNAL_WAVE_TRIANGLE)
        ? SIGNAL_JOINT_MAX_HARMONICS : 1U;
    float nyquist_hz = 0.5f * (float)sample_rate_hz;

    for (uint32_t index = 0U; index < requested_count; ++index)
    {
        uint32_t harmonic = harmonics[index];
        float harmonic_frequency = component->frequency_hz
            * (float)harmonic;
        if (harmonic_frequency >= nyquist_hz)
        {
            continue;
        }

        uint32_t slot = oscillator->count;
        float phase = component->phase_rad * (float)harmonic;
        float step = 2.0f * SIGNAL_JOINT_PI_F * harmonic_frequency
            / (float)sample_rate_hz;
        AppTrigLookup_SinCosRadians(phase,
                                    &oscillator->sine[slot],
                                    &oscillator->cosine[slot]);
        AppTrigLookup_SinCosRadians(step,
                                    &oscillator->sine_step[slot],
                                    &oscillator->cosine_step[slot]);
        oscillator->ratio[slot] = 1.0f
            / ((float)harmonic * (float)harmonic);
        ++oscillator->count;
    }

    return oscillator->count > 0U;
}

static float signal_joint_oscillator_next(
    SignalJointOscillator *oscillator)
{
    float value = 0.0f;
    for (uint32_t index = 0U; index < oscillator->count; ++index)
    {
        value += oscillator->ratio[index] * oscillator->cosine[index];
        float next_cosine = oscillator->cosine[index]
                * oscillator->cosine_step[index]
            - oscillator->sine[index] * oscillator->sine_step[index];
        float next_sine = oscillator->sine[index]
                * oscillator->cosine_step[index]
            + oscillator->cosine[index] * oscillator->sine_step[index];
        oscillator->cosine[index] = next_cosine;
        oscillator->sine[index] = next_sine;
    }
    return value * oscillator->amplitude;
}

/* 在扣除另一分量模型后，以复数相关估计当前基波幅值和相位。 */
static bool signal_joint_estimate_component(
    const float *samples,
    uint32_t sample_count,
    uint32_t sample_rate_hz,
    const SignalJointComponent *other,
    SignalJointComponent *component)
{
    if ((samples == NULL) || (component == NULL)
        || (sample_count == 0U) || (sample_rate_hz == 0U))
    {
        return false;
    }

    bool subtract_other = (other != NULL)
        && (other->amplitude_code > 0.0f)
        && signal_joint_oscillator_init(&s_joint_oscillator_b,
                                        other,
                                        sample_rate_hz);
    float step = 2.0f * SIGNAL_JOINT_PI_F * component->frequency_hz
        / (float)sample_rate_hz;
    float cosine = 1.0f;
    float sine = 0.0f;
    float cosine_step;
    float sine_step;
    AppTrigLookup_SinCosRadians(step, &sine_step, &cosine_step);
    float i_sum = 0.0f;
    float q_sum = 0.0f;

    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        float sample = samples[index];
        if (subtract_other)
        {
            sample -= signal_joint_oscillator_next(&s_joint_oscillator_b);
        }
        i_sum += sample * cosine;
        q_sum -= sample * sine;

        float next_cosine = cosine * cosine_step - sine * sine_step;
        float next_sine = sine * cosine_step + cosine * sine_step;
        cosine = next_cosine;
        sine = next_sine;
    }

    float scale = 2.0f / (float)sample_count;
    float i_scaled = i_sum * scale;
    float q_scaled = q_sum * scale;
    component->amplitude_code = sqrtf(i_scaled * i_scaled
                                      + q_scaled * q_scaled);
    component->phase_rad = atan2f(q_scaled, i_scaled);
    return isfinite(component->amplitude_code)
        && isfinite(component->phase_rad);
}

static bool signal_joint_measure_residual(
    const float *samples,
    uint32_t sample_count,
    uint32_t sample_rate_hz,
    const SignalJointComponent *first,
    const SignalJointComponent *second,
    SignalJointResult *candidate)
{
    if (!signal_joint_oscillator_init(&s_joint_oscillator_a,
                                      first,
                                      sample_rate_hz)
        || !signal_joint_oscillator_init(&s_joint_oscillator_b,
                                         second,
                                         sample_rate_hz))
    {
        return false;
    }

    float input_sum = 0.0f;
    float model_sum = 0.0f;
    float residual_sum = 0.0f;
    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        float model = signal_joint_oscillator_next(&s_joint_oscillator_a)
            + signal_joint_oscillator_next(&s_joint_oscillator_b);
        float error = samples[index] - model;
        input_sum += samples[index] * samples[index];
        model_sum += model * model;
        residual_sum += error * error;
    }

    float divisor = (float)sample_count;
    candidate->input_rms_code = sqrtf(input_sum / divisor);
    candidate->model_rms_code = sqrtf(model_sum / divisor);
    candidate->residual_rms_code = sqrtf(residual_sum / divisor);
    if (candidate->input_rms_code <= 0.0f)
    {
        return false;
    }
    candidate->best_residual = candidate->residual_rms_code
        / candidate->input_rms_code;
    return isfinite(candidate->best_residual);
}

static bool signal_joint_evaluate_model(
    const float *samples,
    uint32_t sample_count,
    uint32_t sample_rate_hz,
    const SignalFrequencyCandidate *first_candidate,
    const SignalFrequencyCandidate *second_candidate,
    SignalWaveType first_type,
    SignalWaveType second_type,
    SignalJointResult *candidate)
{
    memset(candidate, 0, sizeof(*candidate));
    candidate->sample_rate_hz = sample_rate_hz;
    candidate->sample_count = sample_count;

    const SignalFrequencyCandidate *low = first_candidate;
    const SignalFrequencyCandidate *high = second_candidate;
    SignalWaveType low_type = first_type;
    SignalWaveType high_type = second_type;
    if (low->nominal_frequency_hz > high->nominal_frequency_hz)
    {
        low = second_candidate;
        high = first_candidate;
        low_type = second_type;
        high_type = first_type;
    }

    SignalJointComponent *first = &candidate->components[0];
    SignalJointComponent *second = &candidate->components[1];
    first->frequency_hz = low->nominal_frequency_hz;
    first->type = low_type;
    first->source_candidate_rank = low->rank;
    second->frequency_hz = high->nominal_frequency_hz;
    second->type = high_type;
    second->source_candidate_rank = high->rank;

    if (!signal_joint_estimate_component(samples,
                                         sample_count,
                                         sample_rate_hz,
                                         NULL,
                                         first)
        || !signal_joint_estimate_component(samples,
                                             sample_count,
                                             sample_rate_hz,
                                             first,
                                             second))
    {
        return false;
    }

    for (uint32_t pass = 0U;
         pass < SIGNAL_JOINT_REFINEMENT_PASSES;
         ++pass)
    {
        if (!signal_joint_estimate_component(samples,
                                             sample_count,
                                             sample_rate_hz,
                                             second,
                                             first)
            || !signal_joint_estimate_component(samples,
                                                 sample_count,
                                                 sample_rate_hz,
                                                 first,
                                                 second))
        {
            return false;
        }
    }

    if ((first->amplitude_code < APP_ANALYSIS_DEFAULT_MIN_AMPLITUDE_CODE)
        || (second->amplitude_code
            < APP_ANALYSIS_DEFAULT_MIN_AMPLITUDE_CODE))
    {
        return false;
    }

    return signal_joint_measure_residual(samples,
                                         sample_count,
                                         sample_rate_hz,
                                         first,
                                         second,
                                         candidate);
}

void SignalJoint_ResetResult(void)
{
    memset((void *)&g_signal_joint_result,
           0,
           sizeof(g_signal_joint_result));
}

bool SignalJoint_Analyze(const float *unwindowed_samples,
                         uint32_t sample_count,
                         const SignalFftResult *fft_result,
                         SignalJointResult *result)
{
    if (result == NULL)
    {
        return false;
    }

    memset(result, 0, sizeof(*result));
    if ((unwindowed_samples == NULL) || (fft_result == NULL)
        || !fft_result->valid || (fft_result->sample_rate_hz == 0U)
        || (sample_count != fft_result->fft_size)
        || (fft_result->selected_candidate_count
            < SIGNAL_JOINT_COMPONENT_COUNT))
    {
        return false;
    }

    memset(&s_joint_best, 0, sizeof(s_joint_best));
    float best_residual = INFINITY;
    float second_residual = INFINITY;
    uint32_t evaluated_pairs = 0U;
    uint32_t evaluated_models = 0U;
    uint32_t candidate_count = fft_result->selected_candidate_count;
    if (candidate_count > APP_ANALYSIS_MAX_SELECTED_CANDIDATES)
    {
        candidate_count = APP_ANALYSIS_MAX_SELECTED_CANDIDATES;
    }

    for (uint32_t first = 0U; first + 1U < candidate_count; ++first)
    {
        for (uint32_t second = first + 1U;
             second < candidate_count;
             ++second)
        {
            ++evaluated_pairs;
            for (uint32_t model = 0U; model < 4U; ++model)
            {
                SignalWaveType first_type = ((model & 2U) != 0U)
                    ? SIGNAL_WAVE_TRIANGLE : SIGNAL_WAVE_SINE;
                SignalWaveType second_type = ((model & 1U) != 0U)
                    ? SIGNAL_WAVE_TRIANGLE : SIGNAL_WAVE_SINE;
                SignalJointResult candidate;
                if (!signal_joint_evaluate_model(
                        unwindowed_samples,
                        sample_count,
                        fft_result->sample_rate_hz,
                        &fft_result->selected[first],
                        &fft_result->selected[second],
                        first_type,
                        second_type,
                        &candidate))
                {
                    continue;
                }

                ++evaluated_models;
                float residual = candidate.best_residual;
                if (residual < best_residual)
                {
                    second_residual = best_residual;
                    best_residual = residual;
                    s_joint_best = candidate;
                }
                else if (residual < second_residual)
                {
                    second_residual = residual;
                }
            }
        }
    }

    s_joint_best.evaluated_pairs = evaluated_pairs;
    s_joint_best.evaluated_models = evaluated_models;
    if (!isfinite(best_residual))
    {
        s_joint_best.best_residual = 0.0f;
        s_joint_best.second_residual = 0.0f;
        s_joint_best.residual_gap = 0.0f;
        s_joint_best.confidence = 0.0f;
        s_joint_best.valid = false;
        *result = s_joint_best;
        return false;
    }
    if (!isfinite(second_residual))
    {
        second_residual = best_residual;
    }
    s_joint_best.second_residual = second_residual;
    if (isfinite(best_residual) && isfinite(second_residual)
        && (second_residual >= best_residual))
    {
        s_joint_best.residual_gap = second_residual - best_residual;
    }

    float residual_score = 1.0f - signal_joint_clamp01(
        best_residual / APP_ANALYSIS_JOINT_MAX_RESIDUAL);
    float gap_score = s_joint_best.residual_gap
        / (s_joint_best.residual_gap + APP_ANALYSIS_JOINT_GAP_SCALE);
    s_joint_best.confidence = signal_joint_clamp01(
        residual_score * gap_score);
    s_joint_best.valid = isfinite(best_residual)
        && isfinite(second_residual)
        && (best_residual <= APP_ANALYSIS_JOINT_MAX_RESIDUAL)
        && (s_joint_best.confidence
            >= APP_ANALYSIS_JOINT_MIN_CONFIDENCE);

    *result = s_joint_best;
    return result->valid;
}

bool SignalJoint_TrackKnown(
    const float *unwindowed_samples,
    uint32_t sample_count,
    uint32_t sample_rate_hz,
    const SignalJointComponent seed_components[SIGNAL_JOINT_COMPONENT_COUNT],
    SignalJointResult *result)
{
    if ((unwindowed_samples == NULL) || (seed_components == NULL)
        || (result == NULL) || (sample_count == 0U)
        || (sample_rate_hz == 0U))
    {
        return false;
    }

    float cosine[SIGNAL_JOINT_COMPONENT_COUNT];
    float sine[SIGNAL_JOINT_COMPONENT_COUNT] = {0};
    float cosine_step[SIGNAL_JOINT_COMPONENT_COUNT];
    float sine_step[SIGNAL_JOINT_COMPONENT_COUNT];
    float i_sum[SIGNAL_JOINT_COMPONENT_COUNT] = {0};
    float q_sum[SIGNAL_JOINT_COMPONENT_COUNT] = {0};
    SignalJointResult tracked = {0};
    tracked.sample_rate_hz = sample_rate_hz;
    tracked.sample_count = sample_count;
    tracked.evaluated_pairs = 1U;
    tracked.evaluated_models = 1U;

    for (uint32_t component = 0U;
         component < SIGNAL_JOINT_COMPONENT_COUNT;
         ++component)
    {
        if (!isfinite(seed_components[component].frequency_hz)
            || (seed_components[component].frequency_hz <= 0.0f)
            || ((seed_components[component].type != SIGNAL_WAVE_SINE)
                && (seed_components[component].type
                    != SIGNAL_WAVE_TRIANGLE)))
        {
            return false;
        }

        tracked.components[component].frequency_hz =
            seed_components[component].frequency_hz;
        tracked.components[component].type =
            seed_components[component].type;
        tracked.components[component].source_candidate_rank = component + 1U;
        float step = 2.0f * SIGNAL_JOINT_PI_F
            * seed_components[component].frequency_hz
            / (float)sample_rate_hz;
        cosine[component] = 1.0f;
        AppTrigLookup_SinCosRadians(step,
                                    &sine_step[component],
                                    &cosine_step[component]);
    }

    float input_sum = 0.0f;
    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        float sample = unwindowed_samples[index];
        input_sum += sample * sample;
        for (uint32_t component = 0U;
             component < SIGNAL_JOINT_COMPONENT_COUNT;
             ++component)
        {
            i_sum[component] += sample * cosine[component];
            q_sum[component] -= sample * sine[component];
            float next_cosine = cosine[component]
                    * cosine_step[component]
                - sine[component] * sine_step[component];
            float next_sine = sine[component]
                    * cosine_step[component]
                + cosine[component] * sine_step[component];
            cosine[component] = next_cosine;
            sine[component] = next_sine;
        }
    }

    float scale = 2.0f / (float)sample_count;
    float captured_rms_squared = 0.0f;
    for (uint32_t component = 0U;
         component < SIGNAL_JOINT_COMPONENT_COUNT;
         ++component)
    {
        float i_scaled = i_sum[component] * scale;
        float q_scaled = q_sum[component] * scale;
        float amplitude = sqrtf(i_scaled * i_scaled
                                + q_scaled * q_scaled);
        tracked.components[component].amplitude_code = amplitude;
        tracked.components[component].phase_rad =
            atan2f(q_scaled, i_scaled);
        captured_rms_squared += 0.5f * amplitude * amplitude;
        if (!isfinite(amplitude)
            || !isfinite(tracked.components[component].phase_rad)
            || (amplitude < APP_ANALYSIS_DEFAULT_MIN_AMPLITUDE_CODE))
        {
            *result = tracked;
            return false;
        }
    }

    float input_rms_squared = input_sum / (float)sample_count;
    tracked.input_rms_code = sqrtf(input_rms_squared);
    tracked.model_rms_code = sqrtf(captured_rms_squared);
    float residual_rms_squared = input_rms_squared - captured_rms_squared;
    if (residual_rms_squared < 0.0f)
    {
        residual_rms_squared = 0.0f;
    }
    tracked.residual_rms_code = sqrtf(residual_rms_squared);
    tracked.best_residual = (tracked.input_rms_code > 0.0f)
        ? (tracked.residual_rms_code / tracked.input_rms_code)
        : 1.0f;
    tracked.second_residual = tracked.best_residual;
    tracked.residual_gap = 0.0f;
    tracked.confidence = 1.0f - signal_joint_clamp01(
        tracked.best_residual / APP_ANALYSIS_JOINT_MAX_RESIDUAL);
    tracked.valid = isfinite(tracked.best_residual);
    *result = tracked;
    return result->valid;
}

void SignalJoint_PublishResult(const SignalJointResult *result,
                               uint32_t processing_time_us)
{
    if (result == NULL)
    {
        SignalJoint_ResetResult();
        return;
    }

    memcpy((void *)&g_signal_joint_result,
           result,
           sizeof(g_signal_joint_result));
    g_signal_joint_result.processing_time_us = processing_time_us;
}
