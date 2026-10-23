/**
  ******************************************************************************
  * @file    app_output_task.c
  * @brief   VCP single-owner task, mode switching and log queue core.
  ******************************************************************************
  */

#include "app_output_task.h"
#include "app_output_task_internal.h"

#include "Types/RtosTypes.h"
#include "app_output_config.h"
#include "app_rtos_diagnostics.h"
#include "cmsis_os2.h"
#include "debug_io.h"

#include <stdarg.h>
#include <stdio.h>

#if (APP_OUTPUT_DEFAULT_MODE > 3U)
#error "APP_OUTPUT_DEFAULT_MODE is invalid"
#endif

#if (APP_OUTPUT_LOG_MESSAGE_BYTES != 128U)
#error "PrintQueue is configured for 128-byte elements"
#endif

_Static_assert(sizeof(AppOutputLogMessage) == APP_OUTPUT_LOG_MESSAGE_BYTES,
               "AppOutputLogMessage must match the CubeMX PrintQueue item size");

AppOutputContext g_app_output_context = {
    .requested_mode = (AppOutputMode)APP_OUTPUT_DEFAULT_MODE,
    .active_mode = (AppOutputMode)APP_OUTPUT_DEFAULT_MODE,
    .stream_build_mode = APP_OUTPUT_MODE_OFF,
};

uint32_t AppOutputInternal_IrqSave(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

void AppOutputInternal_IrqRestore(uint32_t primask)
{
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }
}

static bool app_output_mode_is_valid(AppOutputMode mode)
{
    return (mode >= APP_OUTPUT_MODE_OFF)
        && (mode <= APP_OUTPUT_MODE_FFT_WAVE);
}
static void app_output_discard_log_queue(void)
{
    AppOutputLogMessage message;

    while (osMessageQueueGet(PrintQueueHandle,
                             &message,
                             NULL,
                             0U) == osOK)
    {
        /* Deliberately discard stale text when a binary-style stream is active. */
    }
}

static bool app_output_wait_transport_idle(uint32_t timeout_ms)
{
    uint32_t start = osKernelGetTickCount();

    while (!DebugIO_IsVcpIdle())
    {
        if ((uint32_t)(osKernelGetTickCount() - start) >= timeout_ms)
        {
            return false;
        }
        osDelay(1U);
    }

    return true;
}

static void app_output_apply_requested_mode(void)
{
    AppOutputMode target = g_app_output_context.requested_mode;
    if ((target == g_app_output_context.active_mode) || !app_output_mode_is_valid(target))
    {
        return;
    }

    /* OFF blocks all producers while the old transport data is draining. */
    g_app_output_context.active_mode = APP_OUTPUT_MODE_OFF;
    __DMB();

    app_output_discard_log_queue();
    AppOutputWave_Reset();

    if (!app_output_wait_transport_idle(APP_OUTPUT_MODE_SWITCH_TIMEOUT_MS))
    {
        ++g_app_output_context.transport_errors;
        return;
    }

    /* Apply the most recent request in case it changed during the drain. */
    target = g_app_output_context.requested_mode;
    g_app_output_context.active_mode = target;
    ++g_app_output_context.mode_switches;
    __DMB();
}

int AppOutputInternal_WriteWait(const uint8_t *data, uint16_t length)
{
    uint32_t start = osKernelGetTickCount();

    for (;;)
    {
        int result = vcp_write(data, length);
        if (result >= 0)
        {
            return result;
        }

        if (result != -2)
        {
            ++g_app_output_context.transport_errors;
            return result;
        }

        if ((uint32_t)(osKernelGetTickCount() - start)
            >= APP_OUTPUT_MODE_SWITCH_TIMEOUT_MS)
        {
            ++g_app_output_context.transport_errors;
            return -3;
        }

        ++g_app_output_context.transport_retries;
        osDelay(1U);
    }
}
void StartPrintfTask(void *argument)
{
    (void)argument;

    AppOutputLogMessage message;

    /*
     * main.c may have queued a short startup banner before the scheduler. Do
     * not append FireWater samples until those bytes have left the DMA ring.
     */
    if (!app_output_wait_transport_idle(APP_OUTPUT_MODE_SWITCH_TIMEOUT_MS))
    {
        ++g_app_output_context.transport_errors;
    }

    for (;;)
    {
        AppRtosDiagnostics_Poll();
        app_output_apply_requested_mode();

        AppOutputMode active_mode = g_app_output_context.active_mode;

        if (active_mode == APP_OUTPUT_MODE_LOG)
        {
            AppOutputLogFormatter_EmitPending();
            if (osMessageQueueGet(PrintQueueHandle,
                                  &message,
                                  NULL,
                                  10U) == osOK)
            {
                (void)AppOutputInternal_WriteWait(
                    (const uint8_t *)message.data,
                    message.length
                );
            }
        }
        else if ((active_mode == APP_OUTPUT_MODE_ADC_WAVE)
                 || (active_mode == APP_OUTPUT_MODE_FFT_WAVE))
        {
            app_output_discard_log_queue();

            if (AppOutputWave_PacketAvailable())
            {
                AppOutputWave_SendOne(active_mode);
            }
            else
            {
                osDelay(APP_OUTPUT_TASK_POLL_MS);
            }
        }
        else
        {
            app_output_discard_log_queue();
            AppOutputWave_Reset();
            osDelay(10U);
        }
    }
}

