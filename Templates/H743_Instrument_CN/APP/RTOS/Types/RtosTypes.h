#ifndef APP_RTOS_TYPES_RTOSTYPES_H_
#define APP_RTOS_TYPES_RTOSTYPES_H_

#include "cmsis_os2.h"

/* CubeMX-created RTOS object handles used by application-owned task files. */
extern osMessageQueueId_t PrintQueueHandle;
extern osSemaphoreId_t AdcFrameReadySemHandle;
extern osThreadId_t DisplayTaskHandle;
extern osThreadId_t TouchTaskHandle;
extern osThreadId_t PrintfTaskHandle;
extern osThreadId_t MainTaskHandle;
extern osThreadId_t PhaseLockTaskHandle;

void StartPrintfTask(void *argument);
void StartMainTask(void *argument);
void StartPhaseLockTask(void *argument);

/* AdcTask/FftTask 已退出当前CubeMX任务表，不在活跃任务层导出句柄。 */

#endif /* APP_RTOS_TYPES_RTOSTYPES_H_ */
