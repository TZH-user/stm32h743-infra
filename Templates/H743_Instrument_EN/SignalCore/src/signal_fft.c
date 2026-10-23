/**
  ******************************************************************************
  * @file    signal_fft.c
  * @brief   4096 点 CMSIS-DSP RFFT 与可配置候选频率检测（UTF-8）。
  ******************************************************************************
  */

#include "signal_fft.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "arm_math.h"
#include "signal_core_port.h"

#define SIGNAL_FFT_SIZE                 SIGNAL_PREPROCESS_SAMPLE_COUNT
#define SIGNAL_FFT_HALF_SIZE            (SIGNAL_FFT_SIZE / 2U)
#define SIGNAL_FFT_NOISE_LIMIT_FACTOR   4.0f
#define SIGNAL_FFT_HANN_ENERGY_GAIN     1.5f
#define SIGNAL_FFT_MIN_POWER            1.0e-20f

_Static_assert(SIGNAL_FFT_SIZE == 4096U,
               "E3 uses one fixed 4096-point RFFT");
_Static_assert(APP_ANALYSIS_MAX_SELECTED_CANDIDATES <=
                   SIGNAL_FFT_MAX_GRID_CANDIDATES,
               "Selected candidate capacity is invalid");

/*
 * CMSIS-DSP 快速实数 FFT 的打包格式：output[0] 为 DC，output[1] 为
 * Nyquist；1..N/2-1 号复数 bin 的实部/虚部分别位于 2*k 和 2*k+1。
 */
__attribute__((section(".fft_buffer.signal_fft_output"), aligned(32)))
static float s_fft_output[SIGNAL_FFT_SIZE];

__attribute__((section(".fft_buffer.signal_fft_power"), aligned(32)))
float g_signal_fft_power[SIGNAL_FFT_SPECTRUM_BINS];

__attribute__((section(".fft_buffer.signal_fft_candidates"), aligned(32)))
SignalFrequencyCandidate
    g_signal_fft_candidates[SIGNAL_FFT_MAX_GRID_CANDIDATES];

SignalFftResult g_signal_fft_result;
volatile SignalFftDiagnostics g_signal_fft_diagnostics;

static arm_rfft_fast_instance_f32 s_fft_instance;
static bool s_fft_initialized;

/* 计时由 SignalCorePort 注入，核心算法不依赖具体 RTOS。 */
static uint32_t signal_fft_elapsed_us(uint32_t start_count)
{
    return SignalCorePort_ElapsedUs(start_count,
                                    SignalCorePort_TimerCount());
}

static uint32_t signal_fft_frequency_to_bin(uint32_t frequency_hz,
                                            uint32_t sample_rate_hz)
{
    return (uint32_t)((((uint64_t)frequency_hz * SIGNAL_FFT_SIZE)
                       + (sample_rate_hz / 2U))
                      / sample_rate_hz);
}

static float signal_fft_refine_frequency(uint32_t peak_bin,
                                         uint32_t sample_rate_hz)
{
    if ((peak_bin == 0U) || (peak_bin >= SIGNAL_FFT_HALF_SIZE))
    {
        return ((float)peak_bin * (float)sample_rate_hz)
               / (float)SIGNAL_FFT_SIZE;
    }

    float left = g_signal_fft_power[peak_bin - 1U];
    float center = g_signal_fft_power[peak_bin];
    float right = g_signal_fft_power[peak_bin + 1U];
    float denominator = left - (2.0f * center) + right;
    float offset = 0.0f;

    if (fabsf(denominator) > SIGNAL_FFT_MIN_POWER)
    {
        offset = 0.5f * (left - right) / denominator;
        if (offset > 0.5f)
        {
            offset = 0.5f;
        }
        else if (offset < -0.5f)
        {
            offset = -0.5f;
        }
    }

    return (((float)peak_bin + offset) * (float)sample_rate_hz)
           / (float)SIGNAL_FFT_SIZE;
}

