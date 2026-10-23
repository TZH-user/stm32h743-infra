#ifndef APP_SIGNAL_DETECT_SIGNAL_FFT_H_
#define APP_SIGNAL_DETECT_SIGNAL_FFT_H_

#include <stdbool.h>
#include <stdint.h>

#include "app_analysis_config.h"
#include "signal_detect_cfg.h"

#define SIGNAL_FFT_MAX_GRID_CANDIDATES  100U
#define SIGNAL_FFT_SPECTRUM_BINS        \
    ((SIGNAL_PREPROCESS_SAMPLE_COUNT / 2U) + 1U)

/* 一次 FFT 的可直接输入参数（UTF-8）。 */
typedef struct
{
    uint32_t sample_rate_hz;
    uint32_t min_frequency_hz;
    uint32_t max_frequency_hz;
    uint32_t candidate_step_hz;
    float minimum_snr_db;
    float minimum_amplitude_code;
    uint32_t max_selected_candidates;
} SignalFftConfig;

typedef struct
{
    float nominal_frequency_hz;
    float measured_frequency_hz;
    uint32_t center_bin;
    uint32_t peak_bin;
    float local_energy;
    float amplitude_code;
    float snr_db;
    uint32_t rank;
    bool active;
} SignalFrequencyCandidate;

typedef struct
{
    uint32_t sample_rate_hz;
    uint32_t fft_size;
    float frequency_resolution_hz;
    uint32_t min_frequency_hz;
    uint32_t max_frequency_hz;
    uint32_t candidate_step_hz;
    uint32_t candidate_count;
    uint32_t active_candidate_count;
    uint32_t selected_candidate_count;
    float noise_floor_power;
    float noise_floor_amplitude_code;
    uint32_t full_spectrum_peak_bin;
    float full_spectrum_peak_frequency_hz;
    float full_spectrum_peak_amplitude_code;
    uint32_t transform_time_us;
    uint32_t candidate_time_us;
    uint32_t processing_time_us;
    bool full_spectrum_peak_in_search_range;
    bool initialized;
    bool valid;
    SignalFrequencyCandidate
        selected[APP_ANALYSIS_MAX_SELECTED_CANDIDATES];
} SignalFftResult;

typedef struct
{
    uint32_t initialization_count;
    uint32_t run_count;
    uint32_t successful_runs;
    uint32_t failed_runs;
    uint32_t last_processing_time_us;
} SignalFftDiagnostics;

/* 功率谱和完整候选表位于 AXI RAM，供后续串口/UI适配器只读使用。 */
extern float g_signal_fft_power[SIGNAL_FFT_SPECTRUM_BINS];
extern SignalFrequencyCandidate
    g_signal_fft_candidates[SIGNAL_FFT_MAX_GRID_CANDIDATES];
extern SignalFftResult g_signal_fft_result;
extern volatile SignalFftDiagnostics g_signal_fft_diagnostics;

bool SignalFft_Init(void);
void SignalFft_LoadDefaultConfig(SignalFftConfig *config,
                                uint32_t sample_rate_hz);

/*
 * 执行一次正式 4096 点 RFFT。CMSIS-DSP 会修改 windowed_input；
 * 未加窗数据和原始 ADC 帧不受影响。
 */
bool SignalFft_Run(float *windowed_input,
                   const SignalFftConfig *config,
                   SignalFftResult *result);

void SignalFft_ResetDiagnostics(void);
void SignalFft_RecordDiagnostics(bool successful,
                                uint32_t processing_time_us);

#endif /* APP_SIGNAL_DETECT_SIGNAL_FFT_H_ */
