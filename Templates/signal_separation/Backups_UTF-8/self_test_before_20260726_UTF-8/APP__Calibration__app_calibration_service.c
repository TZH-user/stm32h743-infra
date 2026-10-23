#include "app_calibration_service.h"

#include "ad9226.h"
#include "app_dac_output.h"
#include "app_main_task.h"
#include "app_phase_lock_task.h"
#include "stm32h7xx.h"

#include <stddef.h>
#include <string.h>

#define APP_CALIBRATION_PHASE_STOP_POLL_TICKS  1U

static volatile AppCalibrationSnapshot s_snapshot;
static AppCalibrationPorts s_ports;
static CalibrationSessionInfo s_session;
static CalibrationProfile s_candidate_profile;
static CalibrationProfile s_pending_profile;
static CalibrationProfile s_runtime_profile;
static CalibrationProfile s_previous_profile;
static bool s_runtime_profile_valid = false;
static bool s_previous_profile_valid = false;
static uint32_t s_next_session_sequence = 1U;

static void app_calibration_publish_state(AppCalibrationState state,
                                          AppCalibrationError error)
{
    s_snapshot.state = state;
    s_snapshot.error = error;
    __DMB();
}

static void app_calibration_force_safe_outputs(void)
{
    (void)AD9226_Stop();
    AppDacOutput_Stop();

    if (s_ports.adc.stop != NULL)
    {
        (void)s_ports.adc.stop(s_ports.adc.context);
    }
    if (s_ports.dac.stop_all != NULL)
    {
        (void)s_ports.dac.stop_all(s_ports.dac.context);
    }
    if (s_ports.relay.enter_safe_state != NULL)
    {
        (void)s_ports.relay.enter_safe_state(s_ports.relay.context);
    }
}

static bool app_calibration_port_ready(bool (*ready)(void *), void *context)
{
    return (ready != NULL) && ready(context);
}

static void app_calibration_refresh_port_state(void)
{
    s_snapshot.relay_configured = app_calibration_port_ready(
        s_ports.relay.is_configured, s_ports.relay.context);
    s_snapshot.source_supported = s_snapshot.relay_configured
        && (s_ports.relay.supports_source != NULL)
        && s_ports.relay.supports_source(s_ports.relay.context,
                                         (uint32_t)s_snapshot.source);
    s_snapshot.adc_ready = app_calibration_port_ready(
        s_ports.adc.is_configured, s_ports.adc.context);
    s_snapshot.dac_ready = app_calibration_port_ready(
        s_ports.dac.is_configured, s_ports.dac.context);
    s_snapshot.qspi_ready = app_calibration_port_ready(
        s_ports.storage.is_ready, s_ports.storage.context);

    bool source_needs_dac =
        (s_snapshot.source != APP_CALIBRATION_SOURCE_EXTERNAL_INPUT);
    s_snapshot.can_start = s_snapshot.relay_configured
        && s_snapshot.source_supported
        && s_snapshot.adc_ready
        && s_snapshot.qspi_ready
        && (!source_needs_dac || s_snapshot.dac_ready);
    __DMB();
}

static AppCalibrationError app_calibration_missing_port_error(void)
{
    if (!s_snapshot.relay_configured)
    {
        return APP_CALIBRATION_ERROR_RELAY_NOT_CONFIGURED;
    }
    if (!s_snapshot.source_supported)
    {
        return APP_CALIBRATION_ERROR_SOURCE_NOT_SUPPORTED;
    }
    if (!s_snapshot.adc_ready)
    {
        return APP_CALIBRATION_ERROR_ADC_NOT_CONFIGURED;
    }
    if ((s_snapshot.source != APP_CALIBRATION_SOURCE_EXTERNAL_INPUT)
        && !s_snapshot.dac_ready)
    {
        return APP_CALIBRATION_ERROR_DAC_NOT_CONFIGURED;
    }
    if (!s_snapshot.qspi_ready)
    {
        return APP_CALIBRATION_ERROR_STORAGE_NOT_CONFIGURED;
    }
    return APP_CALIBRATION_ERROR_NONE;
}

void AppCalibrationService_ConfigurePorts(const AppCalibrationPorts *ports)
{
    if (ports == NULL)
    {
        memset(&s_ports, 0, sizeof(s_ports));
    }
    else
    {
        s_ports = *ports;
    }
    app_calibration_refresh_port_state();
}

