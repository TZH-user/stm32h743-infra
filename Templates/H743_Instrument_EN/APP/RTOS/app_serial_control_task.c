#include "app_serial_control_task.h"

#include "app_calibration_board_port.h"
#include "app_calibration_service.h"
#include "app_main_task.h"
#include "app_output_task.h"
#include "app_phase_lock_task.h"
#include "app_rtos_diagnostics.h"
#include "app_ui_monitor.h"
#include "app_ui_remote_control.h"
#include "cmsis_os2.h"
#include "debug_io.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#define APP_SERIAL_RX_DMA_SIZE          256U
#define APP_SERIAL_RX_BUFFER_COUNT      2U
#define APP_SERIAL_COMMAND_MAX_LENGTH   128U
#define APP_SERIAL_DCACHE_LINE_SIZE     32U
#define APP_SERIAL_RX_RETRY_MS          100U

__attribute__((section(".dma_buffer.uart_rx"),
               aligned(APP_SERIAL_DCACHE_LINE_SIZE)))
static uint8_t s_rx_dma_buffers[APP_SERIAL_RX_BUFFER_COUNT]
                               [APP_SERIAL_RX_DMA_SIZE];

static UART_HandleTypeDef *s_control_uart = NULL;
static volatile uint16_t s_rx_lengths[APP_SERIAL_RX_BUFFER_COUNT];
static volatile bool s_rx_ready[APP_SERIAL_RX_BUFFER_COUNT];
static volatile uint32_t s_rx_dma_index = 0U;
static volatile bool s_rx_active = false;
static volatile AppSerialControlTaskStats s_serial_stats;

extern osSemaphoreId_t SerialRxSemHandle;

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
        || (SerialRxSemHandle == NULL))
    {
        return false;
    }

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (s_rx_active)
    {
        if ((primask & 1U) == 0U)
        {
            __enable_irq();
        }
        return true;
    }

    uint32_t buffer_index = APP_SERIAL_RX_BUFFER_COUNT;
    for (uint32_t index = 0U;
         index < APP_SERIAL_RX_BUFFER_COUNT;
         ++index)
    {
        if (!s_rx_ready[index])
        {
            buffer_index = index;
            break;
        }
    }

    if (buffer_index >= APP_SERIAL_RX_BUFFER_COUNT)
    {
        if ((primask & 1U) == 0U)
        {
            __enable_irq();
        }
        return false;
    }

    s_rx_dma_index = buffer_index;
    s_rx_active = true;
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    app_serial_cache_invalidate(s_rx_dma_buffers[buffer_index],
                                APP_SERIAL_RX_DMA_SIZE);

    if (HAL_UARTEx_ReceiveToIdle_DMA(s_control_uart,
                                     s_rx_dma_buffers[buffer_index],
                                     APP_SERIAL_RX_DMA_SIZE) != HAL_OK)
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
    (void)AppOutput_Logf("[CTRL] UI TAP <RUN|STOP|PHASE+|PHASE-|CALSTART|CALSTOP>\r\n");
    (void)AppOutput_Logf("[CTRL] UI PAGE <MONITOR|SPECTRUM|TEST|CALIBRATION|SYSTEM>\r\n");
    (void)AppOutput_Logf("[CTRL] UI STATUS; CAL STATUS; use OUTPUT LOG before text queries\r\n");
}