static bool signal_fft_validate_config(const SignalFftConfig *config,
                                       uint32_t *candidate_count)
{
    if ((config == NULL)
        || (candidate_count == NULL)
        || (config->sample_rate_hz == 0U)
        || (config->min_frequency_hz == 0U)
        || (config->min_frequency_hz > config->max_frequency_hz)
        || (config->max_frequency_hz
            >= (config->sample_rate_hz / 2U))
        || (config->candidate_step_hz == 0U)
        || (((uint64_t)config->candidate_step_hz * SIGNAL_FFT_SIZE)
            < config->sample_rate_hz)
        || !isfinite(config->minimum_snr_db)
        || !isfinite(config->minimum_amplitude_code)
        || (config->minimum_amplitude_code < 0.0f)
        || (config->max_selected_candidates == 0U)
        || (config->max_selected_candidates
            > APP_ANALYSIS_MAX_SELECTED_CANDIDATES))
    {
        return false;
    }

    uint32_t count = ((config->max_frequency_hz
                       - config->min_frequency_hz)
                      / config->candidate_step_hz) + 1U;
    if ((count == 0U) || (count > SIGNAL_FFT_MAX_GRID_CANDIDATES))
    {
        return false;
    }

    uint32_t first_bin = signal_fft_frequency_to_bin(
        config->min_frequency_hz,
        config->sample_rate_hz);
    uint32_t last_frequency_hz = config->min_frequency_hz
        + ((count - 1U) * config->candidate_step_hz);
    uint32_t last_bin = signal_fft_frequency_to_bin(
        last_frequency_hz,
        config->sample_rate_hz);
    if ((first_bin < 1U)
        || (last_bin >= SIGNAL_FFT_HALF_SIZE)
        || ((count > 1U) && (last_bin <= first_bin)))
    {
        return false;
    }

    *candidate_count = count;
    return true;
}

static bool signal_fft_make_power_spectrum(void)
{
    g_signal_fft_power[0U] = s_fft_output[0U] * s_fft_output[0U];
    g_signal_fft_power[SIGNAL_FFT_HALF_SIZE] =
        s_fft_output[1U] * s_fft_output[1U];

    for (uint32_t bin = 1U; bin < SIGNAL_FFT_HALF_SIZE; ++bin)
    {
        float real = s_fft_output[2U * bin];
        float imaginary = s_fft_output[(2U * bin) + 1U];
        float power = (real * real) + (imaginary * imaginary);
        if (!isfinite(power))
        {
            return false;
        }
        g_signal_fft_power[bin] = power;
    }

    return isfinite(g_signal_fft_power[0U])
           && isfinite(g_signal_fft_power[SIGNAL_FFT_HALF_SIZE]);
}

static float signal_fft_estimate_noise_floor(uint32_t first_bin,
                                             uint32_t last_bin)
{
    double sum = 0.0;
    uint32_t count = 0U;
    for (uint32_t bin = first_bin; bin <= last_bin; ++bin)
    {
        sum += g_signal_fft_power[bin];
        ++count;
    }

    if (count == 0U)
    {
        return SIGNAL_FFT_MIN_POWER;
    }

    float first_mean = (float)(sum / (double)count);
    float limit = first_mean * SIGNAL_FFT_NOISE_LIMIT_FACTOR;
    double trimmed_sum = 0.0;
    uint32_t trimmed_count = 0U;
    for (uint32_t bin = first_bin; bin <= last_bin; ++bin)
    {
        float power = g_signal_fft_power[bin];
        if (power <= limit)
        {
            trimmed_sum += power;
            ++trimmed_count;
        }
    }

    float noise_floor = (trimmed_count >= 8U)
        ? (float)(trimmed_sum / (double)trimmed_count)
        : first_mean;
    return (noise_floor > SIGNAL_FFT_MIN_POWER)
        ? noise_floor
        : SIGNAL_FFT_MIN_POWER;
}

static void signal_fft_find_full_spectrum_peak(
    const SignalFftConfig *config,
    SignalFftResult *result)
{
    uint32_t peak_bin = 1U;
    float peak_power = g_signal_fft_power[peak_bin];
    for (uint32_t bin = 2U; bin < SIGNAL_FFT_HALF_SIZE; ++bin)
    {
        if (g_signal_fft_power[bin] > peak_power)
        {
            peak_power = g_signal_fft_power[bin];
            peak_bin = bin;
        }
    }

    float local_energy = g_signal_fft_power[peak_bin - 1U]
        + g_signal_fft_power[peak_bin]
        + g_signal_fft_power[peak_bin + 1U];
    float peak_frequency_hz = signal_fft_refine_frequency(
        peak_bin,
        config->sample_rate_hz);
    result->full_spectrum_peak_bin = peak_bin;
    result->full_spectrum_peak_frequency_hz = peak_frequency_hz;
    result->full_spectrum_peak_amplitude_code =
        sqrtf(local_energy / SIGNAL_FFT_HANN_ENERGY_GAIN)
        * (4.0f / (float)(SIGNAL_FFT_SIZE - 1U));
    result->full_spectrum_peak_in_search_range =
        (peak_frequency_hz >= (float)config->min_frequency_hz)
        && (peak_frequency_hz <= (float)config->max_frequency_hz);
}