void AppCalibrationService_Init(void)
{
    memset((void *)&s_snapshot, 0, sizeof(s_snapshot));
    memset(&s_session, 0, sizeof(s_session));
    memset(&s_candidate_profile, 0, sizeof(s_candidate_profile));
    memset(&s_pending_profile, 0, sizeof(s_pending_profile));
    memset(&s_runtime_profile, 0, sizeof(s_runtime_profile));
    memset(&s_previous_profile, 0, sizeof(s_previous_profile));
    s_runtime_profile_valid = false;
    s_previous_profile_valid = false;
    s_next_session_sequence = 1U;

    s_snapshot.state = APP_CALIBRATION_STATE_IDLE;
    s_snapshot.error = APP_CALIBRATION_ERROR_NONE;
    s_snapshot.mode = APP_CALIBRATION_MODE_INTERNAL;
    s_snapshot.source = APP_CALIBRATION_SOURCE_DAC1_TO_ADC;
    s_snapshot.progress_percent = 0U;
    s_snapshot.exclusive = false;
    app_calibration_refresh_port_state();

    if (s_snapshot.qspi_ready
        && (s_ports.storage.get_next_sequence != NULL))
    {
        uint32_t sequence = 1U;
        if (s_ports.storage.get_next_sequence(s_ports.storage.context,
                                              &sequence)
            == APP_CALIBRATION_PORT_OK)
        {
            s_next_session_sequence = (sequence == 0U) ? 1U : sequence;
        }
    }

    if (s_snapshot.qspi_ready
        && (s_ports.storage.load_boot_profile != NULL))
    {
        CalibrationProfile boot_profile;
        if ((s_ports.storage.load_boot_profile(s_ports.storage.context,
                                               &boot_profile)
             == APP_CALIBRATION_PORT_OK)
            && (CalibrationCore_ValidateProfile(&boot_profile)
                == CALIBRATION_CORE_OK))
        {
            s_runtime_profile = boot_profile;
            s_runtime_profile_valid = true;
            s_snapshot.active_profile_sequence = boot_profile.sequence;
        }
    }
    __DMB();
}

bool AppCalibrationService_RequestEnter(AppCalibrationMode mode,
                                        AppCalibrationSource source)
{
    if (s_snapshot.state != APP_CALIBRATION_STATE_IDLE)
    {
        return false;
    }

    s_snapshot.mode = mode;
    s_snapshot.source = source;
    s_snapshot.progress_percent = 0U;
    s_snapshot.exclusive = true;
    app_calibration_refresh_port_state();
    app_calibration_publish_state(APP_CALIBRATION_STATE_ACQUIRE_EXCLUSIVE,
                                  APP_CALIBRATION_ERROR_NONE);
    return true;
}

bool AppCalibrationService_SetSelection(AppCalibrationMode mode,
                                        AppCalibrationSource source)
{
    if ((s_snapshot.state != APP_CALIBRATION_STATE_IDLE)
        && (s_snapshot.state != APP_CALIBRATION_STATE_READY))
    {
        return false;
    }

    s_snapshot.mode = mode;
    s_snapshot.source = source;
    app_calibration_refresh_port_state();
    s_snapshot.error = app_calibration_missing_port_error();
    __DMB();
    return true;
}

bool AppCalibrationService_RequestStart(
    const AppCalibrationRunConfig *config)
{
    if ((config == NULL) || (s_snapshot.state != APP_CALIBRATION_STATE_READY))
    {
        return false;
    }

    app_calibration_refresh_port_state();
    AppCalibrationError error = app_calibration_missing_port_error();
    if (error != APP_CALIBRATION_ERROR_NONE)
    {
        app_calibration_publish_state(APP_CALIBRATION_STATE_READY, error);
        return false;
    }

    CalibrationCore_SessionInit(&s_session,
                                s_next_session_sequence++,
                                (uint32_t)s_snapshot.mode,
                                (uint32_t)s_snapshot.source,
                                config->item_id,
                                config->expected_point_count);
    s_snapshot.session_sequence = s_session.sequence;
    s_snapshot.stored_point_count = 0U;
    s_snapshot.expected_point_count = config->expected_point_count;
    s_snapshot.progress_percent = 0U;
    s_snapshot.session_present = false;
    app_calibration_publish_state(APP_CALIBRATION_STATE_PREPARE_SESSION,
                                  APP_CALIBRATION_ERROR_NONE);
    return true;
}

