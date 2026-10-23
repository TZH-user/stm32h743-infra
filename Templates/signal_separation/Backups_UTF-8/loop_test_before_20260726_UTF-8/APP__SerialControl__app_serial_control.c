#include "app_serial_control.h"

#include "app_calibration_service.h"
#include "app_main_task.h"
#include "app_output_task.h"
#include "app_phase_lock_task.h"
#include "app_rtos_diagnostics.h"
#include "app_ui_remote_control.h"
#include "cmsis_os2.h"
#include "debug_io.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#define APP_SERIAL_RX_DMA_SIZE          256U
#define APP_SERIAL_RX_QUEUE_DEPTH       8U
#define APP_SERIAL_COMMAND_MAX_LENGTH   128U
#define APP_SERIAL_DCACHE_LINE_SIZE     32U
#define APP_SERIAL_RX_RETRY_MS          100U

typedef struct
{
    uint8_t data[APP_SERIAL_RX_DMA_SIZE];
    uint16_t length;
} AppSerialRxChunk;

__attribute__((section(".dma_buffer.uart_rx"),
               aligned(APP_SERIAL_DCACHE_LINE_SIZE)))
static AppSerialRxChunk s_rx_dma_chunk;

static UART_HandleTypeDef *s_control_uart = NULL;
static osMessageQueueId_t s_rx_queue = NULL;
static volatile bool s_rx_active = false;
static volatile AppSerialControlStats s_serial_stats;

static void app_serial_cache_invalidate(void *address, uint32_t length)
{
#if (__DCACHE_PRESENT == 1U)
    uintptr_t start = (uintptr_t)address
        & ~((uintptr_t)APP_SERIAL_DCACHE_LINE_SIZE - 1U);
    uintptr_t end = ((uintptr_t)address + length
                     + APP_SERIAL_DCACHE_LINE_SIZE - 1U)
        & ~((uintptr_t)APP_SERIAL_DCACHE_LINE_SIZE - 1U);

    SCB_InvalidateDCache_by_Addr((uint32_t *)start,
                                (int32_t)(end - start));
    __DSB();
    __ISB();
#else
    (void)address;
    (void)length;
#endif
}

static bool app_serial_start_rx(void)
{
    if ((s_control_uart == NULL)
        || (s_control_uart->hdmarx == NULL)
        || (s_rx_queue == NULL))
    {
        return false;
    }

    app_serial_cache_invalidate(s_rx_dma_chunk.data,
                                sizeof(s_rx_dma_chunk.data));

    if (HAL_UARTEx_ReceiveToIdle_DMA(s_control_uart,
                                     s_rx_dma_chunk.data,
                                     sizeof(s_rx_dma_chunk.data)) != HAL_OK)
    {
        s_rx_active = false;
        ++s_serial_stats.rx_restart_failures;
        return false;
    }

    __HAL_DMA_DISABLE_IT(s_control_uart->hdmarx, DMA_IT_HT);
    s_rx_active = true;
    return true;
}

static char *app_serial_next_token(char **cursor)
{
    if ((cursor == NULL) || (*cursor == NULL))
    {
        return NULL;
    }

    char *token = *cursor;
    while ((*token == ' ') || (*token == '\t'))
    {
        ++token;
    }
    if (*token == '\0')
    {
        *cursor = token;
        return NULL;
    }

    char *end = token;
    while ((*end != '\0') && (*end != ' ') && (*end != '\t'))
    {
        ++end;
    }
    if (*end != '\0')
    {
        *end = '\0';
        ++end;
    }
    *cursor = end;
    return token;
}

static void app_serial_reply_help(void)
{
    (void)AppOutput_Logf("[CTRL] HELP STATUS RTOS OUTPUT <LOG|OFF|ADC|FFT>\r\n");
    (void)AppOutput_Logf("[CTRL] UI TAP <RUN|STOP|PHASE+|PHASE->\r\n");
    (void)AppOutput_Logf("[CTRL] UI PAGE <MONITOR|SPECTRUM|RESULTS|CALIBRATION|SYSTEM>\r\n");
    (void)AppOutput_Logf("[CTRL] UI STATUS; use OUTPUT LOG before text queries\r\n");
}

