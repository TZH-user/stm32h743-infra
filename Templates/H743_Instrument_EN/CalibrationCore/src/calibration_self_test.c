/**
  ******************************************************************************
  * @file    calibration_self_test.c
  * @brief   直流统计与单频正弦拟合实现（UTF-8）。
  ******************************************************************************
  */

#include "calibration_self_test.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define CALIBRATION_SELF_TEST_TWO_PI_F  6.28318530717958647692f

typedef struct
{
    double sum;
    double sum_squares;
    uint32_t low_edge_count;
    uint32_t high_edge_count;
    uint16_t minimum;
    uint16_t maximum;
} CalibrationSelfTestStatistics;

static bool calibration_self_test_criteria_valid(
    const CalibrationSelfTestCriteria *criteria)
{
    return (criteria != NULL)
        && (criteria->adc_max_code != 0U)
        && (criteria->clipping_edge_code < criteria->adc_max_code)
        && isfinite(criteria->codes_per_volt)
        && (criteria->codes_per_volt > 0.0f)
        && isfinite(criteria->amplitude_tolerance_percent)
        && (criteria->amplitude_tolerance_percent >= 0.0f)
        && isfinite(criteria->dc_noise_rms_max_codes)
        && (criteria->dc_noise_rms_max_codes >= 0.0f)
        && isfinite(criteria->dc_noise_peak_to_peak_max_codes)
        && (criteria->dc_noise_peak_to_peak_max_codes >= 0.0f)
        && isfinite(criteria->ac_noise_rms_max_codes)
        && (criteria->ac_noise_rms_max_codes >= 0.0f)
        && isfinite(criteria->minimum_response_codes)
        && (criteria->minimum_response_codes >= 0.0f)
        && isfinite(criteria->clipping_ratio_max_percent)
        && (criteria->clipping_ratio_max_percent >= 0.0f)
        && (criteria->clipping_ratio_max_percent <= 100.0f);
}

/* adc_max_code 表示量程上限而非位掩码，超范围样本按上限计入削顶。 */
static uint16_t calibration_self_test_code(
    uint16_t sample,
    const CalibrationSelfTestCriteria *criteria)
{
    return (sample > criteria->adc_max_code)
        ? criteria->adc_max_code : sample;
}

static CalibrationCoreStatus calibration_self_test_statistics(
    const uint16_t *samples,
    uint32_t sample_count,
    const CalibrationSelfTestCriteria *criteria,
    CalibrationSelfTestStatistics *statistics)
{
    if ((samples == NULL) || (sample_count == 0U)
        || !calibration_self_test_criteria_valid(criteria)
        || (statistics == NULL))
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }

    memset(statistics, 0, sizeof(*statistics));
    statistics->minimum = criteria->adc_max_code;
    uint16_t high_edge = (uint16_t)(criteria->adc_max_code
                                    - criteria->clipping_edge_code);
    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        uint16_t code = calibration_self_test_code(samples[index], criteria);
        double value = (double)code;
        statistics->sum += value;
        statistics->sum_squares += value * value;
        if (code < statistics->minimum)
        {
            statistics->minimum = code;
        }
        if (code > statistics->maximum)
        {
            statistics->maximum = code;
        }
        if (code <= criteria->clipping_edge_code)
        {
            ++statistics->low_edge_count;
        }
        if (code >= high_edge)
        {
            ++statistics->high_edge_count;
        }
    }
    return CALIBRATION_CORE_OK;
}

static void calibration_self_test_apply_common(
    const CalibrationSelfTestStatistics *statistics,
    uint32_t sample_count,
    const CalibrationSelfTestCriteria *criteria,
    CalibrationSelfTestResult *result)
{
    double count = (double)sample_count;
    double mean = statistics->sum / count;
    double variance = (statistics->sum_squares / count) - (mean * mean);
    if (variance < 0.0)
    {
        variance = 0.0;
    }

    result->sample_count = sample_count;
    result->minimum_code = statistics->minimum;
    result->maximum_code = statistics->maximum;
    result->mean_code = (float)mean;
    result->noise_rms_code = (float)sqrt(variance);
    result->noise_peak_to_peak_code =
        (float)(statistics->maximum - statistics->minimum);
    result->clipping_ratio_percent = 100.0f
        * (float)(statistics->low_edge_count + statistics->high_edge_count)
        / (float)sample_count;
    result->output_ok = result->clipping_ratio_percent
        <= criteria->clipping_ratio_max_percent;
}