bool AppCalibrationService_SubmitMeasurementPoint(
    const CalibrationMeasurementPoint *point)
{
    if ((point == NULL)
        || (s_snapshot.state != APP_CALIBRATION_STATE_SESSION_ACTIVE)
        || (s_ports.storage.append_point == NULL))
    {
        return false;
    }

    if (s_ports.storage.append_point(s_ports.storage.context,
                                     s_session.sequence,
                                     point) != APP_CALIBRATION_PORT_OK)
    {
        app_calibration_publish_state(APP_CALIBRATION_STATE_ERROR,
                                      APP_CALIBRATION_ERROR_PORT_IO);
        return false;
    }

    ++s_session.stored_point_count;
    s_snapshot.stored_point_count = s_session.stored_point_count;
    if (s_session.expected_point_count != 0U)
    {
        uint32_t progress = (s_session.stored_point_count * 100U)
            / s_session.expected_point_count;
        s_snapshot.progress_percent = (uint8_t)(
            (progress > 100U) ? 100U : progress);
    }
    __DMB();
    return true;
}

bool AppCalibrationService_RequestFinishSession(void)
{
    if ((s_snapshot.state != APP_CALIBRATION_STATE_SESSION_ACTIVE)
        || (s_ports.storage.finish_session == NULL))
    {
        return false;
    }
    /* REVIEW 阶段不允许继电器或 DAC 保持工作，先执行统一安全收尾。 */
    app_calibration_force_safe_outputs();
    if (s_ports.storage.finish_session(s_ports.storage.context,
                                       s_session.sequence)
        != APP_CALIBRATION_PORT_OK)
    {
        app_calibration_publish_state(APP_CALIBRATION_STATE_ERROR,
                                      APP_CALIBRATION_ERROR_PORT_IO);
        return false;
    }

    s_snapshot.session_present = true;
    s_snapshot.progress_percent = 100U;
    app_calibration_publish_state(APP_CALIBRATION_STATE_REVIEW,
                                  APP_CALIBRATION_ERROR_NONE);
    return true;
}

bool AppCalibrationService_SubmitCandidate(
    const CalibrationProfile *profile)
{
    if ((profile == NULL)
        || ((s_snapshot.state != APP_CALIBRATION_STATE_READY)
            && (s_snapshot.state != APP_CALIBRATION_STATE_REVIEW)))
    {
        return false;
    }
    if (CalibrationCore_ValidateProfile(profile) != CALIBRATION_CORE_OK)
    {
        app_calibration_publish_state(s_snapshot.state,
                                      APP_CALIBRATION_ERROR_PROFILE_INVALID);
        return false;
    }

    s_candidate_profile = *profile;
    s_snapshot.candidate_present = true;
    app_calibration_publish_state(APP_CALIBRATION_STATE_REVIEW,
                                  APP_CALIBRATION_ERROR_NONE);
    return true;
}

bool AppCalibrationService_RequestConfirmCandidate(void)
{
    if (!s_snapshot.candidate_present || !s_snapshot.qspi_ready
        || (s_ports.storage.store_pending_profile == NULL))
    {
        return false;
    }
    if (s_ports.storage.store_pending_profile(
            s_ports.storage.context, &s_candidate_profile)
        != APP_CALIBRATION_PORT_OK)
    {
        app_calibration_publish_state(APP_CALIBRATION_STATE_REVIEW,
                                      APP_CALIBRATION_ERROR_PORT_IO);
        return false;
    }

    s_pending_profile = s_candidate_profile;
    memset(&s_candidate_profile, 0, sizeof(s_candidate_profile));
    s_snapshot.candidate_present = false;
    s_snapshot.pending_present = true;
    s_snapshot.pending_profile_sequence = s_pending_profile.sequence;
    app_calibration_publish_state(APP_CALIBRATION_STATE_READY,
                                  APP_CALIBRATION_ERROR_NONE);
    return true;
}

bool AppCalibrationService_RequestCancelCandidate(void)
{
    if (!s_snapshot.candidate_present)
    {
        return false;
    }
    memset(&s_candidate_profile, 0, sizeof(s_candidate_profile));
    s_snapshot.candidate_present = false;
    app_calibration_publish_state(APP_CALIBRATION_STATE_READY,
                                  APP_CALIBRATION_ERROR_NONE);
    return true;
}

bool AppCalibrationService_RequestApplyNow(void)
{
    if (!s_snapshot.pending_present || !s_runtime_profile_valid)
    {
        if (!s_runtime_profile_valid)
        {
            app_calibration_publish_state(s_snapshot.state,
                APP_CALIBRATION_ERROR_NO_ROLLBACK_PROFILE);
        }
        return false;
    }

    s_previous_profile = s_runtime_profile;
    s_previous_profile_valid = true;
    s_runtime_profile = s_pending_profile;
    s_runtime_profile_valid = true;
    s_snapshot.runtime_applied = true;
    s_snapshot.previous_profile_sequence = s_previous_profile.sequence;
    s_snapshot.active_profile_sequence = s_runtime_profile.sequence;
    __DMB();
    return true;
}

