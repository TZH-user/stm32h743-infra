/**
  ******************************************************************************
  * @file    signal_harmonic.c
  * @brief   E5 谐波特征提取与正弦/三角波分类（UTF-8）。
  ******************************************************************************
  */

#include "signal_harmonic.h"

#include "app_analysis_config.h"
#include "app_trig_lookup.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define SIGNAL_HARMONIC_PI_F  3.14159265358979323846f

volatile SignalHarmonicResult g_signal_harmonic_result;

/* 计算指定频率的复数相关幅值；不申请额外帧缓冲区。 */
static float signal_harmonic_amplitude(
    const float *samples,
    uint32_t sample_count,
    uint32_t sample_rate_hz,
    float frequency_hz,
    const SignalDualSineTone *tone_to_remove)
{
    if ((frequency_hz <= 0.0f)
        || (frequency_hz >= ((float)sample_rate_hz * 0.5f)))
    {
        return 0.0f;
    }

    float i_sum = 0.0f;
    float q_sum = 0.0f;
    float angle_step = 2.0f * SIGNAL_HARMONIC_PI_F * frequency_hz
        / (float)sample_rate_hz;
    float cosine = 1.0f;
    float sine = 0.0f;
    float cosine_step;
    float sine_step;
    AppTrigLookup_SinCosRadians(angle_step, &sine_step, &cosine_step);

    float remove_step = 0.0f;
    float remove_cosine = 1.0f;
    float remove_sine = 0.0f;
    float remove_cosine_step = 1.0f;
    float remove_sine_step = 0.0f;
    if (tone_to_remove != NULL)
    {
        remove_step = 2.0f * SIGNAL_HARMONIC_PI_F
            * tone_to_remove->frequency_hz / (float)sample_rate_hz;
        AppTrigLookup_SinCosRadians(tone_to_remove->estimate.phase_rad,
                                    &remove_sine,
                                    &remove_cosine);
        AppTrigLookup_SinCosRadians(remove_step,
                                    &remove_sine_step,
                                    &remove_cosine_step);
    }

    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        float sample = samples[index];
        if (tone_to_remove != NULL)
        {
            sample -= tone_to_remove->estimate.amplitude
                * remove_cosine;
        }

        i_sum += sample * cosine;
        q_sum -= sample * sine;

        float next_cosine = cosine * cosine_step - sine * sine_step;
        float next_sine = sine * cosine_step + cosine * sine_step;
        cosine = next_cosine;
        sine = next_sine;
        if (tone_to_remove != NULL)
        {
            float next_remove_cosine = remove_cosine * remove_cosine_step
                - remove_sine * remove_sine_step;
            float next_remove_sine = remove_sine * remove_cosine_step
                + remove_cosine * remove_sine_step;
            remove_cosine = next_remove_cosine;
            remove_sine = next_remove_sine;
        }
    }

    float scale = 2.0f / (float)sample_count;
    float i_scaled = i_sum * scale;
    float q_scaled = q_sum * scale;
    return sqrtf(i_scaled * i_scaled + q_scaled * q_scaled);
}

static bool signal_harmonic_classify(
    const float *samples,
    uint32_t sample_count,
    uint32_t sample_rate_hz,
    float frequency_hz,
    const SignalDualSineTone *tone_to_remove,
    SignalHarmonicComponent *component)
{
    if ((samples == NULL) || (component == NULL)
        || (sample_count == 0U) || (sample_rate_hz == 0U))
    {
        return false;
    }

    memset(component, 0, sizeof(*component));
    component->frequency_hz = frequency_hz;
    SignalHarmonicFeature *feature = &component->feature;
    feature->a1 = signal_harmonic_amplitude(samples,
                                             sample_count,
                                             sample_rate_hz,
                                             frequency_hz,
                                             tone_to_remove);
    feature->a2 = signal_harmonic_amplitude(samples,
                                             sample_count,
                                             sample_rate_hz,
                                             2.0f * frequency_hz,
                                             tone_to_remove);
    feature->a3 = signal_harmonic_amplitude(samples,
                                             sample_count,
                                             sample_rate_hz,
                                             3.0f * frequency_hz,
                                             tone_to_remove);
    feature->a4 = signal_harmonic_amplitude(samples,
                                             sample_count,
                                             sample_rate_hz,
                                             4.0f * frequency_hz,
                                             tone_to_remove);
    feature->a5 = signal_harmonic_amplitude(samples,
                                             sample_count,
                                             sample_rate_hz,
                                             5.0f * frequency_hz,
                                             tone_to_remove);
    feature->a7 = signal_harmonic_amplitude(samples,
                                             sample_count,
                                             sample_rate_hz,
                                             7.0f * frequency_hz,
                                             tone_to_remove);

    if (!isfinite(feature->a1)
        || (feature->a1 < APP_ANALYSIS_DEFAULT_MIN_AMPLITUDE_CODE))
    {
        component->type = SIGNAL_WAVE_UNKNOWN;
        return false;
    }

    float a1_square = feature->a1 * feature->a1;
    feature->odd_energy = feature->a3 * feature->a3
        + feature->a5 * feature->a5 + feature->a7 * feature->a7;
    feature->even_energy = feature->a2 * feature->a2
        + feature->a4 * feature->a4;
    feature->thd = sqrtf((feature->odd_energy + feature->even_energy)
                         / a1_square);

    float ratio3 = feature->a3 / feature->a1;
    float ratio5 = feature->a5 / feature->a1;
    float ratio7 = feature->a7 / feature->a1;
    float even_ratio = sqrtf(feature->even_energy / a1_square);
    float triangle_error =
        0.55f * fabsf(ratio3 - (1.0f / 9.0f)) / (1.0f / 9.0f)
        + 0.25f * fabsf(ratio5 - (1.0f / 25.0f)) / (1.0f / 25.0f)
        + 0.10f * fabsf(ratio7 - (1.0f / 49.0f)) / (1.0f / 49.0f)
        + 0.10f * even_ratio / APP_ANALYSIS_TRIANGLE_EVEN_RATIO_SCALE;

    feature->sine_score = 1.0f
        / (1.0f + feature->thd / APP_ANALYSIS_SINE_THD_SCALE);
    feature->triangle_score = 1.0f / (1.0f + triangle_error);

    bool triangle = (ratio3 >= APP_ANALYSIS_TRIANGLE_MIN_A3_RATIO)
        && (feature->triangle_score
            >= feature->sine_score + APP_ANALYSIS_WAVE_SCORE_MIN_GAP);
    component->type = triangle ? SIGNAL_WAVE_TRIANGLE : SIGNAL_WAVE_SINE;
    component->confidence = triangle
        ? feature->triangle_score : feature->sine_score;
    return isfinite(feature->thd)
        && isfinite(component->confidence);
}