static void calibration_self_test_finish(
    const CalibrationSelfTestCriteria *criteria,
    CalibrationSelfTestResult *result)
{
    if (!result->output_ok)
    {
        result->state = CALIBRATION_SELF_TEST_ABNORMAL;
        result->reason = CALIBRATION_SELF_TEST_REASON_CLIPPED;
    }
    else if (result->amplitude_code < criteria->minimum_response_codes)
    {
        result->state = CALIBRATION_SELF_TEST_ABNORMAL;
        result->reason = CALIBRATION_SELF_TEST_REASON_NO_RESPONSE;
        result->output_ok = false;
        result->amplitude_ok = false;
    }
    else if (!result->amplitude_ok)
    {
        result->state = CALIBRATION_SELF_TEST_ABNORMAL;
        result->reason = CALIBRATION_SELF_TEST_REASON_AMPLITUDE;
    }
    else if (!result->noise_ok)
    {
        result->state = CALIBRATION_SELF_TEST_ABNORMAL;
        result->reason = CALIBRATION_SELF_TEST_REASON_NOISE;
    }
    else
    {
        result->state = CALIBRATION_SELF_TEST_PASS;
        result->reason = CALIBRATION_SELF_TEST_REASON_NONE;
    }
}

void CalibrationSelfTest_ResultInit(CalibrationSelfTestResult *result,
                                    CalibrationSelfTestLevel level,
                                    float requested_level_volts)
{
    if (result == NULL)
    {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->level = level;
    result->state = CALIBRATION_SELF_TEST_NOT_RUN;
    result->reason = CALIBRATION_SELF_TEST_REASON_NONE;
    result->requested_level_volts = requested_level_volts;
}

CalibrationCoreStatus CalibrationSelfTest_AnalyzeDc(
    const uint16_t *samples,
    uint32_t sample_count,
    float baseline_mean_code,
    float requested_volts,
    const CalibrationSelfTestCriteria *criteria,
    CalibrationSelfTestResult *result)
{
    if ((result == NULL) || !isfinite(baseline_mean_code)
        || !isfinite(requested_volts) || (requested_volts <= 0.0f))
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }

    CalibrationSelfTestStatistics statistics;
    CalibrationCoreStatus status = calibration_self_test_statistics(
        samples, sample_count, criteria, &statistics);
    if (status != CALIBRATION_CORE_OK)
    {
        return status;
    }

    calibration_self_test_apply_common(&statistics,
                                        sample_count,
                                        criteria,
                                        result);
    result->requested_level_volts = requested_volts;
    result->amplitude_code = fabsf(result->mean_code - baseline_mean_code);
    result->measured_level_volts = result->amplitude_code
        / criteria->codes_per_volt;
    result->amplitude_error_percent = 100.0f
        * (result->measured_level_volts - requested_volts)
        / requested_volts;
    result->amplitude_ok = fabsf(result->amplitude_error_percent)
        <= criteria->amplitude_tolerance_percent;
    result->noise_ok = (result->noise_rms_code
                        <= criteria->dc_noise_rms_max_codes)
        && (result->noise_peak_to_peak_code
            <= criteria->dc_noise_peak_to_peak_max_codes);
    calibration_self_test_finish(criteria, result);
    return CALIBRATION_CORE_OK;
}

