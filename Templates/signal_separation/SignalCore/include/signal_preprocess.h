#ifndef APP_SIGNAL_DETECT_SIGNAL_PREPROCESS_H_
#define APP_SIGNAL_DETECT_SIGNAL_PREPROCESS_H_

#include <stdbool.h>
#include <stdint.h>

#include "signal_detect_cfg.h"

/* 单帧 E2 预处理结果（UTF-8）。 */
typedef struct
{
    uint32_t sample_count;
    float dc_offset;
    float centered_mean;
    float rms;
    float peak;
    float clip_ratio;
    uint32_t clip_count;
    uint32_t non_finite_count;
    float hann_first;
    float hann_middle;
    float hann_last;
    float hann_sum;
    float hann_coherent_gain;
    float hann_amplitude_scale;
    uint32_t processing_time_us;
    bool hann_initialized;
    bool valid;
} SignalPreprocessInfo;

/* 多帧真机稳定性统计，仅保存标量，不复制 ADC 帧。 */
typedef struct
{
    uint32_t processed_frames;
    uint32_t valid_frames;
    uint32_t invalid_frames;
    float dc_offset_min;
    float dc_offset_max;
    float centered_mean_abs_max;
    float rms_min;
    float rms_max;
    float peak_min;
    float peak_max;
    float clip_ratio_max;
    uint32_t processing_time_min_us;
    uint32_t processing_time_max_us;
    uint32_t processing_time_last_us;
    bool passed;
} SignalPreprocessDiagnostics;

/* 后续 FFT 和残差分析直接复用这两块 AXI RAM 工作区。 */
extern float g_signal_preprocess_windowed[SIGNAL_PREPROCESS_SAMPLE_COUNT];
extern float g_signal_preprocess_unwindowed[SIGNAL_PREPROCESS_SAMPLE_COUNT];
extern volatile SignalPreprocessInfo g_signal_preprocess_info;
extern volatile SignalPreprocessDiagnostics g_signal_preprocess_diagnostics;

void SignalPreprocess_Init(void);

bool SignalPreprocess_Run(const uint16_t *raw,
                          uint32_t count,
                          float *windowed_output,
                          float *unwindowed_output,
                          SignalPreprocessInfo *info);

void SignalPreprocess_ResetDiagnostics(void);
void SignalPreprocess_RecordDiagnostics(const SignalPreprocessInfo *info,
                                        uint32_t processing_time_us);

#endif /* APP_SIGNAL_DETECT_SIGNAL_PREPROCESS_H_ */
