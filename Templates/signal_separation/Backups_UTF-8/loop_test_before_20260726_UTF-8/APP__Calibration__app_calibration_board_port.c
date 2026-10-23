/**
  ******************************************************************************
  * @file    app_calibration_board_port.c
  * @brief   P2-18(PC6)/P2-16(PC7) 高有效校准继电器 Port（UTF-8）。
  ******************************************************************************
  */

#include "app_calibration_board_port.h"

#include "app_calibration_service.h"
#include "app_calibration_storage_port.h"
#include "main.h"

#include <stddef.h>

#define APP_CALIBRATION_RELAY_MASK  \
    (CAL_RELAY_INPUT_SEL_Pin | CAL_RELAY_ADC_LOOP_Pin)

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

void AppCalibrationBoardPort_Register(void)
{
    AppCalibrationPorts ports = {0};
    ports.relay.is_configured = app_calibration_board_relay_is_configured;
    ports.relay.supports_source = app_calibration_board_relay_supports_source;
    ports.relay.enter_safe_state = app_calibration_board_relay_safe;
    ports.relay.route_source = app_calibration_board_relay_route;

    /* QSPI 初始化失败时仍注册 Port，由 is_ready 保持硬门禁和错误显示。 */
    (void)AppCalibrationStoragePort_Init();
    AppCalibrationStoragePort_Fill(&ports.storage);

    /* ADC、DAC 安全范围尚未确认，保持空 Port 以维持 START 硬门禁。 */
    AppCalibrationService_ConfigurePorts(&ports);
    (void)app_calibration_board_relay_safe(NULL);
}