CalibrationCoreStatus CalibrationSelfTest_AnalyzeAc(
    const uint16_t *samples,
    uint32_t sample_count,
    uint32_t sample_rate_hz,
    uint32_t tone_frequency_hz,
    float requested_vpp,
    const CalibrationSelfTestCriteria *criteria,
    CalibrationSelfTestResult *result)
{
    if ((result == NULL) || (sample_rate_hz == 0U)
        || (tone_frequency_hz == 0U)
        || (tone_frequency_hz >= (sample_rate_hz / 2U))
        || !isfinite(requested_vpp) || (requested_vpp <= 0.0f))
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }

    CalibrationSelfTestStatistics statistics;
    CalibrationCoreStatus status = calibration_self_test_statistics(
        samples, sample_count, criteria, &statistics);
    if (status != CALIBRATION_CORE_OK)
    {
        return status;
    }
    calibration_self_test_apply_common(&statistics,
                                        sample_count,
                                        criteria,
                                        result);

    float omega = CALIBRATION_SELF_TEST_TWO_PI_F
        * (float)tone_frequency_hz / (float)sample_rate_hz;
    double sine_sum = 0.0;
    double cosine_sum = 0.0;
    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        float phase = omega * (float)index;
        double centered = (double)calibration_self_test_code(samples[index],
                                                             criteria)
            - (double)result->mean_code;
        sine_sum += centered * (double)sinf(phase);
        cosine_sum += centered * (double)cosf(phase);
    }

    double scale = 2.0 / (double)sample_count;
    double sine_coefficient = sine_sum * scale;
    double cosine_coefficient = cosine_sum * scale;
    result->amplitude_code = (float)sqrt(
        (sine_coefficient * sine_coefficient)
        + (cosine_coefficient * cosine_coefficient));

    double residual_sum_squares = 0.0;
    for (uint32_t index = 0U; index < sample_count; ++index)
    {
        float phase = omega * (float)index;
        double fitted = (double)result->mean_code
            + sine_coefficient * (double)sinf(phase)
            + cosine_coefficient * (double)cosf(phase);
        double residual = (double)calibration_self_test_code(samples[index],
                                                             criteria)
            - fitted;
        residual_sum_squares += residual * residual;
    }

    result->requested_level_volts = requested_vpp;
    result->measured_level_volts = 2.0f * result->amplitude_code
        / criteria->codes_per_volt;
    result->amplitude_error_percent = 100.0f
        * (result->measured_level_volts - requested_vpp)
        / requested_vpp;
    result->noise_rms_code = (float)sqrt(
        residual_sum_squares / (double)sample_count);
    result->amplitude_ok = fabsf(result->amplitude_error_percent)
        <= criteria->amplitude_tolerance_percent;
    result->noise_ok = result->noise_rms_code
        <= criteria->ac_noise_rms_max_codes;
    calibration_self_test_finish(criteria, result);
    return CALIBRATION_CORE_OK;
}

const char *CalibrationSelfTest_StateString(CalibrationSelfTestState state)
{
    switch (state)
    {
        case CALIBRATION_SELF_TEST_NOT_RUN:
            return "not_run";
        case CALIBRATION_SELF_TEST_RUNNING:
            return "running";
        case CALIBRATION_SELF_TEST_PASS:
            return "pass";
        case CALIBRATION_SELF_TEST_ABNORMAL:
            return "abnormal";
        case CALIBRATION_SELF_TEST_BLOCKED:
            return "blocked";
        case CALIBRATION_SELF_TEST_ABORTED:
            return "aborted";
        default:
            return "unknown";
    }
}

const char *CalibrationSelfTest_ReasonString(CalibrationSelfTestReason reason)
{
    switch (reason)
    {
        case CALIBRATION_SELF_TEST_REASON_NONE:
            return "none";
        case CALIBRATION_SELF_TEST_REASON_OUTPUT_IO:
            return "output_io";
        case CALIBRATION_SELF_TEST_REASON_AMPLITUDE:
            return "amplitude";
        case CALIBRATION_SELF_TEST_REASON_NOISE:
            return "noise";
        case CALIBRATION_SELF_TEST_REASON_CLIPPED:
            return "clipped";
        case CALIBRATION_SELF_TEST_REASON_TRANSFER_PLATEAU:
            return "transfer_plateau";
        case CALIBRATION_SELF_TEST_REASON_NO_RESPONSE:
            return "no_response";
        case CALIBRATION_SELF_TEST_REASON_INVALID_DATA:
            return "invalid_data";
        case CALIBRATION_SELF_TEST_REASON_USER_ABORT:
            return "user_abort";
        default:
            return "unknown";
    }
}
