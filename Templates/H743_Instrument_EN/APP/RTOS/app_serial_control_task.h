#ifndef APP_RTOS_APP_SERIAL_CONTROL_TASK_H_
#define APP_RTOS_APP_SERIAL_CONTROL_TASK_H_

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
} AppSerialControlTaskStats;

/** @brief 外设初始化完成后绑定串口控制任务使用的 UART。 */
bool AppSerialControlTask_Init(UART_HandleTypeDef *uart);

/** @brief CubeMX 以 weak 形式生成的串口控制任务入口强实现。 */
void StartSerialControlTask(void *argument);

/** @brief 接收 UART IDLE/DMA 事件，由统一 HAL 回调路由调用。 */
void AppSerialControlTask_OnRxEvent(UART_HandleTypeDef *uart, uint16_t size);

/** @brief 接收 UART/DMA 错误事件，由统一 HAL 回调路由调用。 */
void AppSerialControlTask_OnUartError(UART_HandleTypeDef *uart);

/** @brief 读取串口控制任务的接收与命令统计。 */
void AppSerialControlTask_GetStats(AppSerialControlTaskStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* APP_RTOS_APP_SERIAL_CONTROL_TASK_H_ */