bool AppOutput_RequestMode(AppOutputMode mode)
{
    if (!app_output_mode_is_valid(mode))
    {
        return false;
    }

    if ((APP_OUTPUT_ENABLE_FFT_WAVE == 0U)
        && (mode == APP_OUTPUT_MODE_FFT_WAVE))
    {
        return false;
    }

    g_app_output_context.requested_mode = mode;
    __DMB();
    return true;
}

AppOutputMode AppOutput_GetMode(void)
{
    return g_app_output_context.active_mode;
}

AppOutputMode AppOutput_GetRequestedMode(void)
{
    return g_app_output_context.requested_mode;
}

const char *AppOutput_ModeString(AppOutputMode mode)
{
    switch (mode)
    {
        case APP_OUTPUT_MODE_OFF:
            return "off";
        case APP_OUTPUT_MODE_LOG:
            return "log";
        case APP_OUTPUT_MODE_ADC_WAVE:
            return "adc-wave";
        case APP_OUTPUT_MODE_FFT_WAVE:
            return "fft-wave";
        default:
            return "invalid";
    }
}

int AppOutput_Logf(const char *format, ...)
{
    if (format == NULL)
    {
        return -1;
    }

    if (g_app_output_context.active_mode != APP_OUTPUT_MODE_LOG)
    {
        return 0;
    }

    AppOutputLogMessage message = {
        .kind = 0U,
        .reserved = 0U,
    };

    va_list args;
    va_start(args, format);
    int length = vsnprintf(message.data, sizeof(message.data), format, args);
    va_end(args);

    if (length < 0)
    {
        return length;
    }

    message.length = (length >= (int)sizeof(message.data))
        ? (uint16_t)(sizeof(message.data) - 1U)
        : (uint16_t)length;

    if ((PrintQueueHandle == NULL)
        || (osMessageQueuePut(PrintQueueHandle,
                              &message,
                              0U,
                              0U) != osOK))
    {
        ++g_app_output_context.dropped_log_messages;
        return -2;
    }

    uint32_t queued_messages = osMessageQueueGetCount(PrintQueueHandle);
    if (queued_messages > g_app_output_context.log_messages_peak)
    {
        g_app_output_context.log_messages_peak = queued_messages;
    }

    return (int)message.length;
}
void AppOutput_GetStats(AppOutputStats *stats)
{
    if (stats == NULL)
    {
        return;
    }

    uint32_t primask = AppOutputInternal_IrqSave();

    stats->requested_mode = g_app_output_context.requested_mode;
    stats->active_mode = g_app_output_context.active_mode;
    stats->mode_switches = g_app_output_context.mode_switches;
    stats->dropped_log_messages = g_app_output_context.dropped_log_messages;
    stats->log_messages_peak = g_app_output_context.log_messages_peak;
    stats->dropped_stream_packets = g_app_output_context.dropped_stream_packets;
    stats->transport_retries = g_app_output_context.transport_retries;
    stats->transport_errors = g_app_output_context.transport_errors;
    stats->stream_packets_used = AppOutputWave_UsedPacketsLocked();
    stats->stream_packets_peak = g_app_output_context.stream_packets_peak;
    stats->stream_packets_capacity = APP_OUTPUT_STREAM_QUEUE_DEPTH - 1U;
    stats->stream_packets_sent = g_app_output_context.stream_packets_sent;
    stats->stream_points_sent = g_app_output_context.stream_points_sent;

    AppOutputInternal_IrqRestore(primask);
}
