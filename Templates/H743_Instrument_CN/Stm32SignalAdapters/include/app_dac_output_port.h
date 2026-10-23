#ifndef STM32_SIGNAL_ADAPTERS_APP_DAC_OUTPUT_PORT_H_
#define STM32_SIGNAL_ADAPTERS_APP_DAC_OUTPUT_PORT_H_

#include <stdbool.h>
#include <stdint.h>

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    DAC_HandleTypeDef *dac;
    TIM_HandleTypeDef *trigger_timer;
    uint32_t dac_channel;
    volatile uint32_t *dma_lisr;
    uint32_t *buffer;
    uint32_t buffer_points;
    uintptr_t dma_memory_start;
    uintptr_t dma_memory_end;
    uint32_t timer_kernel_clock_hz;
} AppDacOutputPort;

/* Configure the HAL handles, DMA-visible memory and diagnostic register. */
bool AppDacOutput_ConfigurePort(const AppDacOutputPort *port);

#ifdef __cplusplus
}
#endif

#endif /* STM32_SIGNAL_ADAPTERS_APP_DAC_OUTPUT_PORT_H_ */