static void signal_fft_fill_candidates(const SignalFftConfig *config,
                                       uint32_t candidate_count,
                                       float noise_floor_power)
{
    const float hann_amplitude_scale =
        4.0f / (float)(SIGNAL_FFT_SIZE - 1U);
    const float local_noise_power = 3.0f * noise_floor_power;

    for (uint32_t i = 0U; i < candidate_count; ++i)
    {
        SignalFrequencyCandidate *candidate =
            &g_signal_fft_candidates[i];
        uint32_t nominal_frequency_hz = config->min_frequency_hz
            + (i * config->candidate_step_hz);
        uint32_t center_bin = signal_fft_frequency_to_bin(
            nominal_frequency_hz,
            config->sample_rate_hz);
        float left = g_signal_fft_power[center_bin - 1U];
        float center = g_signal_fft_power[center_bin];
        float right = g_signal_fft_power[center_bin + 1U];
        uint32_t peak_bin = center_bin;

        if ((left > center) && (left >= right))
        {
            peak_bin = center_bin - 1U;
        }
        else if (right > center)
        {
            peak_bin = center_bin + 1U;
        }

        candidate->nominal_frequency_hz =
            (float)nominal_frequency_hz;
        candidate->measured_frequency_hz = signal_fft_refine_frequency(
            peak_bin,
            config->sample_rate_hz);
        candidate->center_bin = center_bin;
        candidate->peak_bin = peak_bin;
        candidate->local_energy = left + center + right;
        candidate->amplitude_code = sqrtf(
            candidate->local_energy / SIGNAL_FFT_HANN_ENERGY_GAIN)
            * hann_amplitude_scale;
        candidate->snr_db = 10.0f * log10f(
            candidate->local_energy / local_noise_power);
        candidate->rank = 0U;
        candidate->active = (candidate->amplitude_code
                             >= config->minimum_amplitude_code)
                            && (candidate->snr_db
                                >= config->minimum_snr_db);
    }

    /* 邻近候选抑制：只保留候选能量序列中的局部极大值。 */
    for (uint32_t i = 0U; i < candidate_count; ++i)
    {
        bool not_lower_than_left = (i == 0U)
            || (g_signal_fft_candidates[i].local_energy
                >= g_signal_fft_candidates[i - 1U].local_energy);
        bool higher_than_right = (i == (candidate_count - 1U))
            || (g_signal_fft_candidates[i].local_energy
                > g_signal_fft_candidates[i + 1U].local_energy);
        g_signal_fft_candidates[i].active =
            g_signal_fft_candidates[i].active
            && not_lower_than_left
            && higher_than_right;
    }
}

static void signal_fft_select_candidates(const SignalFftConfig *config,
                                         uint32_t candidate_count,
                                         SignalFftResult *result)
{
    uint32_t active_count = 0U;
    for (uint32_t i = 0U; i < candidate_count; ++i)
    {
        if (g_signal_fft_candidates[i].active)
        {
            ++active_count;
        }
    }
    result->active_candidate_count = active_count;

    for (uint32_t rank = 0U;
         rank < config->max_selected_candidates;
         ++rank)
    {
        uint32_t best_index = UINT32_MAX;
        float best_energy = -1.0f;
        for (uint32_t i = 0U; i < candidate_count; ++i)
        {
            SignalFrequencyCandidate *candidate =
                &g_signal_fft_candidates[i];
            if (candidate->active
                && (candidate->rank == 0U)
                && (candidate->local_energy > best_energy))
            {
                best_energy = candidate->local_energy;
                best_index = i;
            }
        }

        if (best_index == UINT32_MAX)
        {
            break;
        }

        g_signal_fft_candidates[best_index].rank = rank + 1U;
        result->selected[rank] = g_signal_fft_candidates[best_index];
        ++result->selected_candidate_count;
    }
}

bool SignalFft_Init(void)
{
    if (s_fft_initialized)
    {
        return true;
    }

    if (arm_rfft_fast_init_f32(&s_fft_instance, SIGNAL_FFT_SIZE)
        != ARM_MATH_SUCCESS)
    {
        return false;
    }

    s_fft_initialized = true;
    ++g_signal_fft_diagnostics.initialization_count;
    return true;
}

