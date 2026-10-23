#include "signal_core_stm32_adapter.h"

#include "ad9226.h"
#include "cmsis_os2.h"
#include "signal_core_port.h"

static uint16_t signal_core_stm32_raw_to_code(uint16_t raw_sample)
{
    return AD9226_RawToCode(raw_sample);
}

static uint32_t signal_core_stm32_timer_count(void)
{
    return osKernelGetSysTimerCount();
}

static uint32_t signal_core_stm32_timer_frequency_hz(void)
{
    return osKernelGetSysTimerFreq();
}

bool SignalCoreStm32Adapter_Init(void)
{
    const SignalCorePort port = {
        .raw_to_code = signal_core_stm32_raw_to_code,
        .timer_count = signal_core_stm32_timer_count,
        .timer_frequency_hz = signal_core_stm32_timer_frequency_hz,
    };
    return SignalCorePort_Configure(&port);
}
