/**
  ******************************************************************************
  * @file    app_calibration_board_port.c
  * @brief   P2-18(PC6)/P2-16(PC7) 高有效校准继电器 Port（UTF-8）。
  ******************************************************************************
  */

#include "app_calibration_board_port.h"

#include "ad9226.h"
#include "app_dac_output.h"
#include "app_output_task.h"
#include "app_calibration_service.h"
#include "app_calibration_storage_port.h"
#include "cmsis_os2.h"
#include "dac.h"
#include "main.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define APP_CALIBRATION_RELAY_MASK  \
    (CAL_RELAY_INPUT_SEL_Pin | CAL_RELAY_ADC_LOOP_Pin)

#define APP_CALIBRATION_DAC_NOMINAL_VDDA       3.3f
#define APP_CALIBRATION_DAC_LOWER_VOLTS         0.0f
#define APP_CALIBRATION_DAC_UPPER_VOLTS         3.0f
#define APP_CALIBRATION_DAC_MAX_STEP_VOLTS      0.05f
#define APP_CALIBRATION_DAC_DWELL_MS            25U
#define APP_CALIBRATION_ADC_SAMPLE_RATE_HZ      1000000UL
#define APP_CALIBRATION_ADC_CAPTURE_TIMEOUT_MS  100U
#define APP_CALIBRATION_ADC_EDGE_CODE           16U
#define APP_CALIBRATION_ADC_EDGE_PERCENT        1U
#define APP_CALIBRATION_RESPONSE_MIN_CODE       8.0f
#define APP_CALIBRATION_RESPONSE_CHECK_VOLTS    0.20f
#define APP_CALIBRATION_REVERSE_TOLERANCE_CODE  16.0f
#define APP_CALIBRATION_PLATEAU_DELTA_CODE      2.0f
#define APP_CALIBRATION_PLATEAU_LIMIT           3U

#define APP_CALIBRATION_SELF_TEST_DC_FULL_VOLTS      3.0f
#define APP_CALIBRATION_SELF_TEST_AC_FULL_VPP         0.8f
#define APP_CALIBRATION_SELF_TEST_AC_OFFSET_VOLTS     0.5f
#define APP_CALIBRATION_SELF_TEST_AC_FREQUENCY_HZ     20000UL
#define APP_CALIBRATION_SELF_TEST_AC_DWELL_MS         50U
#define APP_CALIBRATION_SELF_TEST_WAVEFORM_HOLD_MS    1000U
/* “ADC DISPLAY ON”在 READY/REVIEW 空闲态的实时预览采集周期。 */
#define APP_CALIBRATION_ADC_PREVIEW_IDLE_PERIOD_MS    250U
/*
 * 预览横轴只取帧首 1/N 时间窗：4096 点 @1 MS/s 全帧 4.1 ms 会把
 * 20 kHz 波形挤进 ~82 个周期，缩到 1/4（约 1.0 ms、20 个周期）后
 * 波形形状在 256 点折线上可辨。
 */
#define APP_CALIBRATION_ADC_PREVIEW_WINDOW_DIVIDER    4U
#define APP_CALIBRATION_SELF_TEST_CODES_PER_VOLT      409.5f
#define APP_CALIBRATION_SELF_TEST_AMPLITUDE_TOLERANCE 5.0f
#define APP_CALIBRATION_SELF_TEST_DC_NOISE_RMS_CODE   3.0f
#define APP_CALIBRATION_SELF_TEST_DC_NOISE_PP_CODE    24.0f
#define APP_CALIBRATION_SELF_TEST_AC_NOISE_RMS_CODE   4.0f

#define APP_CALIBRATION_POINT_FLAG_LOW_EDGE     (1UL << 0)
#define APP_CALIBRATION_POINT_FLAG_HIGH_EDGE    (1UL << 1)
#define APP_CALIBRATION_POINT_FLAG_CLIPPED      (1UL << 2)
#define APP_CALIBRATION_POINT_FLAG_PLATEAU      (1UL << 3)

typedef struct
{
    AppCalibrationLoopStatus status;
    CalibrationRampLimits limits;
    float baseline_mean_code;
    float previous_mean_code;
    uint32_t next_action_tick;
    uint8_t plateau_count;
    bool baseline_valid;
    bool dac_direct_mode;
    const uint16_t *last_frame_samples;
    uint32_t last_frame_sample_count;
    AppCalibrationSelfTestStatus self_test;
    uint32_t self_test_next_action_tick;
    float self_test_current_output_volts;
    float self_test_previous_mean_code;
    float self_test_baseline_mean_code;
    int8_t self_test_response_direction;
    uint8_t self_test_plateau_count;
    uint8_t self_test_phase;
    uint8_t self_test_resume_phase;
    uint8_t self_test_resume_level;
    volatile bool adc_preview_enabled;
    volatile uint32_t adc_preview_write_sequence;
    AppCalibrationAdcPreview adc_preview;
} AppCalibrationBoardContext;

typedef enum
{
    APP_CALIBRATION_SELF_TEST_PHASE_IDLE = 0,
    APP_CALIBRATION_SELF_TEST_PHASE_BASELINE,
    APP_CALIBRATION_SELF_TEST_PHASE_DC_RAMP,
    APP_CALIBRATION_SELF_TEST_PHASE_DC_CAPTURE,
    APP_CALIBRATION_SELF_TEST_PHASE_RAMP_TO_AC,
    APP_CALIBRATION_SELF_TEST_PHASE_AC_START,
    APP_CALIBRATION_SELF_TEST_PHASE_AC_CAPTURE,
    APP_CALIBRATION_SELF_TEST_PHASE_WAVEFORM_HOLD
} AppCalibrationSelfTestPhase;

static const float s_self_test_levels[CALIBRATION_SELF_TEST_LEVEL_COUNT] = {
    APP_CALIBRATION_SELF_TEST_DC_FULL_VOLTS * 0.25f,
    APP_CALIBRATION_SELF_TEST_DC_FULL_VOLTS * 0.50f,
    APP_CALIBRATION_SELF_TEST_DC_FULL_VOLTS,
    APP_CALIBRATION_SELF_TEST_AC_FULL_VPP * 0.25f,
    APP_CALIBRATION_SELF_TEST_AC_FULL_VPP * 0.50f,
    APP_CALIBRATION_SELF_TEST_AC_FULL_VPP,
};

static const CalibrationSelfTestCriteria s_self_test_criteria = {
    .adc_max_code = AD9226_SAMPLE_MASK,
    .clipping_edge_code = APP_CALIBRATION_ADC_EDGE_CODE,
    .codes_per_volt = APP_CALIBRATION_SELF_TEST_CODES_PER_VOLT,
    .amplitude_tolerance_percent =
        APP_CALIBRATION_SELF_TEST_AMPLITUDE_TOLERANCE,
    .dc_noise_rms_max_codes = APP_CALIBRATION_SELF_TEST_DC_NOISE_RMS_CODE,
    .dc_noise_peak_to_peak_max_codes =
        APP_CALIBRATION_SELF_TEST_DC_NOISE_PP_CODE,
    .ac_noise_rms_max_codes = APP_CALIBRATION_SELF_TEST_AC_NOISE_RMS_CODE,
    .minimum_response_codes = APP_CALIBRATION_RESPONSE_MIN_CODE,
    .clipping_ratio_max_percent = APP_CALIBRATION_ADC_EDGE_PERCENT,
};

