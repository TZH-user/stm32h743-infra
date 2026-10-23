/**
  ******************************************************************************
  * @file    app_calibration_board_port.h
  * @brief   当前板卡校准继电器 Port 注册接口（UTF-8）。
  ******************************************************************************
  */

#ifndef APP_CALIBRATION_APP_CALIBRATION_BOARD_PORT_H_
#define APP_CALIBRATION_APP_CALIBRATION_BOARD_PORT_H_

#include "calibration_self_test.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    APP_CALIBRATION_LOOP_STOP_NONE = 0,
    APP_CALIBRATION_LOOP_STOP_COMPLETE,
    APP_CALIBRATION_LOOP_STOP_ADC_CLIPPED,
    APP_CALIBRATION_LOOP_STOP_TRANSFER_PLATEAU,
    APP_CALIBRATION_LOOP_STOP_NO_RESPONSE,
    APP_CALIBRATION_LOOP_STOP_NON_MONOTONIC,
    APP_CALIBRATION_LOOP_STOP_IO_ERROR,
    APP_CALIBRATION_LOOP_STOP_ABORTED
} AppCalibrationLoopStopReason;

typedef struct
{
    uint32_t point_count;
    uint32_t sample_count;
    uint16_t minimum_code;
    uint16_t maximum_code;
    uint32_t low_edge_count;
    uint32_t high_edge_count;
    float output_volts_nominal;
    float mean_code;
    int8_t response_direction;
    uint8_t relay_output_bits;
    uint16_t dac_output_code;
    AppCalibrationLoopStopReason stop_reason;
    bool adc_running;
    bool active;
} AppCalibrationLoopStatus;

typedef struct
{
    CalibrationSelfTestResult results[CALIBRATION_SELF_TEST_LEVEL_COUNT];
    uint8_t active_level;
    uint8_t completed_count;
    bool active;
    bool criteria_provisional;
} AppCalibrationSelfTestStatus;

/* 在 GPIO 初始化完成后注册当前板卡已经确认的校准硬件能力。 */
void AppCalibrationBoardPort_Register(void);
void AppCalibrationBoardPort_Run(void);
uint32_t AppCalibrationBoardPort_GetExpectedPointCount(void);
uint32_t AppCalibrationBoardPort_GetExpectedPointCountForItem(
    uint32_t item_id);
void AppCalibrationBoardPort_GetLoopStatus(AppCalibrationLoopStatus *status);
void AppCalibrationBoardPort_GetSelfTestStatus(
    AppCalibrationSelfTestStatus *status);
const char *AppCalibrationBoardPort_StopReasonString(
    AppCalibrationLoopStopReason reason);

#ifdef __cplusplus
}
#endif

#endif /* APP_CALIBRATION_APP_CALIBRATION_BOARD_PORT_H_ */
