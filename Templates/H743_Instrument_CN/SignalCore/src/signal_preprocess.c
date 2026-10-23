/**
  ******************************************************************************
  * @file    signal_preprocess.c
  * @brief   AD9226 单帧去直流、质量统计和 Hann 加窗（UTF-8）。
  ******************************************************************************
  */

#include "signal_preprocess.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "signal_core_port.h"

/* CPU 专用工作区放在 AXI RAM，禁止占用 MainTask 栈或 DMA 缓冲区。 */
__attribute__((section(".fft_buffer.preprocess_windowed"), aligned(32)))
float g_signal_preprocess_windowed[SIGNAL_PREPROCESS_SAMPLE_COUNT];

__attribute__((section(".fft_buffer.preprocess_unwindowed"), aligned(32)))
float g_signal_preprocess_unwindowed[SIGNAL_PREPROCESS_SAMPLE_COUNT];

__attribute__((section(".fft_buffer.preprocess_hann"), aligned(32)))
static float s_hann_window[SIGNAL_PREPROCESS_SAMPLE_COUNT];

volatile SignalPreprocessInfo g_signal_preprocess_info;
volatile SignalPreprocessDiagnostics g_signal_preprocess_diagnostics;

static bool s_hann_initialized;
static float s_hann_sum;

void SignalPreprocess_Init(void)
{
    if (s_hann_initialized)
    {
        return;
    }

    float sum = 0.0f;
    const float denominator =
        (float)(SIGNAL_PREPROCESS_SAMPLE_COUNT - 1U);

    for (uint32_t i = 0U; i < SIGNAL_PREPROCESS_SAMPLE_COUNT; ++i)
    {
        float phase = (2.0f * SIGNAL_PREPROCESS_PI_F * (float)i)
                      / denominator;
        float coefficient = 0.5f - (0.5f * cosf(phase));
        s_hann_window[i] = coefficient;
        sum += coefficient;
    }

    s_hann_sum = sum;
    s_hann_initialized = true;
}

bool SignalPreprocess_Run(const uint16_t *raw,
                          uint32_t count,
                          float *windowed_output,
                          float *unwindowed_output,
                          SignalPreprocessInfo *info)
{
    if (info == NULL)
    {
        return false;
    }

    memset(info, 0, sizeof(*info));
    info->sample_count = count;

    if ((raw == NULL)
        || !SignalCorePort_IsConfigured()
        || (windowed_output == NULL)
        || (unwindowed_output == NULL)
        || (count != SIGNAL_PREPROCESS_SAMPLE_COUNT)
        || !s_hann_initialized)
    {
        return false;
    }

    uint64_t code_sum = 0ULL;
    uint32_t clip_count = 0U;

    for (uint32_t i = 0U; i < count; ++i)
    {
        /* 原始采样码转换由平台适配器提供。 */
        uint16_t code = SignalCorePort_RawToCode(raw[i]);
        code_sum += code;
        if ((code <= SIGNAL_DETECT_CLIP_LOW_CODE)
            || (code >= SIGNAL_DETECT_CLIP_HIGH_CODE))
        {
            ++clip_count;
        }
    }

    float dc_offset = (float)((double)code_sum / (double)count);
    double centered_sum = 0.0;
    double square_sum = 0.0;
    float peak = 0.0f;
    uint32_t non_finite_count = 0U;

    for (uint32_t i = 0U; i < count; ++i)
    {
        float centered = (float)SignalCorePort_RawToCode(raw[i]) - dc_offset;
        float magnitude = fabsf(centered);

        if (!isfinite(centered))
        {
            ++non_finite_count;
            centered = 0.0f;
            magnitude = 0.0f;
        }

        unwindowed_output[i] = centered;
        windowed_output[i] = centered * s_hann_window[i];
        centered_sum += centered;
        square_sum += (double)centered * (double)centered;
        if (magnitude > peak)
        {
            peak = magnitude;
        }
    }

    info->dc_offset = dc_offset;
    info->centered_mean = (float)(centered_sum / (double)count);
    info->rms = sqrtf((float)(square_sum / (double)count));
    info->peak = peak;
    info->clip_count = clip_count;
    info->clip_ratio = (float)clip_count / (float)count;
    info->non_finite_count = non_finite_count;
    info->hann_first = s_hann_window[0U];
    info->hann_middle = s_hann_window[count / 2U];
    info->hann_last = s_hann_window[count - 1U];
    info->hann_sum = s_hann_sum;
    info->hann_coherent_gain = s_hann_sum / (float)count;
    info->hann_amplitude_scale = 2.0f / s_hann_sum;
    info->hann_initialized = s_hann_initialized;
    info->valid = (non_finite_count == 0U)
                  && isfinite(info->rms)
                  && isfinite(info->peak)
                  && (info->rms >= SIGNAL_PREPROCESS_MIN_RMS_CODE)
                  && (info->clip_ratio
                      <= SIGNAL_PREPROCESS_MAX_CLIP_RATIO);

    return info->valid;
}

void SignalPreprocess_ResetDiagnostics(void)
{
    SignalPreprocessDiagnostics reset = {0};
    SignalPreprocessInfo reset_info = {0};
    reset.dc_offset_min = FLT_MAX;
    reset.rms_min = FLT_MAX;
    reset.peak_min = FLT_MAX;
    reset.processing_time_min_us = UINT32_MAX;
    g_signal_preprocess_diagnostics = reset;
    g_signal_preprocess_info = reset_info;
}

void SignalPreprocess_RecordDiagnostics(const SignalPreprocessInfo *info,
                                        uint32_t processing_time_us)
{
    if (info == NULL)
    {
        return;
    }

    SignalPreprocessDiagnostics current = g_signal_preprocess_diagnostics;
    ++current.processed_frames;
    if (info->valid)
    {
        ++current.valid_frames;
    }
    else
    {
        ++current.invalid_frames;
    }

    if (info->dc_offset < current.dc_offset_min)
    {
        current.dc_offset_min = info->dc_offset;
    }
    if (info->dc_offset > current.dc_offset_max)
    {
        current.dc_offset_max = info->dc_offset;
    }

    float centered_mean_abs = fabsf(info->centered_mean);
    if (centered_mean_abs > current.centered_mean_abs_max)
    {
        current.centered_mean_abs_max = centered_mean_abs;
    }
    if (info->rms < current.rms_min)
    {
        current.rms_min = info->rms;
    }
    if (info->rms > current.rms_max)
    {
        current.rms_max = info->rms;
    }
    if (info->peak < current.peak_min)
    {
        current.peak_min = info->peak;
    }
    if (info->peak > current.peak_max)
    {
        current.peak_max = info->peak;
    }
    if (info->clip_ratio > current.clip_ratio_max)
    {
        current.clip_ratio_max = info->clip_ratio;
    }
    if (processing_time_us < current.processing_time_min_us)
    {
        current.processing_time_min_us = processing_time_us;
    }
    if (processing_time_us > current.processing_time_max_us)
    {
        current.processing_time_max_us = processing_time_us;
    }
    current.processing_time_last_us = processing_time_us;
    current.passed = (current.processed_frames >= 20U)
                     && (current.invalid_frames == 0U);

    SignalPreprocessInfo published = *info;
    published.processing_time_us = processing_time_us;
    g_signal_preprocess_info = published;
    g_signal_preprocess_diagnostics = current;
}