static AppCalibrationBoardContext s_board;

static AppCalibrationPortStatus app_calibration_board_set_relay_state(
    GPIO_PinState state)
{
    /* 两个控制脚位于同一 GPIO Port，一次写入可避免继电器中间组合态。 */
    HAL_GPIO_WritePin(CAL_RELAY_INPUT_SEL_GPIO_Port,
                      APP_CALIBRATION_RELAY_MASK,
                      state);
    __DMB();

    uint32_t expected = (state == GPIO_PIN_SET)
        ? APP_CALIBRATION_RELAY_MASK : 0U;
    uint32_t actual = CAL_RELAY_INPUT_SEL_GPIO_Port->ODR
        & APP_CALIBRATION_RELAY_MASK;
    return (actual == expected)
        ? APP_CALIBRATION_PORT_OK : APP_CALIBRATION_PORT_IO_ERROR;
}

static bool app_calibration_board_relay_is_configured(void *context)
{
    (void)context;
    return true;
}

static bool app_calibration_board_relay_supports_source(void *context,
                                                        uint32_t source_id)
{
    (void)context;
    return (source_id == (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC)
        || (source_id == (uint32_t)APP_CALIBRATION_SOURCE_EXTERNAL_INPUT);
}

static AppCalibrationPortStatus app_calibration_board_relay_safe(
    void *context)
{
    (void)context;
    /* 低电平恢复正常任务测量链路。 */
    return app_calibration_board_set_relay_state(GPIO_PIN_RESET);
}

static AppCalibrationPortStatus app_calibration_board_relay_route(
    void *context,
    uint32_t source_id)
{
    (void)context;

    if (source_id == (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC)
    {
        /* PC6/PC7 同时为高：PA4(DAC1) 接入 AD9226 校准回环。 */
        return app_calibration_board_set_relay_state(GPIO_PIN_SET);
    }
    if (source_id == (uint32_t)APP_CALIBRATION_SOURCE_EXTERNAL_INPUT)
    {
        return app_calibration_board_set_relay_state(GPIO_PIN_RESET);
    }

    /* 当前继电器接线没有 DAC2 回环依据，拒绝前先恢复正常链路。 */
    (void)app_calibration_board_set_relay_state(GPIO_PIN_RESET);
    return APP_CALIBRATION_PORT_OUT_OF_RANGE;
}

static bool app_calibration_board_dac_is_configured(void *context)
{
    (void)context;
    return (hdac1.Instance == DAC1)
        && (HAL_DAC_GetState(&hdac1) != HAL_DAC_STATE_RESET);
}

static AppCalibrationPortStatus app_calibration_board_configure_dac(
    uint32_t trigger)
{
    DAC_ChannelConfTypeDef config = {0};
    config.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
    config.DAC_Trigger = trigger;
    config.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    config.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
    config.DAC_UserTrimming = DAC_TRIMMING_FACTORY;

    return (HAL_DAC_ConfigChannel(&hdac1, &config, DAC_CHANNEL_1) == HAL_OK)
        ? APP_CALIBRATION_PORT_OK : APP_CALIBRATION_PORT_IO_ERROR;
}

static AppCalibrationPortStatus app_calibration_board_dac_stop_all(
    void *context)
{
    (void)context;

    AppDacOutput_Stop();

    if (s_board.dac_direct_mode)
    {
        (void)HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1,
                              DAC_ALIGN_12B_R, 0U);
        __DMB();
        (void)HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
        s_board.dac_direct_mode = false;
    }

    (void)HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1,
                          DAC_ALIGN_12B_R, 0U);
    (void)HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2,
                          DAC_ALIGN_12B_R, 0U);
    __DMB();

    /* 正常任务使用 TIM6 TRGO，退出校准前恢复 CubeMX 配置。 */
    return app_calibration_board_configure_dac(DAC_TRIGGER_T6_TRGO);
}

