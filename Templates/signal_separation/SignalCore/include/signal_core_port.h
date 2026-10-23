#ifndef SIGNAL_CORE_PORT_H_
#define SIGNAL_CORE_PORT_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal platform boundary used by the reusable signal algorithms.
 * The core does not include an ADC driver or an RTOS header directly.
 */
typedef struct
{
    uint16_t (*raw_to_code)(uint16_t raw_sample);
    uint32_t (*timer_count)(void);
    uint32_t (*timer_frequency_hz)(void);
} SignalCorePort;

bool SignalCorePort_Configure(const SignalCorePort *port);
bool SignalCorePort_IsConfigured(void);
uint16_t SignalCorePort_RawToCode(uint16_t raw_sample);
uint32_t SignalCorePort_TimerCount(void);
uint32_t SignalCorePort_TimerFrequencyHz(void);
uint32_t SignalCorePort_ElapsedUs(uint32_t start_count,
                                  uint32_t end_count);

#ifdef __cplusplus
}
#endif

#endif /* SIGNAL_CORE_PORT_H_ */