static void app_serial_reply_status(void)
{
    AppSerialControlStats serial_stats;
    AppUiRemoteStats ui_stats;
    AppCalibrationSnapshot calibration;

    AppSerialControl_GetStats(&serial_stats);
    AppUiRemoteControl_GetStats(&ui_stats);
    AppCalibrationService_GetSnapshot(&calibration);

    (void)AppOutput_Logf(
        "[CTRL] STATUS tick=%lu main=%u phase=%u output=%s requested=%s\r\n",
        (unsigned long)osKernelGetTickCount(),
        AppMain_IsRunning() ? 1U : 0U,
        AppPhaseLockTask_IsRunning() ? 1U : 0U,
        AppOutput_ModeString(AppOutput_GetMode()),
        AppOutput_ModeString(AppOutput_GetRequestedMode()));
    (void)AppOutput_Logf(
        "[CTRL] CAL state=%u error=%u exclusive=%u progress=%u qspi=%u\r\n",
        (unsigned int)calibration.state,
        (unsigned int)calibration.error,
        calibration.exclusive ? 1U : 0U,
        (unsigned int)calibration.progress_percent,
        calibration.qspi_ready ? 1U : 0U);
    (void)AppOutput_Logf(
        "[CTRL] RX bytes=%lu chunks=%lu dropped=%lu lines=%lu overflow=%lu errors=%lu\r\n",
        (unsigned long)serial_stats.received_bytes,
        (unsigned long)serial_stats.received_chunks,
        (unsigned long)serial_stats.dropped_chunks,
        (unsigned long)serial_stats.completed_lines,
        (unsigned long)serial_stats.overflow_lines,
        (unsigned long)serial_stats.uart_errors);
    (void)AppOutput_Logf(
        "[CTRL] UI queued=%lu executed=%lu rejected=%lu dropped=%lu last=%s result=%u\r\n",
        (unsigned long)ui_stats.queued_count,
        (unsigned long)ui_stats.executed_count,
        (unsigned long)ui_stats.rejected_count,
        (unsigned long)ui_stats.dropped_count,
        AppUiRemoteControl_ActionName(ui_stats.last_action),
        ui_stats.last_result ? 1U : 0U);
}

static void app_serial_reply_rtos(void)
{
    const volatile AppRtosDiagnosticsSnapshot *snapshot =
        &g_app_rtos_diagnostics;

    (void)AppOutput_Logf(
        "[CTRL] RTOS tick=%lu tasks=%lu heap=%lu min_heap=%lu\r\n",
        (unsigned long)snapshot->tick_count,
        (unsigned long)snapshot->scheduler_task_count,
        (unsigned long)snapshot->free_heap_bytes,
        (unsigned long)snapshot->min_ever_free_heap_bytes);
    (void)AppOutput_Logf(
        "[CTRL] STACK display=%lu touch=%lu printf=%lu main=%lu phase=%lu\r\n",
        (unsigned long)snapshot->display_stack_min_free_words,
        (unsigned long)snapshot->touch_stack_min_free_words,
        (unsigned long)snapshot->printf_stack_min_free_words,
        (unsigned long)snapshot->main_stack_min_free_words,
        (unsigned long)snapshot->phase_lock_stack_min_free_words);
    (void)AppOutput_Logf(
        "[CTRL] VCP queued=%lu peak=%lu rejected_bytes=%lu\r\n",
        (unsigned long)DebugIO_GetVcpQueuedBytes(),
        (unsigned long)DebugIO_GetVcpPeakQueuedBytes(),
        (unsigned long)DebugIO_GetVcpDroppedBytes());
}

static bool app_serial_parse_output_mode(const char *token,
                                         AppOutputMode *mode)
{
    if ((token == NULL) || (mode == NULL))
    {
        return false;
    }

    if (strcmp(token, "LOG") == 0)
    {
        *mode = APP_OUTPUT_MODE_LOG;
    }
    else if (strcmp(token, "OFF") == 0)
    {
        *mode = APP_OUTPUT_MODE_OFF;
    }
    else if (strcmp(token, "ADC") == 0)
    {
        *mode = APP_OUTPUT_MODE_ADC_WAVE;
    }
    else if (strcmp(token, "FFT") == 0)
    {
        *mode = APP_OUTPUT_MODE_FFT_WAVE;
    }
    else
    {
        return false;
    }

    return true;
}