void SignalHarmonic_ResetResult(void)
{
    memset((void *)&g_signal_harmonic_result,
           0,
           sizeof(g_signal_harmonic_result));
}

bool SignalHarmonic_Analyze(const float *unwindowed_samples,
                            uint32_t sample_count,
                            const SignalFftResult *fft_result,
                            const SignalDualSineResult *dual_result,
                            SignalHarmonicResult *result)
{
    if (result == NULL)
    {
        return false;
    }

    memset(result, 0, sizeof(*result));
    if ((unwindowed_samples == NULL) || (fft_result == NULL)
        || !fft_result->valid || (fft_result->sample_rate_hz == 0U)
        || (sample_count != fft_result->fft_size)
        || (fft_result->selected_candidate_count == 0U))
    {
        return false;
    }

    result->sample_rate_hz = fft_result->sample_rate_hz;
    result->sample_count = sample_count;

    /*
     * 先直接检查最强基频。单个三角波的三次谐波可能被 E4 当成第二路
     * 正弦；若这里已呈现三角特征，就不错误扣除该谐波。
     */
    SignalHarmonicComponent primary = {0};
    if (!signal_harmonic_classify(
            unwindowed_samples,
            sample_count,
            fft_result->sample_rate_hz,
            fft_result->selected[0].nominal_frequency_hz,
            NULL,
            &primary))
    {
        return false;
    }
    if (primary.type == SIGNAL_WAVE_TRIANGLE)
    {
        result->component_count = 1U;
        result->components[0] = primary;
        result->valid = true;
        return true;
    }

    bool use_dual = (dual_result != NULL) && dual_result->valid;
    result->component_count = use_dual ? SIGNAL_HARMONIC_MAX_COMPONENTS : 1U;

    if (!use_dual)
    {
        result->components[0] = primary;
        result->valid = true;
        return true;
    }

    for (uint32_t index = 0U; index < result->component_count; ++index)
    {
        float frequency_hz = dual_result->tones[index].frequency_hz;
        const SignalDualSineTone *tone_to_remove =
            &dual_result->tones[1U - index];
        if (!signal_harmonic_classify(unwindowed_samples,
                                      sample_count,
                                      fft_result->sample_rate_hz,
                                      frequency_hz,
                                      tone_to_remove,
                                      &result->components[index]))
        {
            return false;
        }
    }

    result->valid = true;
    return true;
}

void SignalHarmonic_PublishResult(const SignalHarmonicResult *result,
                                  uint32_t processing_time_us)
{
    if (result == NULL)
    {
        SignalHarmonic_ResetResult();
        return;
    }

    memcpy((void *)&g_signal_harmonic_result,
           result,
           sizeof(g_signal_harmonic_result));
    g_signal_harmonic_result.processing_time_us = processing_time_us;
}

const char *SignalHarmonic_WaveTypeString(SignalWaveType type)
{
    switch (type)
    {
        case SIGNAL_WAVE_SINE:
            return "SINE";
        case SIGNAL_WAVE_TRIANGLE:
            return "TRIANGLE";
        default:
            return "UNKNOWN";
    }
}
