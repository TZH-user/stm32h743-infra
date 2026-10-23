#include "signal_core_port.h"

#include <stddef.h>

static SignalCorePort s_signal_core_port;
static bool s_signal_core_port_configured;

bool SignalCorePort_Configure(const SignalCorePort *port)
{
    if ((port == NULL)
        || (port->raw_to_code == NULL)
        || (port->timer_count == NULL)
        || (port->timer_frequency_hz == NULL)
        || (port->timer_frequency_hz() == 0U))
    {
        return false;
    }

    s_signal_core_port = *port;
    s_signal_core_port_configured = true;
    return true;
}

bool SignalCorePort_IsConfigured(void)
{
    return s_signal_core_port_configured;
}

uint16_t SignalCorePort_RawToCode(uint16_t raw_sample)
{
    if (!s_signal_core_port_configured)
    {
        return 0U;
    }
    return s_signal_core_port.raw_to_code(raw_sample);
}

uint32_t SignalCorePort_TimerCount(void)
{
    if (!s_signal_core_port_configured)
    {
        return 0U;
    }
    return s_signal_core_port.timer_count();
}

uint32_t SignalCorePort_TimerFrequencyHz(void)
{
    if (!s_signal_core_port_configured)
    {
        return 0U;
    }
    return s_signal_core_port.timer_frequency_hz();
}

uint32_t SignalCorePort_ElapsedUs(uint32_t start_count,
                                  uint32_t end_count)
{
    uint32_t frequency_hz = SignalCorePort_TimerFrequencyHz();
    if (frequency_hz == 0U)
    {
        return 0U;
    }

    return (uint32_t)(((uint64_t)(end_count - start_count) * 1000000ULL)
                      / frequency_hz);
}
