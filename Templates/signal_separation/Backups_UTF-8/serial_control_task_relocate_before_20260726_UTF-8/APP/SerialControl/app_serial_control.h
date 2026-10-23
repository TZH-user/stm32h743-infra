#ifndef APP_SERIAL_CONTROL_APP_SERIAL_CONTROL_H_
#define APP_SERIAL_CONTROL_APP_SERIAL_CONTROL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t received_bytes;
    uint32_t received_chunks;
    uint32_t dropped_chunks;
    uint32_t completed_lines;
    uint32_t overflow_lines;
    uint32_t unknown_commands;
    uint32_t uart_errors;
    uint32_t rx_restart_failures;
} AppSerialControlStats;

/** Bind the UART after peripheral initialization. */
bool AppSerialControl_Init(UART_HandleTypeDef *uart);

/** Strong implementation of the task entry generated as weak by CubeMX. */
void StartSerialControlTask(void *argument);

/** HAL callback router entry points. */
void AppSerialControl_OnRxEvent(UART_HandleTypeDef *uart, uint16_t size);
void AppSerialControl_OnUartError(UART_HandleTypeDef *uart);

void AppSerialControl_GetStats(AppSerialControlStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* APP_SERIAL_CONTROL_APP_SERIAL_CONTROL_H_ */