static void app_serial_reply_status(void)
{
    AppSerialControlTaskStats serial_stats;
    AppUiRemoteStats ui_stats;
    AppCalibrationSnapshot calibration;

    AppSerialControlTask_GetStats(&serial_stats);
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
        "[CTRL] CAL state=%u error=%u exclusive=%u progress=%u qspi=%u relay=%u adc=%u dac=%u can_start=%u\r\n",
        (unsigned int)calibration.state,
        (unsigned int)calibration.error,
        calibration.exclusive ? 1U : 0U,
        (unsigned int)calibration.progress_percent,
        calibration.qspi_ready ? 1U : 0U,
        calibration.relay_configured ? 1U : 0U,
        calibration.adc_ready ? 1U : 0U,
        calibration.dac_ready ? 1U : 0U,
        calibration.can_start ? 1U : 0U);
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

    /* UI 交互自监视：DisplayTask 心跳、循环耗时、LVGL heap 与触摸健康。 */
    AppUiMonitorSnapshot ui_monitor;
    AppUiMonitor_GetSnapshot(&ui_monitor);
    (void)AppOutput_Logf(
        "[CTRL] UIMON heartbeat=%lu loops=%lu last_ms=%lu max_ms=%lu overruns=%lu\r\n",
        (unsigned long)ui_monitor.last_loop_tick,
        (unsigned long)ui_monitor.loop_count,
        (unsigned long)ui_monitor.last_loop_duration_ms,
        (unsigned long)ui_monitor.max_loop_duration_ms,
        (unsigned long)ui_monitor.overrun_count);
    (void)AppOutput_Logf(
        "[CTRL] LVGL used=%lu(%u%%) free=%lu biggest=%lu frag=%u%%\r\n",
        (unsigned long)ui_monitor.lvgl_used_bytes,
        (unsigned int)ui_monitor.lvgl_used_percent,
        (unsigned long)ui_monitor.lvgl_free_bytes,
        (unsigned long)ui_monitor.lvgl_biggest_free_bytes,
        (unsigned int)ui_monitor.lvgl_frag_percent);
    (void)AppOutput_Logf(
        "[CTRL] TOUCH errors=%lu stale_rel=%lu reinit=%lu last_valid=%lu\r\n",
        (unsigned long)ui_monitor.touch_read_errors,
        (unsigned long)ui_monitor.touch_stale_releases,
        (unsigned long)ui_monitor.touch_reinit_count,
        (unsigned long)ui_monitor.touch_last_valid_tick);
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
        else if (strcmp(target, "CALSTART") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_CALIBRATION_START;
        }
        else if (strcmp(target, "CALSTOP") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_CALIBRATION_STOP;
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
        else if (strcmp(target, "TEST") == 0)
        {
            *action = APP_UI_REMOTE_ACTION_PAGE_TEST;
        }
        else if ((strcmp(target, "CALIBRATION") == 0)
                 || (strcmp(target, "RESULTS") == 0))
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

static void app_serial_process_calibration(char **cursor)
{
    char *operation = app_serial_next_token(cursor);
    if ((operation == NULL) || (strcmp(operation, "STATUS") == 0))
    {
        AppCalibrationSnapshot snapshot;
        AppCalibrationLoopStatus loop;
        AppCalibrationService_GetSnapshot(&snapshot);
        AppCalibrationBoardPort_GetLoopStatus(&loop);
        (void)AppOutput_Logf(
            "[CAL] STATUS state=%u error=%u exclusive=%u can_start=%u active=%u reason=%s points=%lu/%lu\r\n",
            (unsigned int)snapshot.state,
            (unsigned int)snapshot.error,
            snapshot.exclusive ? 1U : 0U,
            snapshot.can_start ? 1U : 0U,
            loop.active ? 1U : 0U,
            AppCalibrationBoardPort_StopReasonString(loop.stop_reason),
            (unsigned long)loop.point_count,
            (unsigned long)snapshot.expected_point_count);
        (void)AppOutput_Logf(
            "[CAL] LAST output_mv=%lu mean=%lu.%03lu min=%u max=%u low=%lu high=%lu dir=%d samples=%lu relay=0x%x dac_code=%u adc_run=%u\r\n",
            (unsigned long)((loop.output_volts_nominal * 1000.0f) + 0.5f),
            (unsigned long)(((uint32_t)((loop.mean_code * 1000.0f) + 0.5f))
                            / 1000U),
            (unsigned long)(((uint32_t)((loop.mean_code * 1000.0f) + 0.5f))
                            % 1000U),
            (unsigned int)loop.minimum_code,
            (unsigned int)loop.maximum_code,
            (unsigned long)loop.low_edge_count,
            (unsigned long)loop.high_edge_count,
            (int)loop.response_direction,
            (unsigned long)loop.sample_count,
            (unsigned int)loop.relay_output_bits,
            (unsigned int)loop.dac_output_code,
            loop.adc_running ? 1U : 0U);
        return;
    }

    AppUiRemoteAction action;
    if (strcmp(operation, "START") == 0)
    {
        action = APP_UI_REMOTE_ACTION_CALIBRATION_START;
    }
    else if (strcmp(operation, "STOP") == 0)
    {
        action = APP_UI_REMOTE_ACTION_CALIBRATION_STOP;
    }
    else
    {
        (void)AppOutput_Logf("[CTRL] ERR CAL FORMAT\r\n");
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
    else if (strcmp(command, "CAL") == 0)
    {
        app_serial_process_calibration(&cursor);
    }
    else
    {
        ++s_serial_stats.unknown_commands;
        (void)AppOutput_Logf("[CTRL] ERR UNKNOWN COMMAND\r\n");
    }
}

bool AppSerialControlTask_Init(UART_HandleTypeDef *uart)
{
    if ((uart == NULL) || (uart->hdmarx == NULL))
    {
        return false;
    }

    s_control_uart = uart;
    s_rx_active = false;
    s_rx_dma_index = 0U;
    for (uint32_t index = 0U;
         index < APP_SERIAL_RX_BUFFER_COUNT;
         ++index)
    {
        s_rx_lengths[index] = 0U;
        s_rx_ready[index] = false;
    }
    return true;
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
        (void)osSemaphoreAcquire(SerialRxSemHandle,
                                 APP_SERIAL_RX_RETRY_MS);

        for (uint32_t buffer_index = 0U;
             buffer_index < APP_SERIAL_RX_BUFFER_COUNT;
             ++buffer_index)
        {
            if (!s_rx_ready[buffer_index])
            {
                continue;
            }

            __DMB();
            uint16_t rx_length = s_rx_lengths[buffer_index];
            for (uint32_t index = 0U; index < rx_length; ++index)
            {
                uint8_t byte = s_rx_dma_buffers[buffer_index][index];
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

            s_rx_lengths[buffer_index] = 0U;
            __DMB();
            s_rx_ready[buffer_index] = false;
        }

        (void)app_serial_start_rx();
    }
}

void AppSerialControlTask_OnRxEvent(UART_HandleTypeDef *uart, uint16_t size)
{
    if ((uart == NULL) || (uart != s_control_uart))
    {
        return;
    }

    uint32_t buffer_index = s_rx_dma_index;
    s_rx_active = false;
    if (size > APP_SERIAL_RX_DMA_SIZE)
    {
        size = APP_SERIAL_RX_DMA_SIZE;
    }

    if (size > 0U)
    {
        app_serial_cache_invalidate(s_rx_dma_buffers[buffer_index], size);
        if (s_rx_ready[buffer_index])
        {
            ++s_serial_stats.dropped_chunks;
        }
        else
        {
            s_rx_lengths[buffer_index] = size;
            __DMB();
            s_rx_ready[buffer_index] = true;
            s_serial_stats.received_bytes += size;
            ++s_serial_stats.received_chunks;
        }
    }

    (void)app_serial_start_rx();
    if ((size > 0U) && (SerialRxSemHandle != NULL))
    {
        (void)osSemaphoreRelease(SerialRxSemHandle);
    }
}

void AppSerialControlTask_OnUartError(UART_HandleTypeDef *uart)
{
    if ((uart == NULL) || (uart != s_control_uart))
    {
        return;
    }

    s_rx_active = false;
    ++s_serial_stats.uart_errors;
    (void)app_serial_start_rx();
}

void AppSerialControlTask_GetStats(AppSerialControlTaskStats *stats)
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
