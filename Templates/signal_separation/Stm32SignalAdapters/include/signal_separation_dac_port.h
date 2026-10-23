#ifndef STM32_SIGNAL_ADAPTERS_SIGNAL_SEPARATION_DAC_PORT_H_
#define STM32_SIGNAL_ADAPTERS_SIGNAL_SEPARATION_DAC_PORT_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bind this project's DAC1/TIM6/DMA2 resources and RAM_D2 buffer. */
bool SignalSeparationDacPort_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32_SIGNAL_ADAPTERS_SIGNAL_SEPARATION_DAC_PORT_H_ */
