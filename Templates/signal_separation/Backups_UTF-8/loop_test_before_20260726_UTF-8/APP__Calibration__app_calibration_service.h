#ifndef APP_CALIBRATION_APP_CALIBRATION_SERVICE_H_
#define APP_CALIBRATION_APP_CALIBRATION_SERVICE_H_

#include "app_calibration_port.h"
#include "calibration_core.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    APP_CALIBRATION_MODE_INTERNAL = 0,
    APP_CALIBRATION_MODE_EXTERNAL
} AppCalibrationMode;

typedef enum
{
    APP_CALIBRATION_SOURCE_DAC1_TO_ADC = 0,
    APP_CALIBRATION_SOURCE_DAC2_TO_ADC,
    APP_CALIBRATION_SOURCE_EXTERNAL_INPUT
} AppCalibrationSource;

typedef enum
{
    APP_CALIBRATION_STATE_IDLE = 0,
    APP_CALIBRATION_STATE_ACQUIRE_EXCLUSIVE,
    APP_CALIBRATION_STATE_WAIT_FOR_IDLE,
    APP_CALIBRATION_STATE_READY,
    APP_CALIBRATION_STATE_PREPARE_SESSION,
    APP_CALIBRATION_STATE_SESSION_ACTIVE,
    APP_CALIBRATION_STATE_REVIEW,
    APP_CALIBRATION_STATE_STOPPING,
    APP_CALIBRATION_STATE_ERROR
} AppCalibrationState;

typedef enum
{
    APP_CALIBRATION_ERROR_NONE = 0,
    APP_CALIBRATION_ERROR_BUSY,
    APP_CALIBRATION_ERROR_RELAY_NOT_CONFIGURED,
    APP_CALIBRATION_ERROR_SOURCE_NOT_SUPPORTED,
    APP_CALIBRATION_ERROR_ADC_NOT_CONFIGURED,
    APP_CALIBRATION_ERROR_DAC_NOT_CONFIGURED,
    APP_CALIBRATION_ERROR_STORAGE_NOT_CONFIGURED,
    APP_CALIBRATION_ERROR_SAFE_STOP_TIMEOUT,
    APP_CALIBRATION_ERROR_PORT_IO,
    APP_CALIBRATION_ERROR_PROFILE_INVALID,
    APP_CALIBRATION_ERROR_NO_ROLLBACK_PROFILE
} AppCalibrationError;

typedef struct
{
    uint32_t item_id;
    uint32_t expected_point_count;
} AppCalibrationRunConfig;

typedef struct
{
    AppCalibrationState state;
    AppCalibrationError error;
    AppCalibrationMode mode;
    AppCalibrationSource source;
    uint8_t progress_percent;
    uint32_t session_sequence;
    uint32_t stored_point_count;
    uint32_t expected_point_count;
    uint32_t active_profile_sequence;
    uint32_t pending_profile_sequence;
    uint32_t previous_profile_sequence;
    bool exclusive;
    bool relay_configured;
    bool source_supported;
    bool adc_ready;
    bool dac_ready;
    bool qspi_ready;
    bool can_start;
    bool session_present;
    bool candidate_present;
    bool pending_present;
    bool runtime_applied;
} AppCalibrationSnapshot;

void AppCalibrationService_ConfigurePorts(const AppCalibrationPorts *ports);
void AppCalibrationService_Init(void);
void AppCalibrationService_Run(void);

bool AppCalibrationService_RequestEnter(AppCalibrationMode mode,
                                        AppCalibrationSource source);
bool AppCalibrationService_SetSelection(AppCalibrationMode mode,
                                        AppCalibrationSource source);
bool AppCalibrationService_RequestStart(
    const AppCalibrationRunConfig *config);
bool AppCalibrationService_SubmitMeasurementPoint(
    const CalibrationMeasurementPoint *point);
bool AppCalibrationService_RequestFinishSession(void);
bool AppCalibrationService_SubmitCandidate(
    const CalibrationProfile *profile);
bool AppCalibrationService_RequestConfirmCandidate(void);
bool AppCalibrationService_RequestCancelCandidate(void);
bool AppCalibrationService_RequestApplyNow(void);
bool AppCalibrationService_RequestUndoApply(void);
bool AppCalibrationService_RequestExit(void);
bool AppCalibrationService_IsExclusive(void);
void AppCalibrationService_GetSnapshot(AppCalibrationSnapshot *snapshot);
bool AppCalibrationService_GetRuntimeProfile(CalibrationProfile *profile);

#ifdef __cplusplus
}
#endif

#endif /* APP_CALIBRATION_APP_CALIBRATION_SERVICE_H_ */