static void app_serial_set_output_mode(const char *token)
{
    AppOutputMode mode;
    if (!app_serial_parse_output_mode(token, &mode))
    {
        (void)AppOutput_Logf("[CTRL] ERR OUTPUT MODE\r\n");
        return;
    }

    if ((mode != APP_OUTPUT_MODE_LOG)
        && (AppOutput_GetMode() == APP_OUTPUT_MODE_LOG))
    {
        (void)AppOutput_Logf("[CTRL] OK OUTPUT %s\r\n",
                            AppOutput_ModeString(mode));
    }

    if (!AppOutput_RequestMode(mode))
    {
        (void)AppOutput_Logf("[CTRL] ERR OUTPUT REJECTED\r\n");
        return;
    }

    if (mode == APP_OUTPUT_MODE_LOG)
    {
        for (uint32_t attempt = 0U; attempt < 20U; ++attempt)
        {
            if (AppOutput_GetMode() == APP_OUTPUT_MODE_LOG)
            {
                (void)AppOutput_Logf("[CTRL] OK OUTPUT log\r\n");
                return;
            }
            osDelay(10U);
        }
    }
}

static bool app_serial_parse_ui_action(const char *group,
                                       const char *target,
                                       AppUiRemoteAction *action)
{
    if ((group == NULL) || (target == NULL) || (action == NULL))
    {
        return false;
    }

    if (strcmp(group, "TAP") == 0)
    {
        if (strcmp(target, "RUN") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_RUN;
        }
        else if (strcmp(target, "STOP") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_STOP;
        }
        else if (strcmp(target, "PHASE-") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_PHASE_DECREASE;
        }
        else if (strcmp(target, "PHASE+") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_PHASE_INCREASE;
        }
        else
        {
            return false;
        }
        return true;
    }

    if (strcmp(group, "PAGE") == 0)
    {
        if (strcmp(target, "MONITOR") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_PAGE_MONITOR;
        }
        else if (strcmp(target, "SPECTRUM") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_PAGE_SPECTRUM;
        }
        else if (strcmp(target, "RESULTS") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_PAGE_RESULTS;
        }
        else if (strcmp(target, "CALIBRATION") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_PAGE_CALIBRATION;
        }
        else if (strcmp(target, "SYSTEM") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_PAGE_SYSTEM;
        }
        else
        {
            return false;
        }
        return true;
    }

    return false;
}

static void app_serial_process_ui(char **cursor)
{
    char *group = app_serial_next_token(cursor);
    if ((group != NULL) && (strcmp(group, "STATUS") == 0))
    {
        AppUiRemoteStats stats;
        AppUiRemoteControl_GetStats(&stats);
        (void)AppOutput_Logf(
            "[CTRL] UI queued=%lu executed=%lu rejected=%lu dropped=%lu last=%s result=%u\r\n",
            (unsigned long)stats.queued_count,
            (unsigned long)stats.executed_count,
            (unsigned long)stats.rejected_count,
            (unsigned long)stats.dropped_count,
            AppUiRemoteControl_ActionName(stats.last_action),
            stats.last_result ? 1U : 0U);
        return;
    }

    char *target = app_serial_next_token(cursor);
    AppUiRemoteAction action;
    if (!app_serial_parse_ui_action(group, target, &action))
    {
        (void)AppOutput_Logf("[CTRL] ERR UI FORMAT\r\n");
        return;
    }

    if (!AppUiRemoteControl_Post(action))
    {
        (void)AppOutput_Logf("[CTRL] ERR UI QUEUE FULL\r\n");
    }
}

static void app_serial_process_line(char *line)
{
    for (char *character = line; *character != '\0'; ++character)
    {
        *character = (char)toupper((unsigned char)*character);
    }

    char *cursor = line;
    char *command = app_serial_next_token(&cursor);
    if (command == NULL)
    {
        return;
    }

    if (strcmp(command, "HELP") == 0)
    {
        app_serial_reply_help();
    }
    else if (strcmp(command, "PING") == 0)
    {
        (void)AppOutput_Logf("[CTRL] PONG\r\n");
    }
    else if (strcmp(command, "STATUS") == 0)
    {
        app_serial_reply_status();
    }
    else if (strcmp(command, "RTOS") == 0)
    {
        app_serial_reply_rtos();
    }
    else if (strcmp(command, "OUTPUT") == 0)
    {
        char *mode = app_serial_next_token(&cursor);
        if (mode == NULL)
        {
            (void)AppOutput_Logf("[CTRL] OUTPUT active=%s requested=%s\r\n",
                                AppOutput_ModeString(AppOutput_GetMode()),
                                AppOutput_ModeString(
                                    AppOutput_GetRequestedMode()));
        }
        else
        {
            app_serial_set_output_mode(mode);
        }
    }
    else if (strcmp(command, "UI") == 0)
    {
        app_serial_process_ui(&cursor);
    }
    else
    {
        ++s_serial_stats.unknown_commands;
        (void)AppOutput_Logf("[CTRL] ERR UNKNOWN COMMAND\r\n");
    }
}

