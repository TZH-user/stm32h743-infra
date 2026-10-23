/**
  ******************************************************************************
  * @file    app_calibration_board_port.h
  * @brief   当前板卡校准继电器 Port 注册接口（UTF-8）。
  ******************************************************************************
  */

#ifndef APP_CALIBRATION_APP_CALIBRATION_BOARD_PORT_H_
#define APP_CALIBRATION_APP_CALIBRATION_BOARD_PORT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* 在 GPIO 初始化完成后注册当前板卡已经确认的校准硬件能力。 */
void AppCalibrationBoardPort_Register(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CALIBRATION_APP_CALIBRATION_BOARD_PORT_H_ */
