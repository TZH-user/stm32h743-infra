#ifndef APP_SIGNAL_DETECT_SIGNAL_DUAL_SINE_H_
#define APP_SIGNAL_DETECT_SIGNAL_DUAL_SINE_H_

#include <stdbool.h>
#include <stdint.h>

#include "signal_fft.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIGNAL_DUAL_SINE_TONE_COUNT  2U

/* 单个候选频率的同相、正交相关结果（UTF-8）。 */
typedef struct
{
    float i;
    float q;
    float amplitude;
    float phase_rad;
} SignalComplexEstimate;

typedef struct
{
    float frequency_hz;
    SignalComplexEstimate estimate;
    uint32_t source_candidate_rank;
} SignalDualSineTone;

/* 同一帧双正弦模型及重构质量（UTF-8）。 */
typedef struct
{
    uint32_t sample_rate_hz;
    uint32_t sample_count;
    uint32_t evaluated_pairs;
    SignalDualSineTone tones[SIGNAL_DUAL_SINE_TONE_COUNT];
    float input_rms_code;
    float model_rms_code;
    float residual_rms_code;
    float normalized_residual;
    float relative_phase_rad;
    uint32_t processing_time_us;
    bool valid;
} SignalDualSineResult;

extern volatile SignalDualSineResult g_signal_dual_sine_result;

void SignalDualSine_ResetResult(void);

bool SignalDualSine_Analyze(const float *unwindowed_samples,
                            uint32_t sample_count,
                            const SignalFftResult *fft_result,
                            SignalDualSineResult *result);

void SignalDualSine_PublishResult(const SignalDualSineResult *result,
                                  uint32_t processing_time_us);

#ifdef __cplusplus
}
#endif

#endif /* APP_SIGNAL_DETECT_SIGNAL_DUAL_SINE_H_ */
