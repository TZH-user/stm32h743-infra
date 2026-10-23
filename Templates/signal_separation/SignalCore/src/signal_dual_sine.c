/**
  ******************************************************************************
  * @file    signal_dual_sine.c
  * @brief   E4 双正弦相关估计、模型重构和残差选择（UTF-8）。
  ******************************************************************************
  */

#include "signal_dual_sine.h"

#include "app_analysis_config.h"
#include "app_trig_lookup.h"

#include <math.h>
#include <string.h>

#define SIGNAL_DUAL_SINE_PI_F  3.14159265358979323846f

volatile SignalDualSineResult g_signal_dual_sine_result;

/* E4 只由 MainTask 串行调用，静态工作区避免占用任务栈。 */
static SignalDualSineTone
    s_dual_estimates[APP_ANALYSIS_MAX_SELECTED_CANDIDATES];
static SignalDualSineResult s_dual_best_result;

static float signal_dual_sine_wrap_phase(float phase_rad)
{
    while (phase_rad > SIGNAL_DUAL_SINE_PI_F)
    {
        phase_rad -= 2.0f * SIGNAL_DUAL_SINE_PI_F;
    }
    while (phase_rad < -SIGNAL_DUAL_SINE_PI_F)
    {
        phase_rad += 2.0f * SIGNAL_DUAL_SINE_PI_F;
    }
    return phase_rad;
}

/* 使用未加窗、已去直流的数据计算单个候选频率的复数相关量。 */
static bool signal_dual_sine_estimate(const float *samples,
                                      uint32_t sample_count,
                                      uint32_t sample_rate_hz,
                                      float frequency_hz,
                                      SignalComplexEstimate *estimate)
{
    if ((samples == NULL) || (sample_count == 0U)
        || (sample_rate_hz == 0U) || (estimate == NULL)
        || !isfinite(frequency_hz) || (frequency_hz <= 0.0f))
    {
        return false;
    }

    float i_sum = 0.0f;
    float q_sum = 0.0f;
    float angle_step = (2.0f * SIGNAL_DUAL_SINE_PI_F * frequency_hz)
        / (float)sample_rate_hz;
    float cosine = 1.0f;
    float sine = 0.0f;
    float cosine_step;
    float sine_step;
    AppTrigLookup_SinCosRadians(angle_step, &sine_step, &cosine_step);

    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        float sample = samples[index];
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
    estimate->i = i_sum;
    estimate->q = q_sum;
    estimate->amplitude = sqrtf((i_scaled * i_scaled)
                                + (q_scaled * q_scaled));
    estimate->phase_rad = atan2f(q_scaled, i_scaled);
    return isfinite(estimate->amplitude)
        && isfinite(estimate->phase_rad);
}

/* 按余弦相位约定重构一对候选，并返回归一化 RMS 残差。 */
static bool signal_dual_sine_evaluate_pair(
    const float *samples,
    uint32_t sample_count,
    uint32_t sample_rate_hz,
    const SignalDualSineTone *tone_a,
    const SignalDualSineTone *tone_b,
    float *input_rms,
    float *model_rms,
    float *residual_rms,
    float *normalized_residual)
{
    float input_sum = 0.0f;
    float model_sum = 0.0f;
    float residual_sum = 0.0f;
    float step_a = (2.0f * SIGNAL_DUAL_SINE_PI_F
                    * tone_a->frequency_hz) / (float)sample_rate_hz;
    float step_b = (2.0f * SIGNAL_DUAL_SINE_PI_F
                    * tone_b->frequency_hz) / (float)sample_rate_hz;
    float cosine_a;
    float sine_a;
    float cosine_b;
    float sine_b;
    float cosine_step_a;
    float sine_step_a;
    float cosine_step_b;
    float sine_step_b;
    AppTrigLookup_SinCosRadians(tone_a->estimate.phase_rad,
                                &sine_a,
                                &cosine_a);
    AppTrigLookup_SinCosRadians(tone_b->estimate.phase_rad,
                                &sine_b,
                                &cosine_b);
    AppTrigLookup_SinCosRadians(step_a, &sine_step_a, &cosine_step_a);
    AppTrigLookup_SinCosRadians(step_b, &sine_step_b, &cosine_step_b);

    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        float model = tone_a->estimate.amplitude * cosine_a
            + tone_b->estimate.amplitude * cosine_b;
        float error = samples[index] - model;
        input_sum += samples[index] * samples[index];
        model_sum += model * model;
        residual_sum += error * error;

        float next_cosine_a = cosine_a * cosine_step_a
            - sine_a * sine_step_a;
        float next_sine_a = sine_a * cosine_step_a
            + cosine_a * sine_step_a;
        float next_cosine_b = cosine_b * cosine_step_b
            - sine_b * sine_step_b;
        float next_sine_b = sine_b * cosine_step_b
            + cosine_b * sine_step_b;
        cosine_a = next_cosine_a;
        sine_a = next_sine_a;
        cosine_b = next_cosine_b;
        sine_b = next_sine_b;
    }

    float divisor = (float)sample_count;
    *input_rms = sqrtf(input_sum / divisor);
    *model_rms = sqrtf(model_sum / divisor);
    *residual_rms = sqrtf(residual_sum / divisor);
    if (*input_rms <= 0.0f)
    {
        return false;
    }
    *normalized_residual = *residual_rms / *input_rms;
    return isfinite(*normalized_residual);
}

