/**
  ******************************************************************************
  * @file    app_calibration_port.h
  * @brief   校准服务的 ADC/DAC/继电器/QSPI Port 契约（UTF-8）。
  ******************************************************************************
  */

#ifndef APP_CALIBRATION_APP_CALIBRATION_PORT_H_
#define APP_CALIBRATION_APP_CALIBRATION_PORT_H_

#include "calibration_core.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    APP_CALIBRATION_PORT_OK = 0,
    APP_CALIBRATION_PORT_BUSY,
    APP_CALIBRATION_PORT_NOT_CONFIGURED,
    APP_CALIBRATION_PORT_IO_ERROR,
    APP_CALIBRATION_PORT_OUT_OF_RANGE
} AppCalibrationPortStatus;

typedef struct
{
    void *context;
    bool (*is_configured)(void *context);
    bool (*supports_source)(void *context, uint32_t source_id);
    AppCalibrationPortStatus (*enter_safe_state)(void *context);
    AppCalibrationPortStatus (*route_source)(void *context,
                                             uint32_t source_id);
} AppCalibrationRelayPort;

typedef struct
{
    void *context;
    bool (*is_configured)(void *context);
    AppCalibrationPortStatus (*stop_all)(void *context);
    AppCalibrationPortStatus (*get_limits)(
        void *context,
        uint32_t source_id,
        CalibrationRampLimits *limits);
    AppCalibrationPortStatus (*set_output)(void *context,
                                           uint32_t source_id,
                                           float target_value);
} AppCalibrationDacPort;

typedef struct
{
    void *context;
    bool (*is_configured)(void *context);
    AppCalibrationPortStatus (*stop)(void *context);
    AppCalibrationPortStatus (*capture_point)(
        void *context,
        uint32_t source_id,
        CalibrationMeasurementPoint *point);
} AppCalibrationAdcPort;

typedef struct
{
    void *context;
    bool (*is_ready)(void *context);
    AppCalibrationPortStatus (*get_next_sequence)(void *context,
                                                  uint32_t *sequence);
    AppCalibrationPortStatus (*begin_session)(
        void *context,
        const CalibrationSessionInfo *session);
    AppCalibrationPortStatus (*append_point)(
        void *context,
        uint32_t session_sequence,
        const CalibrationMeasurementPoint *point);
    AppCalibrationPortStatus (*finish_session)(void *context,
                                                uint32_t session_sequence);
    AppCalibrationPortStatus (*store_pending_profile)(
        void *context,
        const CalibrationProfile *profile);
    AppCalibrationPortStatus (*load_boot_profile)(
        void *context,
        CalibrationProfile *profile);
} AppCalibrationStoragePort;

typedef struct
{
    AppCalibrationRelayPort relay;
    AppCalibrationDacPort dac;
    AppCalibrationAdcPort adc;
    AppCalibrationStoragePort storage;
} AppCalibrationPorts;

#ifdef __cplusplus
}
#endif

#endif /* APP_CALIBRATION_APP_CALIBRATION_PORT_H_ */