static AppCalibrationPortStatus app_calibration_board_dac_get_limits(
    void *context,
    uint32_t source_id,
    CalibrationRampLimits *limits)
{
    (void)context;
    if ((limits == NULL)
        || (source_id != (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC))
    {
        return APP_CALIBRATION_PORT_OUT_OF_RANGE;
    }

    limits->lower_limit = APP_CALIBRATION_DAC_LOWER_VOLTS;
    limits->upper_limit = APP_CALIBRATION_DAC_UPPER_VOLTS;
    limits->max_step = APP_CALIBRATION_DAC_MAX_STEP_VOLTS;
    limits->dwell_time_ms = APP_CALIBRATION_DAC_DWELL_MS;
    return APP_CALIBRATION_PORT_OK;
}

static AppCalibrationPortStatus app_calibration_board_dac_set_output(
    void *context,
    uint32_t source_id,
    float target_value)
{
    (void)context;
    if ((source_id != (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC)
        || !isfinite(target_value)
        || (target_value < APP_CALIBRATION_DAC_LOWER_VOLTS)
        || (target_value > APP_CALIBRATION_DAC_UPPER_VOLTS))
    {
        return APP_CALIBRATION_PORT_OUT_OF_RANGE;
    }

    if (!s_board.dac_direct_mode)
    {
        (void)HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
        if (app_calibration_board_configure_dac(DAC_TRIGGER_NONE)
            != APP_CALIBRATION_PORT_OK)
        {
            return APP_CALIBRATION_PORT_IO_ERROR;
        }
        if (HAL_DAC_Start(&hdac1, DAC_CHANNEL_1) != HAL_OK)
        {
            (void)app_calibration_board_configure_dac(DAC_TRIGGER_T6_TRGO);
            return APP_CALIBRATION_PORT_IO_ERROR;
        }
        s_board.dac_direct_mode = true;
    }

    uint32_t code = (uint32_t)(
        (target_value * 4095.0f / APP_CALIBRATION_DAC_NOMINAL_VDDA)
        + 0.5f);
    if (code > 4095U)
    {
        code = 4095U;
    }
    if (HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1,
                         DAC_ALIGN_12B_R, code) != HAL_OK)
    {
        return APP_CALIBRATION_PORT_IO_ERROR;
    }
    s_board.status.output_volts_nominal = target_value;
    return APP_CALIBRATION_PORT_OK;
}

static bool app_calibration_board_adc_is_configured(void *context)
{
    (void)context;
    AD9226_Stats stats = {0};
    AD9226_GetStats(&stats);
    return stats.initialized;
}

static uint32_t app_calibration_board_ms_to_ticks(uint32_t milliseconds)
{
    uint32_t frequency_hz = osKernelGetTickFreq();
    if (frequency_hz == 0U)
    {
        return milliseconds;
    }
    uint32_t ticks = (uint32_t)(
        (((uint64_t)milliseconds * frequency_hz) + 999ULL) / 1000ULL);
    return (ticks == 0U) ? 1U : ticks;
}

static uint32_t app_calibration_board_preview_write_begin(void)
{
    uint32_t sequence = s_board.adc_preview_write_sequence;
    if ((sequence & 1U) != 0U)
    {
        ++sequence;
    }
    ++sequence;
    s_board.adc_preview_write_sequence = sequence;
    __DMB();
    return sequence;
}

static void app_calibration_board_preview_write_end(uint32_t sequence)
{
    s_board.adc_preview.sequence = sequence + 1U;
    __DMB();
    s_board.adc_preview_write_sequence = sequence + 1U;
    __DMB();
}

static void app_calibration_board_invalidate_adc_preview(void)
{
    uint32_t sequence = app_calibration_board_preview_write_begin();
    s_board.adc_preview.point_count = 0U;
    s_board.adc_preview.source_sample_count = 0U;
    s_board.adc_preview.sample_rate_hz = 0U;
    s_board.adc_preview.valid = false;
    app_calibration_board_preview_write_end(sequence);
}

static void app_calibration_board_store_adc_preview(
    const uint16_t *samples,
    uint32_t sample_count,
    uint32_t sample_rate_hz)
{
    if (!s_board.adc_preview_enabled || (samples == NULL)
        || (sample_count == 0U) || (sample_rate_hz == 0U))
    {
        return;
    }

    uint32_t window_count =
        sample_count / APP_CALIBRATION_ADC_PREVIEW_WINDOW_DIVIDER;
    if (window_count == 0U)
    {
        window_count = sample_count;
    }

    uint32_t point_count = window_count;
    if (point_count > APP_CALIBRATION_ADC_PREVIEW_POINTS)
    {
        point_count = APP_CALIBRATION_ADC_PREVIEW_POINTS;
    }

    uint32_t sequence = app_calibration_board_preview_write_begin();
    for (uint32_t index = 0U; index < point_count; ++index)
    {
        uint32_t source_index = 0U;
        if (point_count > 1U)
        {
            source_index = (uint32_t)(
                ((uint64_t)index * (uint64_t)(window_count - 1U))
                / (uint64_t)(point_count - 1U));
        }
        s_board.adc_preview.samples[index] =
            AD9226_RawToCode(samples[source_index]);
    }
    s_board.adc_preview.point_count = (uint16_t)point_count;
    s_board.adc_preview.source_sample_count = window_count;
    s_board.adc_preview.sample_rate_hz = sample_rate_hz;
    s_board.adc_preview.valid = true;
    app_calibration_board_preview_write_end(sequence);
}

static AppCalibrationPortStatus app_calibration_board_adc_stop(void *context)
{
    (void)context;
    AD9226_Stats stats = {0};
    AD9226_GetStats(&stats);
    if (!stats.initialized)
    {
        return APP_CALIBRATION_PORT_NOT_CONFIGURED;
    }
    return (AD9226_Stop() == AD9226_STATUS_OK)
        ? APP_CALIBRATION_PORT_OK : APP_CALIBRATION_PORT_IO_ERROR;
}

static AppCalibrationPortStatus app_calibration_board_adc_capture_point(
    void *context,
    uint32_t source_id,
    CalibrationMeasurementPoint *point)
{
    (void)context;
    if ((point == NULL)
        || (source_id != (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC))
    {
        return APP_CALIBRATION_PORT_OUT_OF_RANGE;
    }

    uint32_t actual_rate_hz = 0U;
    if ((AD9226_SetSampleRate(APP_CALIBRATION_ADC_SAMPLE_RATE_HZ,
                              &actual_rate_hz) != AD9226_STATUS_OK)
        || (actual_rate_hz != APP_CALIBRATION_ADC_SAMPLE_RATE_HZ)
        || (AD9226_Start() != AD9226_STATUS_OK))
    {
        return APP_CALIBRATION_PORT_IO_ERROR;
    }

    AD9226_Frame frame = {0};
    uint32_t start_tick = osKernelGetTickCount();
    uint32_t timeout_ticks = app_calibration_board_ms_to_ticks(
        APP_CALIBRATION_ADC_CAPTURE_TIMEOUT_MS);
    while (!AD9226_AcquireLatestFrame(&frame))
    {
        if ((osKernelGetTickCount() - start_tick)
            >= timeout_ticks)
        {
            (void)AD9226_Stop();
            return APP_CALIBRATION_PORT_IO_ERROR;
        }
        osDelay(1U);
    }
    if (AD9226_Stop() != AD9226_STATUS_OK)
    {
        return APP_CALIBRATION_PORT_IO_ERROR;
    }

    s_board.last_frame_samples = frame.samples;
    s_board.last_frame_sample_count = frame.sample_count;
    app_calibration_board_store_adc_preview(
        frame.samples, frame.sample_count, actual_rate_hz);

    uint64_t sum = 0ULL;
    uint16_t minimum = AD9226_SAMPLE_MASK;
    uint16_t maximum = 0U;
    uint32_t low_edge_count = 0U;
    uint32_t high_edge_count = 0U;
    for (uint32_t index = 0U; index < frame.sample_count; ++index)
    {
        uint16_t code = AD9226_RawToCode(frame.samples[index]);
        sum += code;
        if (code < minimum)
        {
            minimum = code;
        }
        if (code > maximum)
        {
            maximum = code;
        }
        if (code <= APP_CALIBRATION_ADC_EDGE_CODE)
        {
            ++low_edge_count;
        }
        if (code >= (AD9226_SAMPLE_MASK - APP_CALIBRATION_ADC_EDGE_CODE))
        {
            ++high_edge_count;
        }
    }

    s_board.status.sample_count = frame.sample_count;
    s_board.status.minimum_code = minimum;
    s_board.status.maximum_code = maximum;
    s_board.status.low_edge_count = low_edge_count;
    s_board.status.high_edge_count = high_edge_count;
    s_board.status.mean_code = (frame.sample_count != 0U)
        ? ((float)sum / (float)frame.sample_count) : 0.0f;

    point->measured_value = s_board.status.mean_code;
    point->frequency_hz = 0U;
    point->phase_deg = 0.0f;
    point->flags = 0U;
    if (low_edge_count != 0U)
    {
        point->flags |= APP_CALIBRATION_POINT_FLAG_LOW_EDGE;
    }
    if (high_edge_count != 0U)
    {
        point->flags |= APP_CALIBRATION_POINT_FLAG_HIGH_EDGE;
    }
    return APP_CALIBRATION_PORT_OK;
}

static bool app_calibration_board_tick_reached(uint32_t now,
                                                uint32_t deadline)
{
    return ((int32_t)(now - deadline) >= 0);
}

static uint32_t app_calibration_board_millivolts(float volts)
{
    return (volts <= 0.0f) ? 0U : (uint32_t)((volts * 1000.0f) + 0.5f);
}

static uint32_t app_calibration_board_millicodes(float code)
{
    return (code <= 0.0f) ? 0U : (uint32_t)((code * 1000.0f) + 0.5f);
}

static bool app_calibration_board_edge_clipped(void)
{
    if ((s_board.status.sample_count == 0U)
        || (s_board.status.response_direction == 0))
    {
        return false;
    }

    uint32_t allowed = (s_board.status.sample_count
                        * APP_CALIBRATION_ADC_EDGE_PERCENT) / 100U;
    return (s_board.status.response_direction > 0)
        ? (s_board.status.high_edge_count > allowed)
        : (s_board.status.low_edge_count > allowed);
}

static void app_calibration_board_finish_loop(
    AppCalibrationLoopStopReason reason)
{
    s_board.status.stop_reason = reason;
    s_board.status.active = false;
    (void)app_calibration_board_adc_stop(NULL);
    (void)app_calibration_board_dac_stop_all(NULL);
    (void)app_calibration_board_relay_safe(NULL);
    (void)AppCalibrationService_RequestFinishSession();
    (void)AppOutput_Logf(
        "[CAL] STOP reason=%s points=%lu output_mv=%lu mean=%lu.%03lu min=%u max=%u\r\n",
        AppCalibrationBoardPort_StopReasonString(reason),
        (unsigned long)s_board.status.point_count,
        (unsigned long)app_calibration_board_millivolts(
            s_board.status.output_volts_nominal),
        (unsigned long)(app_calibration_board_millicodes(
            s_board.status.mean_code) / 1000U),
        (unsigned long)(app_calibration_board_millicodes(
            s_board.status.mean_code) % 1000U),
        (unsigned int)s_board.status.minimum_code,
        (unsigned int)s_board.status.maximum_code);
}

static bool app_calibration_board_begin_loop(void)
{
    memset(&s_board.status, 0, sizeof(s_board.status));
    s_board.baseline_mean_code = 0.0f;
    s_board.previous_mean_code = 0.0f;
    s_board.plateau_count = 0U;
    s_board.baseline_valid = false;
    s_board.limits.lower_limit = APP_CALIBRATION_DAC_LOWER_VOLTS;
    s_board.limits.upper_limit = APP_CALIBRATION_DAC_UPPER_VOLTS;
    s_board.limits.max_step = APP_CALIBRATION_DAC_MAX_STEP_VOLTS;
    s_board.limits.dwell_time_ms = APP_CALIBRATION_DAC_DWELL_MS;

    if (app_calibration_board_dac_set_output(
            NULL,
            (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC,
            s_board.limits.lower_limit) != APP_CALIBRATION_PORT_OK)
    {
        return false;
    }

    s_board.status.active = true;
    s_board.next_action_tick = osKernelGetTickCount()
        + app_calibration_board_ms_to_ticks(s_board.limits.dwell_time_ms);
    (void)AppOutput_Logf(
        "[CAL] LOOP START lower_mv=0 upper_mv=3000 step_mv=50 dwell_ms=25 samples=4096\r\n");
    return true;
}

static uint32_t app_calibration_board_dac_code(float volts)
{
    if (volts <= 0.0f)
    {
        return 0U;
    }
    uint32_t code = (uint32_t)(
        (volts * 4095.0f / APP_CALIBRATION_DAC_NOMINAL_VDDA) + 0.5f);
    return (code > 4095U) ? 4095U : code;
}

static bool app_calibration_board_self_test_submit(uint8_t level)
{
    if (level >= CALIBRATION_SELF_TEST_LEVEL_COUNT)
    {
        return false;
    }

    const CalibrationSelfTestResult *result =
        &s_board.self_test.results[level];
    CalibrationMeasurementPoint point = {
        .reference_value = result->requested_level_volts,
        .measured_value = result->measured_level_volts,
        .frequency_hz = (level >= CALIBRATION_SELF_TEST_AC_QUARTER)
            ? APP_CALIBRATION_SELF_TEST_AC_FREQUENCY_HZ : 0U,
        .phase_deg = 0.0f,
        .flags = ((uint32_t)result->state << 8U)
            | (uint32_t)result->reason,
    };
    if (!AppCalibrationService_SubmitMeasurementPoint(&point))
    {
        return false;
    }

    ++s_board.self_test.completed_count;
    (void)AppOutput_Logf(
        "[CAL] SELF level=%u state=%s reason=%s request_mv=%lu measured_mv=%lu amp_err=%ld.%02lu noise=%lu.%03lu min=%u max=%u\r\n",
        (unsigned int)level,
        CalibrationSelfTest_StateString(result->state),
        CalibrationSelfTest_ReasonString(result->reason),
        (unsigned long)app_calibration_board_millivolts(
            result->requested_level_volts),
        (unsigned long)app_calibration_board_millivolts(
            result->measured_level_volts),
        (long)result->amplitude_error_percent,
        (unsigned long)(fabsf(result->amplitude_error_percent
                              - (float)((long)result->amplitude_error_percent))
                        * 100.0f),
        (unsigned long)(app_calibration_board_millicodes(
            result->noise_rms_code) / 1000U),
        (unsigned long)(app_calibration_board_millicodes(
            result->noise_rms_code) % 1000U),
        (unsigned int)result->minimum_code,
        (unsigned int)result->maximum_code);
    return true;
}

static void app_calibration_board_self_test_finish(void)
{
    (void)app_calibration_board_adc_stop(NULL);
    (void)app_calibration_board_dac_stop_all(NULL);
    (void)app_calibration_board_relay_safe(NULL);
    s_board.self_test.active = false;
    s_board.self_test_phase = APP_CALIBRATION_SELF_TEST_PHASE_IDLE;
    (void)AppCalibrationService_RequestFinishSession();
}

static void app_calibration_board_self_test_fail_remaining(
    CalibrationSelfTestReason reason)
{
    uint8_t first = s_board.self_test.active_level;
    if (first >= CALIBRATION_SELF_TEST_LEVEL_COUNT)
    {
        app_calibration_board_self_test_finish();
        return;
    }
    for (uint8_t level = first;
         level < CALIBRATION_SELF_TEST_LEVEL_COUNT;
         ++level)
    {
        CalibrationSelfTestResult *result =
            &s_board.self_test.results[level];
        result->state = (level == first)
            ? CALIBRATION_SELF_TEST_ABNORMAL
            : CALIBRATION_SELF_TEST_BLOCKED;
        result->reason = reason;
        result->output_ok = false;
        result->amplitude_ok = false;
        result->noise_ok = false;
        if (!app_calibration_board_self_test_submit(level))
        {
            break;
        }
    }
    app_calibration_board_self_test_finish();
}

static bool app_calibration_board_self_test_capture(
    CalibrationMeasurementPoint *point)
{
    if (app_calibration_board_adc_capture_point(
            NULL,
            (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC,
            point) != APP_CALIBRATION_PORT_OK)
    {
        return false;
    }
    return (s_board.last_frame_samples != NULL)
        && (s_board.last_frame_sample_count != 0U);
}

static bool app_calibration_board_self_test_start_ac(uint8_t level)
{
    if ((level < CALIBRATION_SELF_TEST_AC_QUARTER)
        || (level > CALIBRATION_SELF_TEST_AC_FULL))
    {
        return false;
    }

    (void)app_calibration_board_dac_stop_all(NULL);
    uint16_t center_code = (uint16_t)app_calibration_board_dac_code(
        APP_CALIBRATION_SELF_TEST_AC_OFFSET_VOLTS);
    uint16_t peak_code = (uint16_t)app_calibration_board_dac_code(
        s_self_test_levels[level] * 0.5f);
    AppDacOutputConfig config = {0};
    config.channels[0].frequency_hz =
        APP_CALIBRATION_SELF_TEST_AC_FREQUENCY_HZ;
    config.channels[0].wave_type = SIGNAL_WAVE_SINE;
    config.channels[0].center_code = center_code;
    config.channels[0].peak_code = peak_code;
    config.channels[0].use_explicit_level = true;
    config.channels[1].frequency_hz =
        APP_CALIBRATION_SELF_TEST_AC_FREQUENCY_HZ;
    config.channels[1].wave_type = SIGNAL_WAVE_SINE;
    config.channels[1].center_code = 0U;
    config.channels[1].peak_code = 0U;
    config.channels[1].use_explicit_level = true;
    if (!AppDacOutput_Start(&config))
    {
        return false;
    }

    s_board.self_test.active_level = level;
    s_board.self_test.results[level].state = CALIBRATION_SELF_TEST_RUNNING;
    s_board.self_test_phase = APP_CALIBRATION_SELF_TEST_PHASE_AC_CAPTURE;
    s_board.self_test_next_action_tick = osKernelGetTickCount()
        + app_calibration_board_ms_to_ticks(
            APP_CALIBRATION_SELF_TEST_AC_DWELL_MS);
    return true;
}

static void app_calibration_board_self_test_apply_resume(void)
{
    AppCalibrationSelfTestPhase resume_phase =
        (AppCalibrationSelfTestPhase)s_board.self_test_resume_phase;
    uint8_t resume_level = s_board.self_test_resume_level;

    if (resume_phase == APP_CALIBRATION_SELF_TEST_PHASE_IDLE)
    {
        app_calibration_board_self_test_finish();
        return;
    }
    if (resume_phase == APP_CALIBRATION_SELF_TEST_PHASE_DC_RAMP)
    {
        s_board.self_test.active_level = resume_level;
        s_board.self_test.results[resume_level].state =
            CALIBRATION_SELF_TEST_RUNNING;
        s_board.self_test_phase = APP_CALIBRATION_SELF_TEST_PHASE_DC_RAMP;
        s_board.self_test_next_action_tick = osKernelGetTickCount();
        return;
    }
    if (resume_phase == APP_CALIBRATION_SELF_TEST_PHASE_RAMP_TO_AC)
    {
        s_board.self_test_phase =
            APP_CALIBRATION_SELF_TEST_PHASE_RAMP_TO_AC;
        s_board.self_test_next_action_tick = osKernelGetTickCount();
        return;
    }
    if (resume_phase == APP_CALIBRATION_SELF_TEST_PHASE_AC_START)
    {
        if (!app_calibration_board_self_test_start_ac(resume_level))
        {
            s_board.self_test.active_level = resume_level;
            app_calibration_board_self_test_fail_remaining(
                CALIBRATION_SELF_TEST_REASON_OUTPUT_IO);
        }
        return;
    }

    app_calibration_board_self_test_fail_remaining(
        CALIBRATION_SELF_TEST_REASON_INVALID_DATA);
}

static void app_calibration_board_self_test_resume_after_preview(
    AppCalibrationSelfTestPhase resume_phase,
    uint8_t resume_level)
{
    s_board.self_test_resume_phase = (uint8_t)resume_phase;
    s_board.self_test_resume_level = resume_level;

    if (s_board.adc_preview_enabled && s_board.adc_preview.valid)
    {
        s_board.self_test_phase =
            APP_CALIBRATION_SELF_TEST_PHASE_WAVEFORM_HOLD;
        s_board.self_test_next_action_tick = osKernelGetTickCount()
            + app_calibration_board_ms_to_ticks(
                APP_CALIBRATION_SELF_TEST_WAVEFORM_HOLD_MS);
        return;
    }
    app_calibration_board_self_test_apply_resume();
}

static bool app_calibration_board_self_test_begin(void)
{
    memset(&s_board.self_test, 0, sizeof(s_board.self_test));
    for (uint8_t level = 0U;
         level < CALIBRATION_SELF_TEST_LEVEL_COUNT;
         ++level)
    {
        CalibrationSelfTest_ResultInit(&s_board.self_test.results[level],
                                       (CalibrationSelfTestLevel)level,
                                       s_self_test_levels[level]);
    }
    s_board.self_test.criteria_provisional = true;
    s_board.self_test.active_level = CALIBRATION_SELF_TEST_DC_QUARTER;
    s_board.self_test.active = true;
    s_board.self_test_current_output_volts = 0.0f;
    s_board.self_test_previous_mean_code = 0.0f;
    s_board.self_test_baseline_mean_code = 0.0f;
    s_board.self_test_response_direction = 0;
    s_board.self_test_plateau_count = 0U;
    s_board.self_test_resume_phase =
        APP_CALIBRATION_SELF_TEST_PHASE_IDLE;
    s_board.self_test_resume_level = 0U;
    s_board.limits.lower_limit = APP_CALIBRATION_DAC_LOWER_VOLTS;
    s_board.limits.upper_limit = APP_CALIBRATION_DAC_UPPER_VOLTS;
    s_board.limits.max_step = APP_CALIBRATION_DAC_MAX_STEP_VOLTS;
    s_board.limits.dwell_time_ms = APP_CALIBRATION_DAC_DWELL_MS;
    s_board.last_frame_samples = NULL;
    s_board.last_frame_sample_count = 0U;
    if (s_board.adc_preview_enabled)
    {
        app_calibration_board_invalidate_adc_preview();
    }

    if (app_calibration_board_dac_set_output(
            NULL,
            (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC,
            0.0f) != APP_CALIBRATION_PORT_OK)
    {
        return false;
    }
    s_board.self_test_phase = APP_CALIBRATION_SELF_TEST_PHASE_BASELINE;
    s_board.self_test_next_action_tick = osKernelGetTickCount()
        + app_calibration_board_ms_to_ticks(APP_CALIBRATION_DAC_DWELL_MS);
    (void)AppOutput_Logf(
        "[CAL] SELF START dc_full_mv=3000 ac_full_vpp_mv=800 ac_offset_mv=500 ac_hz=20000 criteria=provisional\r\n");
    return true;
}

static void app_calibration_board_self_test_block_dc_from(uint8_t first)
{
    for (uint8_t level = first;
         level <= CALIBRATION_SELF_TEST_DC_FULL;
         ++level)
    {
        CalibrationSelfTestResult *result =
            &s_board.self_test.results[level];
        result->state = CALIBRATION_SELF_TEST_BLOCKED;
        result->reason = CALIBRATION_SELF_TEST_REASON_TRANSFER_PLATEAU;
        result->output_ok = false;
        result->amplitude_ok = false;
        result->noise_ok = false;
        if (!app_calibration_board_self_test_submit(level))
        {
            app_calibration_board_self_test_finish();
            return;
        }
    }
    app_calibration_board_self_test_resume_after_preview(
        APP_CALIBRATION_SELF_TEST_PHASE_RAMP_TO_AC,
        CALIBRATION_SELF_TEST_AC_QUARTER);
}

static void app_calibration_board_self_test_run_dc_capture(void)
{
    CalibrationMeasurementPoint point = {
        .reference_value = s_board.self_test_current_output_volts,
    };
    if (!app_calibration_board_self_test_capture(&point))
    {
        app_calibration_board_self_test_fail_remaining(
            CALIBRATION_SELF_TEST_REASON_OUTPUT_IO);
        return;
    }

    float delta = point.measured_value
        - s_board.self_test_previous_mean_code;
    float from_baseline = point.measured_value
        - s_board.self_test_baseline_mean_code;
    if ((s_board.self_test_response_direction == 0)
        && (fabsf(from_baseline) >= APP_CALIBRATION_RESPONSE_MIN_CODE))
    {
        s_board.self_test_response_direction =
            (from_baseline > 0.0f) ? 1 : -1;
    }
    if (s_board.self_test_response_direction != 0)
    {
        if (fabsf(delta) < APP_CALIBRATION_PLATEAU_DELTA_CODE)
        {
            ++s_board.self_test_plateau_count;
        }
        else
        {
            s_board.self_test_plateau_count = 0U;
        }
    }
    s_board.self_test_previous_mean_code = point.measured_value;

    if (s_board.self_test_plateau_count >= APP_CALIBRATION_PLATEAU_LIMIT)
    {
        app_calibration_board_self_test_block_dc_from(
            s_board.self_test.active_level);
        return;
    }

    uint8_t level = s_board.self_test.active_level;
    float target = s_self_test_levels[level];
    if (s_board.self_test_current_output_volts + 0.0001f < target)
    {
        s_board.self_test_phase = APP_CALIBRATION_SELF_TEST_PHASE_DC_RAMP;
        return;
    }

    CalibrationSelfTestResult *result =
        &s_board.self_test.results[level];
    if (CalibrationSelfTest_AnalyzeDc(
            s_board.last_frame_samples,
            s_board.last_frame_sample_count,
            s_board.self_test_baseline_mean_code,
            target,
            &s_self_test_criteria,
            result) != CALIBRATION_CORE_OK)
    {
        result->state = CALIBRATION_SELF_TEST_ABNORMAL;
        result->reason = CALIBRATION_SELF_TEST_REASON_INVALID_DATA;
    }
    if (!app_calibration_board_self_test_submit(level))
    {
        app_calibration_board_self_test_finish();
        return;
    }

    if (level < CALIBRATION_SELF_TEST_DC_FULL)
    {
        app_calibration_board_self_test_resume_after_preview(
            APP_CALIBRATION_SELF_TEST_PHASE_DC_RAMP,
            level + 1U);
    }
    else
    {
        app_calibration_board_self_test_resume_after_preview(
            APP_CALIBRATION_SELF_TEST_PHASE_RAMP_TO_AC,
            CALIBRATION_SELF_TEST_AC_QUARTER);
    }
}

static void app_calibration_board_self_test_run(void)
{
    if (!s_board.self_test.active)
    {
        if (!app_calibration_board_self_test_begin())
        {
            app_calibration_board_self_test_fail_remaining(
                CALIBRATION_SELF_TEST_REASON_OUTPUT_IO);
        }
        return;
    }

    if (s_board.self_test.active_level >= CALIBRATION_SELF_TEST_LEVEL_COUNT)
    {
        app_calibration_board_self_test_fail_remaining(
            CALIBRATION_SELF_TEST_REASON_INVALID_DATA);
        return;
    }

    uint32_t now = osKernelGetTickCount();
    bool preview_hold_cancelled =
        (s_board.self_test_phase
         == APP_CALIBRATION_SELF_TEST_PHASE_WAVEFORM_HOLD)
        && !s_board.adc_preview_enabled;
    if (!preview_hold_cancelled
        && !app_calibration_board_tick_reached(
            now, s_board.self_test_next_action_tick))
    {
        return;
    }

    switch ((AppCalibrationSelfTestPhase)s_board.self_test_phase)
    {
        case APP_CALIBRATION_SELF_TEST_PHASE_BASELINE:
        {
            CalibrationMeasurementPoint point = {0};
            if (!app_calibration_board_self_test_capture(&point))
            {
                app_calibration_board_self_test_fail_remaining(
                    CALIBRATION_SELF_TEST_REASON_OUTPUT_IO);
                return;
            }
            s_board.self_test_baseline_mean_code = point.measured_value;
            s_board.self_test_previous_mean_code = point.measured_value;
            s_board.self_test.results[0].state =
                CALIBRATION_SELF_TEST_RUNNING;
            s_board.self_test_phase = APP_CALIBRATION_SELF_TEST_PHASE_DC_RAMP;
            break;
        }

        case APP_CALIBRATION_SELF_TEST_PHASE_DC_RAMP:
        {
            float next_value = s_board.self_test_current_output_volts;
            bool reached = false;
            if ((CalibrationCore_NextRampValue(
                    &s_board.limits,
                    s_board.self_test_current_output_volts,
                    s_self_test_levels[s_board.self_test.active_level],
                    &next_value,
                    &reached) != CALIBRATION_CORE_OK)
                || (app_calibration_board_dac_set_output(
                        NULL,
                        (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC,
                        next_value) != APP_CALIBRATION_PORT_OK))
            {
                app_calibration_board_self_test_fail_remaining(
                    CALIBRATION_SELF_TEST_REASON_OUTPUT_IO);
                return;
            }
            s_board.self_test_current_output_volts = next_value;
            s_board.self_test_phase =
                APP_CALIBRATION_SELF_TEST_PHASE_DC_CAPTURE;
            s_board.self_test_next_action_tick = now
                + app_calibration_board_ms_to_ticks(
                    APP_CALIBRATION_DAC_DWELL_MS);
            break;
        }

        case APP_CALIBRATION_SELF_TEST_PHASE_DC_CAPTURE:
            app_calibration_board_self_test_run_dc_capture();
            break;

        case APP_CALIBRATION_SELF_TEST_PHASE_RAMP_TO_AC:
        {
            if (s_board.self_test_current_output_volts > 0.0001f)
            {
                float next_value = s_board.self_test_current_output_volts
                    - APP_CALIBRATION_DAC_MAX_STEP_VOLTS;
                if (next_value < 0.0f)
                {
                    next_value = 0.0f;
                }
                if (app_calibration_board_dac_set_output(
                        NULL,
                        (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC,
                        next_value) != APP_CALIBRATION_PORT_OK)
                {
                    app_calibration_board_self_test_fail_remaining(
                        CALIBRATION_SELF_TEST_REASON_OUTPUT_IO);
                    return;
                }
                s_board.self_test_current_output_volts = next_value;
                s_board.self_test_next_action_tick = now
                    + app_calibration_board_ms_to_ticks(
                        APP_CALIBRATION_DAC_DWELL_MS);
                return;
            }
            if (!app_calibration_board_self_test_start_ac(
                    CALIBRATION_SELF_TEST_AC_QUARTER))
            {
                s_board.self_test.active_level =
                    CALIBRATION_SELF_TEST_AC_QUARTER;
                app_calibration_board_self_test_fail_remaining(
                    CALIBRATION_SELF_TEST_REASON_OUTPUT_IO);
            }
            break;
        }

        case APP_CALIBRATION_SELF_TEST_PHASE_AC_CAPTURE:
        {
            uint8_t level = s_board.self_test.active_level;
            CalibrationMeasurementPoint point = {
                .frequency_hz = APP_CALIBRATION_SELF_TEST_AC_FREQUENCY_HZ,
            };
            if (!app_calibration_board_self_test_capture(&point))
            {
                app_calibration_board_self_test_fail_remaining(
                    CALIBRATION_SELF_TEST_REASON_OUTPUT_IO);
                return;
            }
            AppDacOutput_Stop();
            CalibrationSelfTestResult *result =
                &s_board.self_test.results[level];
            if (CalibrationSelfTest_AnalyzeAc(
                    s_board.last_frame_samples,
                    s_board.last_frame_sample_count,
                    APP_CALIBRATION_ADC_SAMPLE_RATE_HZ,
                    APP_CALIBRATION_SELF_TEST_AC_FREQUENCY_HZ,
                    s_self_test_levels[level],
                    &s_self_test_criteria,
                    result) != CALIBRATION_CORE_OK)
            {
                result->state = CALIBRATION_SELF_TEST_ABNORMAL;
                result->reason = CALIBRATION_SELF_TEST_REASON_INVALID_DATA;
            }
            if (!app_calibration_board_self_test_submit(level))
            {
                app_calibration_board_self_test_finish();
                return;
            }
            if (level < CALIBRATION_SELF_TEST_AC_FULL)
            {
                app_calibration_board_self_test_resume_after_preview(
                    APP_CALIBRATION_SELF_TEST_PHASE_AC_START,
                    level + 1U);
            }
            else
            {
                app_calibration_board_self_test_resume_after_preview(
                    APP_CALIBRATION_SELF_TEST_PHASE_IDLE,
                    level);
            }
            break;
        }

        case APP_CALIBRATION_SELF_TEST_PHASE_WAVEFORM_HOLD:
            app_calibration_board_self_test_apply_resume();
            break;

        case APP_CALIBRATION_SELF_TEST_PHASE_AC_START:
        case APP_CALIBRATION_SELF_TEST_PHASE_IDLE:
        default:
            app_calibration_board_self_test_fail_remaining(
                CALIBRATION_SELF_TEST_REASON_INVALID_DATA);
            break;
    }
}

void AppCalibrationBoardPort_Register(void)
{
    AppCalibrationPorts ports = {0};
    ports.relay.is_configured = app_calibration_board_relay_is_configured;
    ports.relay.supports_source = app_calibration_board_relay_supports_source;
    ports.relay.enter_safe_state = app_calibration_board_relay_safe;
    ports.relay.route_source = app_calibration_board_relay_route;
    ports.dac.is_configured = app_calibration_board_dac_is_configured;
    ports.dac.stop_all = app_calibration_board_dac_stop_all;
    ports.dac.get_limits = app_calibration_board_dac_get_limits;
    ports.dac.set_output = app_calibration_board_dac_set_output;
    ports.adc.is_configured = app_calibration_board_adc_is_configured;
    ports.adc.stop = app_calibration_board_adc_stop;
    ports.adc.capture_point = app_calibration_board_adc_capture_point;

    /* QSPI 初始化失败时仍注册 Port，由 is_ready 保持硬门禁和错误显示。 */
    (void)AppCalibrationStoragePort_Init();
    AppCalibrationStoragePort_Fill(&ports.storage);

    AppCalibrationService_ConfigurePorts(&ports);
    memset(&s_board, 0, sizeof(s_board));
    (void)app_calibration_board_relay_safe(NULL);
}

/*
 * “ADC DISPLAY ON”时在 READY/REVIEW 空闲态周期性采一帧实时波形：
 * 1. 测试开始前可确认 ADC 链路有信号；
 * 2. 自检结束（REVIEW）后预览窗口保持活动，不再停在 WAIT FOR ADC。
 * 仅在校准独占硬件时执行，绝不触碰正常分析流程的 ADC。
 */
static void app_calibration_board_run_idle_preview(
    const AppCalibrationSnapshot *snapshot)
{
    static uint32_t s_next_idle_preview_tick = 0U;

    if (!s_board.adc_preview_enabled
        || !snapshot->exclusive
        || ((snapshot->state != APP_CALIBRATION_STATE_READY)
            && (snapshot->state != APP_CALIBRATION_STATE_REVIEW)))
    {
        return;
    }

    uint32_t now = osKernelGetTickCount();
    if (!app_calibration_board_tick_reached(now,
                                            s_next_idle_preview_tick))
    {
        return;
    }
    s_next_idle_preview_tick = now
        + app_calibration_board_ms_to_ticks(
            APP_CALIBRATION_ADC_PREVIEW_IDLE_PERIOD_MS);

    /* 只读取 ADC，不驱动 DAC/继电器；采集失败静默等待下个周期。 */
    CalibrationMeasurementPoint point = {0};
    (void)app_calibration_board_adc_capture_point(
        NULL,
        (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC,
        &point);
}

void AppCalibrationBoardPort_Run(void)
{
    AppCalibrationSnapshot snapshot = {0};
    AppCalibrationService_GetSnapshot(&snapshot);

    bool internal_dac1_session =
        (snapshot.state == APP_CALIBRATION_STATE_SESSION_ACTIVE)
        && (snapshot.mode == APP_CALIBRATION_MODE_INTERNAL)
        && (snapshot.source == APP_CALIBRATION_SOURCE_DAC1_TO_ADC);
    bool output_self_test_session = internal_dac1_session
        && (snapshot.active_item_id
            == (uint32_t)APP_CALIBRATION_ITEM_OUTPUT_SELF_TEST);
    if (!internal_dac1_session)
    {
        if (s_board.self_test.active)
        {
            if (s_board.self_test.active_level
                < CALIBRATION_SELF_TEST_LEVEL_COUNT)
            {
                CalibrationSelfTestResult *result =
                    &s_board.self_test.results[
                        s_board.self_test.active_level];
                result->state = CALIBRATION_SELF_TEST_ABORTED;
                result->reason = CALIBRATION_SELF_TEST_REASON_USER_ABORT;
            }
            s_board.self_test.active = false;
            s_board.self_test_phase = APP_CALIBRATION_SELF_TEST_PHASE_IDLE;
            (void)app_calibration_board_adc_stop(NULL);
            (void)app_calibration_board_dac_stop_all(NULL);
            (void)app_calibration_board_relay_safe(NULL);
        }
        if (s_board.status.active)
        {
            s_board.status.stop_reason = APP_CALIBRATION_LOOP_STOP_ABORTED;
            s_board.status.active = false;
            (void)app_calibration_board_adc_stop(NULL);
            (void)app_calibration_board_dac_stop_all(NULL);
            (void)app_calibration_board_relay_safe(NULL);
        }
        app_calibration_board_run_idle_preview(&snapshot);
        return;
    }

    if (output_self_test_session)
    {
        if (s_board.status.active)
        {
            s_board.status.active = false;
            (void)app_calibration_board_adc_stop(NULL);
            (void)app_calibration_board_dac_stop_all(NULL);
        }
        app_calibration_board_self_test_run();
        return;
    }

    if (s_board.self_test.active)
    {
        s_board.self_test.active = false;
        s_board.self_test_phase = APP_CALIBRATION_SELF_TEST_PHASE_IDLE;
        (void)app_calibration_board_adc_stop(NULL);
        (void)app_calibration_board_dac_stop_all(NULL);
    }

    if (!s_board.status.active)
    {
        if (!app_calibration_board_begin_loop())
        {
            app_calibration_board_finish_loop(
                APP_CALIBRATION_LOOP_STOP_IO_ERROR);
        }
        return;
    }

    uint32_t now = osKernelGetTickCount();
    if (!app_calibration_board_tick_reached(now, s_board.next_action_tick))
    {
        return;
    }

    CalibrationMeasurementPoint point = {
        .reference_value = s_board.status.output_volts_nominal,
    };
    if (app_calibration_board_adc_capture_point(
            NULL,
            (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC,
            &point) != APP_CALIBRATION_PORT_OK)
    {
        app_calibration_board_finish_loop(APP_CALIBRATION_LOOP_STOP_IO_ERROR);
        return;
    }

    float delta = 0.0f;
    if (!s_board.baseline_valid)
    {
        s_board.baseline_mean_code = point.measured_value;
        s_board.baseline_valid = true;
    }
    else
    {
        delta = point.measured_value - s_board.previous_mean_code;
        float from_baseline = point.measured_value
            - s_board.baseline_mean_code;
        if ((s_board.status.response_direction == 0)
            && (fabsf(from_baseline) >= APP_CALIBRATION_RESPONSE_MIN_CODE))
        {
            s_board.status.response_direction =
                (from_baseline > 0.0f) ? 1 : -1;
        }
    }

    if (s_board.status.response_direction != 0)
    {
        if ((delta * (float)s_board.status.response_direction)
            < -APP_CALIBRATION_REVERSE_TOLERANCE_CODE)
        {
            app_calibration_board_finish_loop(
                APP_CALIBRATION_LOOP_STOP_NON_MONOTONIC);
            return;
        }
        if (fabsf(delta) < APP_CALIBRATION_PLATEAU_DELTA_CODE)
        {
            ++s_board.plateau_count;
        }
        else
        {
            s_board.plateau_count = 0U;
        }
    }

    bool edge_clipped = app_calibration_board_edge_clipped();
    bool transfer_plateau =
        s_board.plateau_count >= APP_CALIBRATION_PLATEAU_LIMIT;
    if (edge_clipped)
    {
        point.flags |= APP_CALIBRATION_POINT_FLAG_CLIPPED;
    }
    if (transfer_plateau)
    {
        point.flags |= APP_CALIBRATION_POINT_FLAG_PLATEAU;
    }

    if (!AppCalibrationService_SubmitMeasurementPoint(&point))
    {
        app_calibration_board_finish_loop(APP_CALIBRATION_LOOP_STOP_IO_ERROR);
        return;
    }
    ++s_board.status.point_count;
    s_board.previous_mean_code = point.measured_value;
    (void)AppOutput_Logf(
        "[CAL] POINT n=%lu output_mv=%lu mean=%lu.%03lu min=%u max=%u low=%lu high=%lu dir=%d flags=0x%lx\r\n",
        (unsigned long)s_board.status.point_count,
        (unsigned long)app_calibration_board_millivolts(
            s_board.status.output_volts_nominal),
        (unsigned long)(app_calibration_board_millicodes(
            s_board.status.mean_code) / 1000U),
        (unsigned long)(app_calibration_board_millicodes(
            s_board.status.mean_code) % 1000U),
        (unsigned int)s_board.status.minimum_code,
        (unsigned int)s_board.status.maximum_code,
        (unsigned long)s_board.status.low_edge_count,
        (unsigned long)s_board.status.high_edge_count,
        (int)s_board.status.response_direction,
        (unsigned long)point.flags);

    if (edge_clipped)
    {
        app_calibration_board_finish_loop(
            APP_CALIBRATION_LOOP_STOP_ADC_CLIPPED);
        return;
    }
    if (transfer_plateau)
    {
        app_calibration_board_finish_loop(
            APP_CALIBRATION_LOOP_STOP_TRANSFER_PLATEAU);
        return;
    }
    if ((s_board.status.output_volts_nominal
         >= APP_CALIBRATION_RESPONSE_CHECK_VOLTS)
        && (s_board.status.response_direction == 0))
    {
        app_calibration_board_finish_loop(
            APP_CALIBRATION_LOOP_STOP_NO_RESPONSE);
        return;
    }
    if (s_board.status.output_volts_nominal
        >= s_board.limits.upper_limit)
    {
        app_calibration_board_finish_loop(APP_CALIBRATION_LOOP_STOP_COMPLETE);
        return;
    }

    float next_value = s_board.status.output_volts_nominal;
    bool target_reached = false;
    if ((CalibrationCore_NextRampValue(
            &s_board.limits,
            s_board.status.output_volts_nominal,
            s_board.limits.upper_limit,
            &next_value,
            &target_reached) != CALIBRATION_CORE_OK)
        || (app_calibration_board_dac_set_output(
                NULL,
                (uint32_t)APP_CALIBRATION_SOURCE_DAC1_TO_ADC,
                next_value) != APP_CALIBRATION_PORT_OK))
    {
        app_calibration_board_finish_loop(APP_CALIBRATION_LOOP_STOP_IO_ERROR);
        return;
    }
    s_board.next_action_tick = osKernelGetTickCount()
        + app_calibration_board_ms_to_ticks(s_board.limits.dwell_time_ms);
}

void AppCalibrationBoardPort_GetLoopStatus(AppCalibrationLoopStatus *status)
{
    if (status == NULL)
    {
        return;
    }
    __DMB();
    *status = s_board.status;
    status->relay_output_bits = (uint8_t)(
        (CAL_RELAY_INPUT_SEL_GPIO_Port->ODR & APP_CALIBRATION_RELAY_MASK)
        >> 6U);
    status->dac_output_code = (uint16_t)(DAC1->DOR1 & 0x0FFFU);
    AD9226_Stats adc_stats = {0};
    AD9226_GetStats(&adc_stats);
    status->adc_running = adc_stats.running;
    __DMB();
}

uint32_t AppCalibrationBoardPort_GetExpectedPointCount(void)
{
    return (uint32_t)(
        ((APP_CALIBRATION_DAC_UPPER_VOLTS
          - APP_CALIBRATION_DAC_LOWER_VOLTS)
         / APP_CALIBRATION_DAC_MAX_STEP_VOLTS) + 0.5f) + 1U;
}

uint32_t AppCalibrationBoardPort_GetExpectedPointCountForItem(
    uint32_t item_id)
{
    if (item_id == (uint32_t)APP_CALIBRATION_ITEM_OUTPUT_SELF_TEST)
    {
        return CALIBRATION_SELF_TEST_LEVEL_COUNT;
    }
    return AppCalibrationBoardPort_GetExpectedPointCount();
}

void AppCalibrationBoardPort_GetSelfTestStatus(
    AppCalibrationSelfTestStatus *status)
{
    if (status == NULL)
    {
        return;
    }
    __DMB();
    *status = s_board.self_test;
    __DMB();
}

void AppCalibrationBoardPort_SetAdcPreviewEnabled(bool enabled)
{
    s_board.adc_preview_enabled = enabled;
    __DMB();
}

bool AppCalibrationBoardPort_GetAdcPreviewEnabled(void)
{
    __DMB();
    bool enabled = s_board.adc_preview_enabled;
    __DMB();
    return enabled;
}

bool AppCalibrationBoardPort_GetAdcPreview(
    AppCalibrationAdcPreview *preview)
{
    if (preview == NULL)
    {
        return false;
    }

    for (uint32_t retry = 0U; retry < 4U; ++retry)
    {
        uint32_t sequence_before = s_board.adc_preview_write_sequence;
        if ((sequence_before & 1U) != 0U)
        {
            continue;
        }
        __DMB();
        *preview = s_board.adc_preview;
        __DMB();
        uint32_t sequence_after = s_board.adc_preview_write_sequence;
        if ((sequence_before == sequence_after)
            && ((sequence_after & 1U) == 0U))
        {
            return preview->valid && (preview->point_count != 0U);
        }
    }
    return false;
}

const char *AppCalibrationBoardPort_StopReasonString(
    AppCalibrationLoopStopReason reason)
{
    switch (reason)
    {
        case APP_CALIBRATION_LOOP_STOP_NONE:
            return "none";
        case APP_CALIBRATION_LOOP_STOP_COMPLETE:
            return "complete";
        case APP_CALIBRATION_LOOP_STOP_ADC_CLIPPED:
            return "adc_clipped";
        case APP_CALIBRATION_LOOP_STOP_TRANSFER_PLATEAU:
            return "transfer_plateau";
        case APP_CALIBRATION_LOOP_STOP_NO_RESPONSE:
            return "no_response";
        case APP_CALIBRATION_LOOP_STOP_NON_MONOTONIC:
            return "non_monotonic";
        case APP_CALIBRATION_LOOP_STOP_IO_ERROR:
            return "io_error";
        case APP_CALIBRATION_LOOP_STOP_ABORTED:
            return "aborted";
        default:
            return "unknown";
    }
}
