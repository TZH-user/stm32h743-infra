#ifndef APP_CONFIG_APP_CONFIG_CONTRACTS_H_
#define APP_CONFIG_APP_CONFIG_CONTRACTS_H_

#include <stdint.h>

#include "ad9226_config.h"
#include "app_analysis_config.h"
#include "app_dac_output_config.h"
#include "app_output_config.h"
#include "app_phase_lock_config.h"
#include "signal_detect_cfg.h"

/* Project-owned hardware constants used by runtime adapter validation. */
#define APP_CONFIG_TIM6_KERNEL_CLOCK_HZ  240000000UL
#define APP_CONFIG_RAM_D2_START          0x30000000UL
#define APP_CONFIG_RAM_D2_END            0x30048000UL

_Static_assert(AD9226_FRAME_SAMPLES == SIGNAL_PREPROCESS_SAMPLE_COUNT,
               "AD9226 frame size must match SignalCore preprocessing");
_Static_assert(APP_ANALYSIS_DEFAULT_FFT_SIZE
                   == SIGNAL_PREPROCESS_SAMPLE_COUNT,
               "Configured FFT size must match preprocessing frame size");
_Static_assert(APP_PHASE_LOCK_SAMPLE_RATE_HZ <= AD9226_MAX_SAMPLE_RATE_HZ,
               "Phase-lock sample rate exceeds AD9226 driver limit");
_Static_assert(APP_PHASE_LOCK_MIN_OUTPUT_FREQUENCY_HZ
                   == APP_DAC_OUTPUT_MIN_FREQUENCY_HZ,
               "Phase-lock and DAC minimum frequencies differ");
_Static_assert(APP_PHASE_LOCK_MAX_OUTPUT_FREQUENCY_HZ
                   == APP_DAC_OUTPUT_MAX_FREQUENCY_HZ,
               "Phase-lock and DAC maximum frequencies differ");
_Static_assert((APP_DAC_OUTPUT_BUFFER_POINTS % 2U) == 0U,
               "DAC DMA buffer must have two equal halves");
_Static_assert(APP_DAC_OUTPUT_BUFFER_POINTS <= 65535U,
               "DAC buffer exceeds DMA NDTR range");
_Static_assert(APP_DAC_OUTPUT_PHASE_MIN_DEG == 0U,
               "Output phase range must start at zero degrees");
_Static_assert(APP_DAC_OUTPUT_PHASE_MAX_DEG == 180U,
               "Output phase range must end at 180 degrees");
_Static_assert(APP_DAC_OUTPUT_PHASE_STEP_DEG == 5U,
               "Output phase resolution must remain five degrees");
_Static_assert(((APP_DAC_OUTPUT_PHASE_MAX_DEG
                 - APP_DAC_OUTPUT_PHASE_MIN_DEG)
                % APP_DAC_OUTPUT_PHASE_STEP_DEG) == 0U,
               "Output phase range is not divisible by its step");
_Static_assert((APP_CONFIG_TIM6_KERNEL_CLOCK_HZ
                % APP_DAC_OUTPUT_SAMPLE_RATE_HZ) == 0U,
               "TIM6 kernel clock cannot generate the configured DAC rate");
_Static_assert((APP_CONFIG_TIM6_KERNEL_CLOCK_HZ
                / APP_DAC_OUTPUT_SAMPLE_RATE_HZ) == 150U,
               "TIM6 PSC/ARR contract changed; verify CubeMX configuration");
_Static_assert((((uint32_t)APP_OUTPUT_STREAM_QUEUE_DEPTH - 1U)
                * (uint32_t)APP_OUTPUT_STREAM_CHUNK_POINTS)
                   >= AD9226_FRAME_SAMPLES,
               "Output queue cannot hold one complete ADC frame");

#endif /* APP_CONFIG_APP_CONFIG_CONTRACTS_H_ */
