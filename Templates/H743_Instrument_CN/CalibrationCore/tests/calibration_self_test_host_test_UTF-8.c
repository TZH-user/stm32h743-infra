/**
  ******************************************************************************
  * @file    calibration_self_test_host_test_UTF-8.c
  * @brief   直流/交流输出自检核心的主机侧回归测试（UTF-8）。
  ******************************************************************************
  */

#include "calibration_self_test.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_SAMPLE_COUNT  1000U
#define TEST_TWO_PI        6.28318530717958647692

static CalibrationSelfTestCriteria test_default_criteria(void)
{
    const CalibrationSelfTestCriteria criteria = {
        .adc_max_code = 4095U,
        .clipping_edge_code = 16U,
        .codes_per_volt = 410.0f,
        .amplitude_tolerance_percent = 2.0f,
        .dc_noise_rms_max_codes = 2.0f,
        .dc_noise_peak_to_peak_max_codes = 6.0f,
        .ac_noise_rms_max_codes = 2.0f,
        .minimum_response_codes = 8.0f,
        .clipping_ratio_max_percent = 1.0f,
    };
    return criteria;
}

static void test_require(bool condition, const char *message)
{
    if (!condition)
    {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void test_dc_pass_and_noise_failure(void)
{
    const uint16_t clean[] = {909U, 910U, 911U, 910U,
                              909U, 910U, 911U, 910U};
    const uint16_t noisy[] = {890U, 930U, 890U, 930U,
                              890U, 930U, 890U, 930U};
    const uint16_t no_response[] = {500U, 500U, 500U, 500U};
    CalibrationSelfTestCriteria criteria = test_default_criteria();
    CalibrationSelfTestResult result;

    CalibrationSelfTest_ResultInit(&result,
                                   CALIBRATION_SELF_TEST_DC_QUARTER,
                                   1.0f);
    test_require(CalibrationSelfTest_AnalyzeDc(
                     clean, 8U, 500.0f, 1.0f, &criteria, &result)
                     == CALIBRATION_CORE_OK,
                 "clean DC analysis returned an error");
    test_require(result.state == CALIBRATION_SELF_TEST_PASS,
                 "clean DC level did not pass");

    CalibrationSelfTest_ResultInit(&result,
                                   CALIBRATION_SELF_TEST_DC_QUARTER,
                                   1.0f);
    test_require(CalibrationSelfTest_AnalyzeDc(
                     noisy, 8U, 500.0f, 1.0f, &criteria, &result)
                     == CALIBRATION_CORE_OK,
                 "noisy DC analysis returned an error");
    test_require(result.state == CALIBRATION_SELF_TEST_ABNORMAL,
                 "noisy DC level was not rejected");
    test_require(result.reason == CALIBRATION_SELF_TEST_REASON_NOISE,
                 "noisy DC level reported the wrong reason");

    CalibrationSelfTest_ResultInit(&result,
                                   CALIBRATION_SELF_TEST_DC_QUARTER,
                                   1.0f);
    test_require(CalibrationSelfTest_AnalyzeDc(
                     no_response, 4U, 500.0f, 1.0f, &criteria, &result)
                     == CALIBRATION_CORE_OK,
                 "no-response DC analysis returned an error");
    test_require(result.reason == CALIBRATION_SELF_TEST_REASON_NO_RESPONSE,
                 "no-response DC level reported the wrong reason");
    test_require(!result.output_ok,
                 "no-response DC level incorrectly reported output OK");
}

static void test_ac_pass_and_noise_failure(void)
{
    static uint16_t clean[TEST_SAMPLE_COUNT];
    static uint16_t noisy[TEST_SAMPLE_COUNT];
    CalibrationSelfTestCriteria criteria = test_default_criteria();
    CalibrationSelfTestResult result;

    for (uint32_t index = 0U; index < TEST_SAMPLE_COUNT; ++index)
    {
        double phase = TEST_TWO_PI * 20000.0 * (double)index / 1000000.0;
        int32_t code = (int32_t)lround(2000.0 + 205.0 * sin(phase));
        clean[index] = (uint16_t)code;
        noisy[index] = (uint16_t)(code + ((index & 1U) ? 10 : -10));
    }

    CalibrationSelfTest_ResultInit(&result,
                                   CALIBRATION_SELF_TEST_AC_QUARTER,
                                   1.0f);
    test_require(CalibrationSelfTest_AnalyzeAc(
                     clean, TEST_SAMPLE_COUNT, 1000000U, 20000U,
                     1.0f, &criteria, &result) == CALIBRATION_CORE_OK,
                 "clean AC analysis returned an error");
    test_require(result.state == CALIBRATION_SELF_TEST_PASS,
                 "clean AC level did not pass");

    CalibrationSelfTest_ResultInit(&result,
                                   CALIBRATION_SELF_TEST_AC_QUARTER,
                                   1.0f);
    test_require(CalibrationSelfTest_AnalyzeAc(
                     noisy, TEST_SAMPLE_COUNT, 1000000U, 20000U,
                     1.0f, &criteria, &result) == CALIBRATION_CORE_OK,
                 "noisy AC analysis returned an error");
    test_require(result.state == CALIBRATION_SELF_TEST_ABNORMAL,
                 "noisy AC level was not rejected");
    test_require(result.reason == CALIBRATION_SELF_TEST_REASON_NOISE,
                 "noisy AC level reported the wrong reason");
}

static void test_non_mask_adc_limit_and_invalid_criteria(void)
{
    const uint16_t over_range[] = {1200U, 1200U, 1200U, 1200U};
    CalibrationSelfTestCriteria criteria = test_default_criteria();
    CalibrationSelfTestResult result;

    criteria.adc_max_code = 1000U;
    criteria.clipping_edge_code = 10U;
    criteria.codes_per_volt = 100.0f;
    CalibrationSelfTest_ResultInit(&result,
                                   CALIBRATION_SELF_TEST_DC_FULL,
                                   10.0f);
    test_require(CalibrationSelfTest_AnalyzeDc(
                     over_range, 4U, 0.0f, 10.0f, &criteria, &result)
                     == CALIBRATION_CORE_OK,
                 "non-mask ADC limit analysis returned an error");
    test_require(result.maximum_code == 1000U,
                 "adc_max_code was incorrectly treated as a bit mask");
    test_require(result.reason == CALIBRATION_SELF_TEST_REASON_CLIPPED,
                 "over-range data was not reported as clipped");

    criteria.ac_noise_rms_max_codes = NAN;
    test_require(CalibrationSelfTest_AnalyzeDc(
                     over_range, 4U, 0.0f, 10.0f, &criteria, &result)
                     == CALIBRATION_CORE_ERROR_ARGUMENT,
                 "NaN criteria was accepted");
}

int main(void)
{
    test_dc_pass_and_noise_failure();
    test_ac_pass_and_noise_failure();
    test_non_mask_adc_limit_and_invalid_criteria();
    (void)puts("calibration_self_test_host_test: PASS");
    return EXIT_SUCCESS;
}
