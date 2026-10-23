/**
  ******************************************************************************
  * @file    signal_detect.c
  * @brief   信号识别 E1 单帧数据质量统计（UTF-8）。
  ******************************************************************************
  */

#include "signal_detect.h"

#include "ad9226.h"

#include <math.h>
#include <string.h>

#define SIGNAL_DETECT_GATE_REPORT_VERSION  1U

volatile SignalAcquisitionGateReport g_signal_acquisition_gate_report;

static const uint32_t s_gate_rates_hz[SIGNAL_DETECT_GATE_RATE_COUNT] = {
    SIGNAL_DETECT_GATE_RATE_0_HZ,
    SIGNAL_DETECT_GATE_RATE_1_HZ,
    SIGNAL_DETECT_GATE_RATE_2_HZ,
};

void SignalDetect_ResetGateReport(void)
{
    memset((void *)&g_signal_acquisition_gate_report,
           0,
           sizeof(g_signal_acquisition_gate_report));

    g_signal_acquisition_gate_report.version =
        SIGNAL_DETECT_GATE_REPORT_VERSION;
    g_signal_acquisition_gate_report.rate_count =
        SIGNAL_DETECT_GATE_RATE_COUNT;

    for (uint32_t i = 0U; i < SIGNAL_DETECT_GATE_RATE_COUNT; ++i)
    {
        g_signal_acquisition_gate_report.rates[i]
            .requested_sample_rate_hz = s_gate_rates_hz[i];
        g_signal_acquisition_gate_report.rates[i].target_frames =
            SIGNAL_DETECT_GATE_FRAMES_PER_RATE;
        g_signal_acquisition_gate_report.rates[i].observed_min =
            AD9226_SAMPLE_MASK;
    }
}

bool SignalDetect_AnalyzeFrame(const uint16_t *samples,
                               uint32_t count,
                               SignalFrameStats *stats)
{
    if (stats == NULL)
    {
        return false;
    }

    memset(stats, 0, sizeof(*stats));
    if ((samples == NULL)
        || (count != SIGNAL_DETECT_EXPECTED_FRAME_SAMPLES))
    {
        return false;
    }

    uint64_t sum = 0U;
    uint64_t sum_squares = 0U;
    uint16_t raw_min = AD9226_SAMPLE_MASK;
    uint16_t raw_max = 0U;
    uint16_t previous = AD9226_RawToCode(samples[0]);
    uint32_t current_run = 1U;
    uint32_t longest_run = 1U;

    for (uint32_t i = 0U; i < count; ++i)
    {
        uint16_t code = AD9226_RawToCode(samples[i]);

        if (code < raw_min)
        {
            raw_min = code;
        }
        if (code > raw_max)
        {
            raw_max = code;
        }

        sum += code;
        sum_squares += (uint64_t)code * (uint64_t)code;

        if ((code <= SIGNAL_DETECT_CLIP_LOW_CODE)
            || (code >= SIGNAL_DETECT_CLIP_HIGH_CODE))
        {
            ++stats->clip_count;
        }
        if (code == 0U)
        {
            ++stats->zero_count;
        }

        if (i != 0U)
        {
            if (code == previous)
            {
                ++stats->repeated_sample_count;
                ++current_run;
                if (current_run > longest_run)
                {
                    longest_run = current_run;
                }
            }
            else
            {
                current_run = 1U;
            }
        }
        previous = code;
    }

    float count_f = (float)count;
    float mean = (float)sum / count_f;
    float mean_square = (float)sum_squares / count_f;
    float variance = mean_square - (mean * mean);
    if (variance < 0.0f)
    {
        variance = 0.0f;
    }

    stats->sample_count = count;
    stats->raw_min = raw_min;
    stats->raw_max = raw_max;
    stats->raw_mean = mean;
    stats->raw_rms = sqrtf(mean_square);
    stats->ac_rms = sqrtf(variance);
    stats->longest_repeated_run = longest_run;
    stats->valid = true;
    return true;
}

void SignalDetect_AccumulateFrame(uint32_t rate_index,
                                  const SignalFrameStats *stats)
{
    if ((rate_index >= SIGNAL_DETECT_GATE_RATE_COUNT)
        || (stats == NULL)
        || !stats->valid)
    {
        return;
    }

    volatile SignalRateGateResult *rate =
        &g_signal_acquisition_gate_report.rates[rate_index];
    uint32_t previous_valid_frames = rate->valid_frames;
    uint32_t new_valid_frames = previous_valid_frames + 1U;

    if (stats->raw_min < rate->observed_min)
    {
        rate->observed_min = stats->raw_min;
    }
    if (stats->raw_max > rate->observed_max)
    {
        rate->observed_max = stats->raw_max;
    }

    float divisor = (float)new_valid_frames;
    rate->mean_of_frame_means +=
        (stats->raw_mean - rate->mean_of_frame_means) / divisor;
    rate->mean_raw_rms +=
        (stats->raw_rms - rate->mean_raw_rms) / divisor;
    rate->mean_ac_rms +=
        (stats->ac_rms - rate->mean_ac_rms) / divisor;

    rate->total_clip_samples += stats->clip_count;
    rate->total_repeated_samples += stats->repeated_sample_count;
    if (stats->longest_repeated_run > rate->longest_repeated_run)
    {
        rate->longest_repeated_run = stats->longest_repeated_run;
    }

    rate->valid_frames = new_valid_frames;
    g_signal_acquisition_gate_report.last_frame = *stats;
}

void SignalDetect_FinalizeRate(uint32_t rate_index)
{
    if (rate_index >= SIGNAL_DETECT_GATE_RATE_COUNT)
    {
        return;
    }

    volatile SignalRateGateResult *rate =
        &g_signal_acquisition_gate_report.rates[rate_index];

    if (rate->valid_frames == 0U)
    {
        rate->observed_min = 0U;
    }

    rate->passed =
        (rate->attempted_frames == rate->target_frames)
        && (rate->valid_frames == rate->target_frames)
        && (rate->sample_rate_errors == 0U)
        && (rate->start_errors == 0U)
        && (rate->wait_timeouts == 0U)
        && (rate->acquire_errors == 0U)
        && (rate->stop_errors == 0U)
        && (rate->frame_errors == 0U)
        && (rate->overwritten_frames == 0U)
        && (rate->notification_overflows == 0U)
        && (rate->dma_errors == 0U);

    g_signal_acquisition_gate_report.total_attempted_frames +=
        rate->attempted_frames;
    g_signal_acquisition_gate_report.total_valid_frames +=
        rate->valid_frames;
    if (rate->passed != 0U)
    {
        ++g_signal_acquisition_gate_report.passed_rate_count;
    }
}

void SignalDetect_FinalizeGate(uint32_t elapsed_ms)
{
    g_signal_acquisition_gate_report.elapsed_ms = elapsed_ms;
    g_signal_acquisition_gate_report.complete = 1U;
}
