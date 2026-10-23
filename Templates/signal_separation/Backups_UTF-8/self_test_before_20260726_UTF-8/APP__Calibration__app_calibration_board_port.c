/**
  ******************************************************************************
  * @file    app_calibration_board_port.c
  * @brief   P2-18(PC6)/P2-16(PC7) 高有效校准继电器 Port（UTF-8）。
  ******************************************************************************
  */

#include "app_calibration_board_port.h"

#include "ad9226.h"
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
} AppCalibrationBoardContext;

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

    if (s_board.dac_direct_mode)
    {
        (void)HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1,
                              DAC_ALIGN_12B_R, 0U);
        __DMB();
        (void)HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
        s_board.dac_direct_mode = false;
    }

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

void AppCalibrationBoardPort_Run(void)
{
    AppCalibrationSnapshot snapshot = {0};
    AppCalibrationService_GetSnapshot(&snapshot);

    bool internal_dac1_session =
        (snapshot.state == APP_CALIBRATION_STATE_SESSION_ACTIVE)
        && (snapshot.mode == APP_CALIBRATION_MODE_INTERNAL)
        && (snapshot.source == APP_CALIBRATION_SOURCE_DAC1_TO_ADC);
    if (!internal_dac1_session)
    {
        if (s_board.status.active)
        {
            s_board.status.stop_reason = APP_CALIBRATION_LOOP_STOP_ABORTED;
            s_board.status.active = false;
            (void)app_calibration_board_adc_stop(NULL);
            (void)app_calibration_board_dac_stop_all(NULL);
            (void)app_calibration_board_relay_safe(NULL);
        }
        return;
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