void SignalDualSine_ResetResult(void)
{
    memset((void *)&g_signal_dual_sine_result,
           0,
           sizeof(g_signal_dual_sine_result));
}

bool SignalDualSine_Analyze(const float *unwindowed_samples,
                            uint32_t sample_count,
                            const SignalFftResult *fft_result,
                            SignalDualSineResult *result)
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
            < SIGNAL_DUAL_SINE_TONE_COUNT))
    {
        return false;
    }

    memset(s_dual_estimates, 0, sizeof(s_dual_estimates));
    uint32_t estimate_count = fft_result->selected_candidate_count;
    if (estimate_count > APP_ANALYSIS_MAX_SELECTED_CANDIDATES)
    {
        estimate_count = APP_ANALYSIS_MAX_SELECTED_CANDIDATES;
    }

    for (uint32_t index = 0U; index < estimate_count; ++index)
    {
        const SignalFrequencyCandidate *candidate =
            &fft_result->selected[index];
        float frequency_hz = candidate->nominal_frequency_hz;
        if (!signal_dual_sine_estimate(unwindowed_samples,
                                       sample_count,
                                       fft_result->sample_rate_hz,
                                       frequency_hz,
                                       &s_dual_estimates[index].estimate))
        {
            return false;
        }
        s_dual_estimates[index].frequency_hz = frequency_hz;
        s_dual_estimates[index].source_candidate_rank = candidate->rank;
    }

    float best_residual = INFINITY;
    memset(&s_dual_best_result, 0, sizeof(s_dual_best_result));
    s_dual_best_result.sample_rate_hz = fft_result->sample_rate_hz;
    s_dual_best_result.sample_count = sample_count;

    for (uint32_t first = 0U; first + 1U < estimate_count; ++first)
    {
        for (uint32_t second = first + 1U;
             second < estimate_count;
             ++second)
        {
            float input_rms = 0.0f;
            float model_rms = 0.0f;
            float residual_rms = 0.0f;
            float normalized_residual = 0.0f;
            ++s_dual_best_result.evaluated_pairs;
            if (!signal_dual_sine_evaluate_pair(
                    unwindowed_samples,
                    sample_count,
                    fft_result->sample_rate_hz,
                    &s_dual_estimates[first],
                    &s_dual_estimates[second],
                    &input_rms,
                    &model_rms,
                    &residual_rms,
                    &normalized_residual))
            {
                continue;
            }

            if (normalized_residual < best_residual)
            {
                best_residual = normalized_residual;
                if (s_dual_estimates[first].frequency_hz
                    <= s_dual_estimates[second].frequency_hz)
                {
                    s_dual_best_result.tones[0] = s_dual_estimates[first];
                    s_dual_best_result.tones[1] = s_dual_estimates[second];
                }
                else
                {
                    s_dual_best_result.tones[0] = s_dual_estimates[second];
                    s_dual_best_result.tones[1] = s_dual_estimates[first];
                }
                s_dual_best_result.input_rms_code = input_rms;
                s_dual_best_result.model_rms_code = model_rms;
                s_dual_best_result.residual_rms_code = residual_rms;
                s_dual_best_result.normalized_residual = normalized_residual;
                s_dual_best_result.relative_phase_rad =
                    signal_dual_sine_wrap_phase(
                        s_dual_best_result.tones[1].estimate.phase_rad
                        - s_dual_best_result.tones[0].estimate.phase_rad);
            }
        }
    }

    s_dual_best_result.valid = isfinite(best_residual)
        && (best_residual <= APP_ANALYSIS_DUAL_SINE_MAX_RESIDUAL);
    *result = s_dual_best_result;
    return s_dual_best_result.valid;
}

void SignalDualSine_PublishResult(const SignalDualSineResult *result,
                                  uint32_t processing_time_us)
{
    if (result == NULL)
    {
        SignalDualSine_ResetResult();
        return;
    }
    memcpy((void *)&g_signal_dual_sine_result,
           result,
           sizeof(g_signal_dual_sine_result));
    g_signal_dual_sine_result.processing_time_us = processing_time_us;
}
