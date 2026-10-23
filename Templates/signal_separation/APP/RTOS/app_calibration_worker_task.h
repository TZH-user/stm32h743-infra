/**
  ******************************************************************************
  * @file    app_calibration_worker_task.h
  * @brief   校准/QSPI 阻塞操作专用 worker 任务接口（UTF-8）。
  ******************************************************************************
  */

#ifndef APP_RTOS_APP_CALIBRATION_WORKER_TASK_H_
#define APP_RTOS_APP_CALIBRATION_WORKER_TASK_H_

#include "cmsis_os2.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 审计报告 P0-01 整改：
 * 校准板级回环（继电器/ADC/DAC）、校准状态机和 QSPI 存储不允许留在
 * DisplayTask 中串行执行，否则任一外设阻塞都会冻结 LVGL 和触摸。
 * 这些职责统一收敛到本 worker 任务，优先级低于 DisplayTask。
 */

/** 创建 worker 任务；重复调用安全。由 DisplayTask 初始化阶段调用一次。 */
bool AppCalibrationWorkerTask_Start(void);

/** 诊断用：返回 worker 线程句柄，未创建时为 NULL。 */
osThreadId_t AppCalibrationWorkerTask_GetHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_RTOS_APP_CALIBRATION_WORKER_TASK_H_ */