bool AppSerialControl_Init(UART_HandleTypeDef *uart)
{
    if (uart == NULL)
    {
        return false;
    }

    s_control_uart = uart;
    if (s_rx_queue != NULL)
    {
        return true;
    }

    static const osMessageQueueAttr_t attributes = {
        .name = "SerialRxQueue"
    };
    s_rx_queue = osMessageQueueNew(APP_SERIAL_RX_QUEUE_DEPTH,
                                   sizeof(AppSerialRxChunk),
                                   &attributes);
    return s_rx_queue != NULL;
}

void StartSerialControlTask(void *argument)
{
    (void)argument;

    char line[APP_SERIAL_COMMAND_MAX_LENGTH];
    uint32_t line_length = 0U;
    bool discard_line = false;
    (void)app_serial_start_rx();
    (void)AppOutput_Logf("[CTRL] READY; send HELP\r\n");

    for (;;)
    {
        AppSerialRxChunk chunk;
        osStatus_t status = osMessageQueueGet(s_rx_queue,
                                              &chunk,
                                              NULL,
                                              APP_SERIAL_RX_RETRY_MS);
        if (status == osOK)
        {
            for (uint32_t index = 0U; index < chunk.length; ++index)
            {
                uint8_t byte = chunk.data[index];
                if ((byte == '\r') || (byte == '\n'))
                {
                    if (discard_line)
                    {
                        discard_line = false;
                        line_length = 0U;
                        continue;
                    }

                    if (line_length > 0U)
                    {
                        line[line_length] = '\0';
                        ++s_serial_stats.completed_lines;
                        app_serial_process_line(line);
                        line_length = 0U;
                    }
                    continue;
                }

                if (discard_line)
                {
                    continue;
                }

                if ((byte < 0x20U) || (byte > 0x7EU))
                {
                    continue;
                }

                if (line_length >= (sizeof(line) - 1U))
                {
                    discard_line = true;
                    line_length = 0U;
                    ++s_serial_stats.overflow_lines;
                    continue;
                }

                line[line_length++] = (char)byte;
            }
        }

        if (!s_rx_active)
        {
            (void)app_serial_start_rx();
        }
    }
}

void AppSerialControl_OnRxEvent(UART_HandleTypeDef *uart, uint16_t size)
{
    if ((uart == NULL) || (uart != s_control_uart))
    {
        return;
    }

    s_rx_active = false;
    if (size > APP_SERIAL_RX_DMA_SIZE)
    {
        size = APP_SERIAL_RX_DMA_SIZE;
    }

    if (size > 0U)
    {
        app_serial_cache_invalidate(s_rx_dma_chunk.data, size);
        s_rx_dma_chunk.length = size;
        s_serial_stats.received_bytes += size;
        ++s_serial_stats.received_chunks;

        if ((s_rx_queue == NULL)
            || (osMessageQueuePut(s_rx_queue,
                                  &s_rx_dma_chunk,
                                  0U,
                                  0U) != osOK))
        {
            ++s_serial_stats.dropped_chunks;
        }
    }

    (void)app_serial_start_rx();
}

void AppSerialControl_OnUartError(UART_HandleTypeDef *uart)
{
    if ((uart == NULL) || (uart != s_control_uart))
    {
        return;
    }

    s_rx_active = false;
    ++s_serial_stats.uart_errors;
    (void)app_serial_start_rx();
}

void AppSerialControl_GetStats(AppSerialControlStats *stats)
{
    if (stats == NULL)
    {
        return;
    }

    __DMB();
    stats->received_bytes = s_serial_stats.received_bytes;
    stats->received_chunks = s_serial_stats.received_chunks;
    stats->dropped_chunks = s_serial_stats.dropped_chunks;
    stats->completed_lines = s_serial_stats.completed_lines;
    stats->overflow_lines = s_serial_stats.overflow_lines;
    stats->unknown_commands = s_serial_stats.unknown_commands;
    stats->uart_errors = s_serial_stats.uart_errors;
    stats->rx_restart_failures = s_serial_stats.rx_restart_failures;
}
