/**
  ******************************************************************************
  * @file    calibration_self_test.h
  * @brief   与硬件无关的直流/交流输出自检分析接口（UTF-8）。
  ******************************************************************************
  */

#ifndef CALIBRATION_SELF_TEST_H_
#define CALIBRATION_SELF_TEST_H_

#include "calibration_core.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CALIBRATION_SELF_TEST_LEVEL_COUNT  6U

typedef enum
{
    CALIBRATION_SELF_TEST_DC_QUARTER = 0,
    CALIBRATION_SELF_TEST_DC_HALF,
    CALIBRATION_SELF_TEST_DC_FULL,
    CALIBRATION_SELF_TEST_AC_QUARTER,
    CALIBRATION_SELF_TEST_AC_HALF,
    CALIBRATION_SELF_TEST_AC_FULL
} CalibrationSelfTestLevel;

typedef enum
{
    CALIBRATION_SELF_TEST_NOT_RUN = 0,
    CALIBRATION_SELF_TEST_RUNNING,
    CALIBRATION_SELF_TEST_PASS,
    CALIBRATION_SELF_TEST_ABNORMAL,
    CALIBRATION_SELF_TEST_BLOCKED,
    CALIBRATION_SELF_TEST_ABORTED
} CalibrationSelfTestState;

typedef enum
{
    CALIBRATION_SELF_TEST_REASON_NONE = 0,
    CALIBRATION_SELF_TEST_REASON_OUTPUT_IO,
    CALIBRATION_SELF_TEST_REASON_AMPLITUDE,
    CALIBRATION_SELF_TEST_REASON_NOISE,
    CALIBRATION_SELF_TEST_REASON_CLIPPED,
    CALIBRATION_SELF_TEST_REASON_TRANSFER_PLATEAU,
    CALIBRATION_SELF_TEST_REASON_NO_RESPONSE,
    CALIBRATION_SELF_TEST_REASON_INVALID_DATA,
    CALIBRATION_SELF_TEST_REASON_USER_ABORT
} CalibrationSelfTestReason;

typedef struct
{
    uint16_t adc_max_code;
    uint16_t clipping_edge_code;
    float codes_per_volt;
    float amplitude_tolerance_percent;
    float dc_noise_rms_max_codes;
    float dc_noise_peak_to_peak_max_codes;
    float ac_noise_rms_max_codes;
    float minimum_response_codes;
    float clipping_ratio_max_percent;
} CalibrationSelfTestCriteria;

typedef struct
{
    CalibrationSelfTestLevel level;
    CalibrationSelfTestState state;
    CalibrationSelfTestReason reason;
    float requested_level_volts;
    float measured_level_volts;
    float mean_code;
    float amplitude_code;
    float amplitude_error_percent;
    float noise_rms_code;
    float noise_peak_to_peak_code;
    float clipping_ratio_percent;
    uint32_t sample_count;
    uint16_t minimum_code;
    uint16_t maximum_code;
    bool output_ok;
    bool amplitude_ok;
    bool noise_ok;
} CalibrationSelfTestResult;

void CalibrationSelfTest_ResultInit(CalibrationSelfTestResult *result,
                                    CalibrationSelfTestLevel level,
                                    float requested_level_volts);

CalibrationCoreStatus CalibrationSelfTest_AnalyzeDc(
    const uint16_t *samples,
    uint32_t sample_count,
    float baseline_mean_code,
    float requested_volts,
    const CalibrationSelfTestCriteria *criteria,
    CalibrationSelfTestResult *result);

CalibrationCoreStatus CalibrationSelfTest_AnalyzeAc(
    const uint16_t *samples,
    uint32_t sample_count,
    uint32_t sample_rate_hz,
    uint32_t tone_frequency_hz,
    float requested_vpp,
    const CalibrationSelfTestCriteria *criteria,
    CalibrationSelfTestResult *result);

const char *CalibrationSelfTest_StateString(CalibrationSelfTestState state);
const char *CalibrationSelfTest_ReasonString(CalibrationSelfTestReason reason);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_SELF_TEST_H_ */