void SignalFft_LoadDefaultConfig(SignalFftConfig *config,
                                uint32_t sample_rate_hz)
{
    if (config == NULL)
    {
        return;
    }

    config->sample_rate_hz = sample_rate_hz;
    config->min_frequency_hz = APP_ANALYSIS_DEFAULT_DISPLAY_MIN_HZ;
    config->max_frequency_hz = APP_ANALYSIS_DEFAULT_DISPLAY_MAX_HZ;
    config->candidate_step_hz =
        APP_ANALYSIS_DEFAULT_CANDIDATE_STEP_HZ;
    config->minimum_snr_db = APP_ANALYSIS_DEFAULT_MIN_SNR_DB;
    config->minimum_amplitude_code =
        APP_ANALYSIS_DEFAULT_MIN_AMPLITUDE_CODE;
    config->max_selected_candidates =
        APP_ANALYSIS_MAX_SELECTED_CANDIDATES;
}

bool SignalFft_Run(float *windowed_input,
                   const SignalFftConfig *config,
                   SignalFftResult *result)
{
    if (result == NULL)
    {
        return false;
    }

    memset(result, 0, sizeof(*result));
    memset(g_signal_fft_candidates,
           0,
           sizeof(g_signal_fft_candidates));

    uint32_t candidate_count = 0U;
    if ((windowed_input == NULL)
        || !SignalCorePort_IsConfigured()
        || !s_fft_initialized
        || !signal_fft_validate_config(config, &candidate_count))
    {
        return false;
    }

    result->sample_rate_hz = config->sample_rate_hz;
    result->fft_size = SIGNAL_FFT_SIZE;
    result->frequency_resolution_hz =
        (float)config->sample_rate_hz / (float)SIGNAL_FFT_SIZE;
    result->min_frequency_hz = config->min_frequency_hz;
    result->max_frequency_hz = config->max_frequency_hz;
    result->candidate_step_hz = config->candidate_step_hz;
    result->candidate_count = candidate_count;
    result->initialized = s_fft_initialized;

    uint32_t transform_start = SignalCorePort_TimerCount();
    arm_rfft_fast_f32(&s_fft_instance,
                      windowed_input,
                      s_fft_output,
                      0U);
    bool spectrum_valid = signal_fft_make_power_spectrum();
    if (spectrum_valid)
    {
        signal_fft_find_full_spectrum_peak(config, result);
    }
    result->transform_time_us = signal_fft_elapsed_us(transform_start);
    if (!spectrum_valid)
    {
        return false;
    }

    uint32_t candidate_start = SignalCorePort_TimerCount();
    uint32_t first_bin = signal_fft_frequency_to_bin(
        config->min_frequency_hz,
        config->sample_rate_hz);
    uint32_t last_bin = signal_fft_frequency_to_bin(
        config->max_frequency_hz,
        config->sample_rate_hz);
    float noise_floor_power = signal_fft_estimate_noise_floor(first_bin,
                                                              last_bin);
    result->noise_floor_power = noise_floor_power;
    result->noise_floor_amplitude_code = sqrtf(noise_floor_power)
        * (4.0f / (float)(SIGNAL_FFT_SIZE - 1U));

    signal_fft_fill_candidates(config,
                               candidate_count,
                               noise_floor_power);
    signal_fft_select_candidates(config, candidate_count, result);
    result->candidate_time_us = signal_fft_elapsed_us(candidate_start);
    result->valid = (result->selected_candidate_count > 0U);
    g_signal_fft_result = *result;
    return result->valid;
}

void SignalFft_ResetDiagnostics(void)
{
    SignalFftResult reset_result = {0};
    SignalFftDiagnostics reset_diagnostics = {0};
    memset(g_signal_fft_candidates,
           0,
           sizeof(g_signal_fft_candidates));
    g_signal_fft_result = reset_result;
    g_signal_fft_diagnostics = reset_diagnostics;
}

void SignalFft_RecordDiagnostics(bool successful,
                                uint32_t processing_time_us)
{
    SignalFftDiagnostics diagnostics = g_signal_fft_diagnostics;
    ++diagnostics.run_count;
    if (successful)
    {
        ++diagnostics.successful_runs;
    }
    else
    {
        ++diagnostics.failed_runs;
    }
    diagnostics.last_processing_time_us = processing_time_us;
    g_signal_fft_result.processing_time_us = processing_time_us;
    g_signal_fft_diagnostics = diagnostics;
}
