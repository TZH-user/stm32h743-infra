/**
  ******************************************************************************
  * @file    app_calibration_storage_port.h
  * @brief   校准数据到外部 QSPI Flash 的 Port（UTF-8）。
  ******************************************************************************
  */

#ifndef APP_CALIBRATION_APP_CALIBRATION_STORAGE_PORT_H_
#define APP_CALIBRATION_APP_CALIBRATION_STORAGE_PORT_H_

#include "app_calibration_port.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 在 RTOS 启动后初始化互斥、读取 JEDEC ID 并恢复校准元数据。 */
bool AppCalibrationStoragePort_Init(void);

/* 把已经初始化的存储能力填入板级端口集合，不直接覆盖其他 Port。 */
void AppCalibrationStoragePort_Fill(AppCalibrationStoragePort *storage);

#ifdef __cplusplus
}
#endif

#endif /* APP_CALIBRATION_APP_CALIBRATION_STORAGE_PORT_H_ */