bool AppCalibrationService_RequestUndoApply(void)
{
    if (!s_snapshot.runtime_applied || !s_previous_profile_valid)
    {
        return false;
    }

    s_runtime_profile = s_previous_profile;
    s_runtime_profile_valid = true;
    s_snapshot.active_profile_sequence = s_runtime_profile.sequence;
    s_snapshot.runtime_applied = false;
    __DMB();
    return true;
}

bool AppCalibrationService_RequestExit(void)
{
    if (s_snapshot.state == APP_CALIBRATION_STATE_IDLE)
    {
        return false;
    }

    app_calibration_publish_state(APP_CALIBRATION_STATE_STOPPING,
                                  APP_CALIBRATION_ERROR_NONE);
    return true;
}

bool AppCalibrationService_IsExclusive(void)
{
    return s_snapshot.exclusive;
}

void AppCalibrationService_GetSnapshot(AppCalibrationSnapshot *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }

    __DMB();
    *snapshot = s_snapshot;
    __DMB();
}

bool AppCalibrationService_GetRuntimeProfile(CalibrationProfile *profile)
{
    if ((profile == NULL) || !s_runtime_profile_valid)
    {
        return false;
    }
    *profile = s_runtime_profile;
    return true;
}

void AppCalibrationService_Run(void)
{
    switch (s_snapshot.state)
    {
        case APP_CALIBRATION_STATE_IDLE:
            break;

        case APP_CALIBRATION_STATE_ACQUIRE_EXCLUSIVE:
            (void)AppMain_RequestStop();
            if (AppPhaseLockTask_IsRunning())
            {
                (void)AppPhaseLockTask_Stop(
                    APP_CALIBRATION_PHASE_STOP_POLL_TICKS);
            }
            app_calibration_publish_state(
                APP_CALIBRATION_STATE_WAIT_FOR_IDLE,
                APP_CALIBRATION_ERROR_NONE);
            break;

        case APP_CALIBRATION_STATE_WAIT_FOR_IDLE:
            if (AppPhaseLockTask_IsRunning())
            {
                (void)AppPhaseLockTask_Stop(
                    APP_CALIBRATION_PHASE_STOP_POLL_TICKS);
            }
            if (AppMain_IsRunning() || AppPhaseLockTask_IsRunning())
            {
                break;
            }

            app_calibration_force_safe_outputs();
            app_calibration_refresh_port_state();
            app_calibration_publish_state(
                APP_CALIBRATION_STATE_READY,
                app_calibration_missing_port_error());
            break;

        case APP_CALIBRATION_STATE_READY:
        case APP_CALIBRATION_STATE_REVIEW:
            break;

        case APP_CALIBRATION_STATE_PREPARE_SESSION:
            app_calibration_force_safe_outputs();
            if ((s_ports.relay.route_source == NULL)
                || (s_ports.storage.begin_session == NULL)
                || (s_ports.relay.route_source(
                        s_ports.relay.context,
                        (uint32_t)s_snapshot.source)
                    != APP_CALIBRATION_PORT_OK)
                || (s_ports.storage.begin_session(
                        s_ports.storage.context, &s_session)
                    != APP_CALIBRATION_PORT_OK))
            {
                app_calibration_publish_state(APP_CALIBRATION_STATE_ERROR,
                                              APP_CALIBRATION_ERROR_PORT_IO);
                break;
            }
            app_calibration_publish_state(
                APP_CALIBRATION_STATE_SESSION_ACTIVE,
                APP_CALIBRATION_ERROR_NONE);
            break;

        case APP_CALIBRATION_STATE_SESSION_ACTIVE:
            /* 数据来源通过 SubmitMeasurementPoint 注入，服务不关心采集者。 */
            break;

        case APP_CALIBRATION_STATE_STOPPING:
            app_calibration_force_safe_outputs();
            s_snapshot.progress_percent = 0U;
            s_snapshot.exclusive = false;
            app_calibration_publish_state(APP_CALIBRATION_STATE_IDLE,
                                          APP_CALIBRATION_ERROR_NONE);
            break;

        case APP_CALIBRATION_STATE_ERROR:
            app_calibration_force_safe_outputs();
            break;

        default:
            app_calibration_force_safe_outputs();
            app_calibration_publish_state(APP_CALIBRATION_STATE_ERROR,
                                          APP_CALIBRATION_ERROR_PORT_IO);
            break;
    }
}
