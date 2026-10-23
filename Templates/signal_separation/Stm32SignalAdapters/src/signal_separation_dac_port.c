#include "signal_separation_dac_port.h"

#include "app_dac_output.h"
#include "app_dac_output_config.h"
#include "app_config_contracts.h"
#include "dac.h"
#include "tim.h"

__attribute__((section(".dma_buffer"), aligned(32)))
static uint32_t s_signal_separation_dac_buffer[
    APP_DAC_OUTPUT_BUFFER_POINTS];

bool SignalSeparationDacPort_Init(void)
{
    const AppDacOutputPort port = {
        .dac = &hdac1,
        .trigger_timer = &htim6,
        .dac_channel = DAC_CHANNEL_1,
        .dma_lisr = &DMA2->LISR,
        .buffer = s_signal_separation_dac_buffer,
        .buffer_points = APP_DAC_OUTPUT_BUFFER_POINTS,
        .dma_memory_start = APP_CONFIG_RAM_D2_START,
        .dma_memory_end = APP_CONFIG_RAM_D2_END,
        .timer_kernel_clock_hz = APP_CONFIG_TIM6_KERNEL_CLOCK_HZ,
    };
    return AppDacOutput_ConfigurePort(&port);
}
