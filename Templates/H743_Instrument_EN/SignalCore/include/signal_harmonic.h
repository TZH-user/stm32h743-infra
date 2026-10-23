#ifndef APP_SIGNAL_DETECT_SIGNAL_HARMONIC_H_
#define APP_SIGNAL_DETECT_SIGNAL_HARMONIC_H_

#include <stdbool.h>
#include <stdint.h>

#include "signal_dual_sine.h"
#include "signal_fft.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIGNAL_HARMONIC_MAX_COMPONENTS  2U

typedef enum
{
    SIGNAL_WAVE_UNKNOWN = 0,
    SIGNAL_WAVE_SINE,
    SIGNAL_WAVE_TRIANGLE
} SignalWaveType;

/* 单个基频的谐波幅值与分类分数（UTF-8）。 */
typedef struct
{
    float a1;
    float a2;
    float a3;
    float a4;
    float a5;
    float a7;
    float odd_energy;
    float even_energy;
    float thd;
    float triangle_score;
    float sine_score;
} SignalHarmonicFeature;

typedef struct
{
    float frequency_hz;
    SignalWaveType type;
    float confidence;
    SignalHarmonicFeature feature;
} SignalHarmonicComponent;

typedef struct
{
    uint32_t sample_rate_hz;
    uint32_t sample_count;
    uint32_t component_count;
    uint32_t processing_time_us;
    bool valid;
    SignalHarmonicComponent components[SIGNAL_HARMONIC_MAX_COMPONENTS];
} SignalHarmonicResult;

extern volatile SignalHarmonicResult g_signal_harmonic_result;

void SignalHarmonic_ResetResult(void);

bool SignalHarmonic_Analyze(const float *unwindowed_samples,
                            uint32_t sample_count,
                            const SignalFftResult *fft_result,
                            const SignalDualSineResult *dual_result,
                            SignalHarmonicResult *result);

void SignalHarmonic_PublishResult(const SignalHarmonicResult *result,
                                  uint32_t processing_time_us);

const char *SignalHarmonic_WaveTypeString(SignalWaveType type);

#ifdef __cplusplus
}
#endif

#endif /* APP_SIGNAL_DETECT_SIGNAL_HARMONIC_H_ */
