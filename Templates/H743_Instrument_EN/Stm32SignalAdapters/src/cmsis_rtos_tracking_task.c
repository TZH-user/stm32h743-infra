#include "app_phase_lock_task.h"

#include "FreeRTOS.h"
#include "ad9226.h"
#include "app_dac_output.h"
#include "app_dac_output_config.h"
#include "app_output_task.h"
#include "app_phase_lock.h"
#include "app_phase_lock_config.h"
#include "cmsis_os2.h"
#include "signal_preprocess.h"
#include "signal_separation_dac_port.h"
#include "task.h"

#include <math.h>
#include <string.h>

#define APP_PHASE_LOCK_TASK_EVENT_START  (1UL << 0)
#define APP_PHASE_LOCK_TASK_EVENT_STOP   (1UL << 1)
#define APP_PHASE_LOCK_TASK_EVENT_MASK   \
    (APP_PHASE_LOCK_TASK_EVENT_START | APP_PHASE_LOCK_TASK_EVENT_STOP \
     | APP_DAC_OUTPUT_EVENT_MASK)
#define APP_PHASE_LOCK_HALF_PI_F         1.57079632679489661923f
#define APP_PHASE_LOCK_DEG_TO_RAD_F       0.01745329251994329577f

static osThreadId_t s_phase_lock_task_id;
static SignalVoteResult s_pending_result;
static bool s_pending_result_valid;
static bool s_last_published_lock;

static void app_phase_lock_task_build_dac_references(
    const AppPhaseLockReference
        lock_references[SIGNAL_JOINT_COMPONENT_COUNT],
    AppDacOutputPhaseReference
        dac_references[APP_DAC_OUTPUT_CHANNEL_COUNT])
{
    static const float calibration_rad[APP_DAC_OUTPUT_CHANNEL_COUNT] = {
        APP_PHASE_LOCK_DAC1_CALIBRATION_RAD,
        APP_PHASE_LOCK_DAC2_CALIBRATION_RAD,
    };

    uint8_t harmonic_ratio = g_app_dac_output_status.harmonic_ratio;
    if (g_app_dac_output_status.phase_control_active
        && (harmonic_ratio >= 2U))
    {
        float ratio = (float)harmonic_ratio;
        float fundamental_hz = 0.5f * (
            lock_references[0].frequency_hz
            + (lock_references[1].frequency_hz / ratio));
        float maximum_fundamental_hz =
            (float)APP_DAC_OUTPUT_MAX_FREQUENCY_HZ / ratio;
        if (fundamental_hz < (float)APP_DAC_OUTPUT_MIN_FREQUENCY_HZ)
        {
            fundamental_hz = (float)APP_DAC_OUTPUT_MIN_FREQUENCY_HZ;
        }
        else if (fundamental_hz > maximum_fundamental_hz)
        {
            fundamental_hz = maximum_fundamental_hz;
        }

        float phase_a_rad =
            lock_references[0].cosine_phase_rad
            + APP_PHASE_LOCK_HALF_PI_F;
        float target_phase_rad =
            (float)AppDacOutput_GetPhaseDifference()
            * APP_PHASE_LOCK_DEG_TO_RAD_F;

        dac_references[0].frequency_hz = fundamental_hz;
        dac_references[0].wave_phase_rad =
            phase_a_rad + calibration_rad[0];
        dac_references[0].epoch_sys_timer =
            lock_references[0].epoch_sys_timer;

        dac_references[1].frequency_hz =
            fundamental_hz * ratio;
        dac_references[1].wave_phase_rad =
            (ratio * phase_a_rad)
            + target_phase_rad + calibration_rad[1];
        dac_references[1].epoch_sys_timer =
            lock_references[0].epoch_sys_timer;
        return;
    }

    for (uint32_t channel = 0U;
         channel < APP_DAC_OUTPUT_CHANNEL_COUNT;
         ++channel)
    {
        dac_references[channel].frequency_hz =
            lock_references[channel].frequency_hz;
        dac_references[channel].wave_phase_rad =
            lock_references[channel].cosine_phase_rad
            + APP_PHASE_LOCK_HALF_PI_F + calibration_rad[channel];
        dac_references[channel].epoch_sys_timer =
            lock_references[channel].epoch_sys_timer;
    }
}

static uint32_t app_phase_lock_task_ms_to_ticks(uint32_t milliseconds)
{
    uint32_t tick_frequency_hz = osKernelGetTickFreq();
    return (uint32_t)(((uint64_t)milliseconds * tick_frequency_hz
                       + 999ULL) / 1000ULL);
}

static uint32_t app_phase_lock_task_sample0_timer(
    const AD9226_Frame *frame,
    uint32_t sample_rate_hz)
{
    uint32_t sys_timer_hz = osKernelGetSysTimerFreq();
    uint64_t frame_ticks = ((uint64_t)(frame->sample_count - 1U)
                            * sys_timer_hz
                            + (sample_rate_hz / 2U))
        / sample_rate_hz;
    return frame->completion_sys_timer - (uint32_t)frame_ticks;
}

/*@brief:处理一帧已知双分量模型并把相位参考提交给DAC。 */
static void app_phase_lock_task_process_frame(const AD9226_Frame *frame,
                                              uint32_t sample_rate_hz)
{
    static SignalPreprocessInfo preprocess_info;
    static SignalJointComponent seeds[SIGNAL_JOINT_COMPONENT_COUNT];
    static SignalJointResult joint_result;
    static AppPhaseLockReference
        lock_references[SIGNAL_JOINT_COMPONENT_COUNT];
    AppDacOutputPhaseReference
        dac_references[APP_DAC_OUTPUT_CHANNEL_COUNT] = {0};

    uint32_t processing_start = osKernelGetSysTimerCount();
    memset(&preprocess_info, 0, sizeof(preprocess_info));
    memset(&joint_result, 0, sizeof(joint_result));
    if (!SignalPreprocess_Run(frame->samples,
                             frame->sample_count,
                             g_signal_preprocess_windowed,
                             g_signal_preprocess_unwindowed,
                             &preprocess_info))
    {
        AppPhaseLock_RecordInvalid(APP_PHASE_LOCK_ERROR_PREPROCESS);
        return;
    }

    AppPhaseLock_GetSeeds(seeds);
    if (!SignalJoint_TrackKnown(g_signal_preprocess_unwindowed,
                                frame->sample_count,
                                sample_rate_hz,
                                seeds,
                                &joint_result))
    {
        AppPhaseLock_RecordInvalid(APP_PHASE_LOCK_ERROR_MODEL);
        return;
    }

    uint32_t sample0_timer = app_phase_lock_task_sample0_timer(
        frame, sample_rate_hz);
    if (!AppPhaseLock_Update(&joint_result,
                             sample0_timer,
                             lock_references))
    {
        return;
    }

    app_phase_lock_task_build_dac_references(lock_references,
                                             dac_references);
    if (!AppDacOutput_UpdatePhaseReferences(dac_references))
    {
        AppPhaseLock_RecordInvalid(APP_PHASE_LOCK_ERROR_DAC_UPDATE);
    }

    uint32_t sys_timer_hz = osKernelGetSysTimerFreq();
    uint32_t processing_us = (sys_timer_hz == 0U)
        ? 0U
        : (uint32_t)(((uint64_t)(osKernelGetSysTimerCount()
                                - processing_start) * 1000000ULL)
                     / sys_timer_hz);
    g_app_phase_lock_status.processing_time_last_us = processing_us;
    if (processing_us > g_app_phase_lock_status.processing_time_max_us)
    {
        g_app_phase_lock_status.processing_time_max_us = processing_us;
    }

    bool all_locked = g_app_phase_lock_status.all_locked;
    if ((g_app_phase_lock_status.captured_frames == 1U)
        || (all_locked != s_last_published_lock)
        || ((g_app_phase_lock_status.captured_frames % 25U) == 0U))
    {
        (void)AppOutput_PublishPhaseLockStatus(
            (const AppPhaseLockStatus *)&g_app_phase_lock_status);
        s_last_published_lock = all_locked;
    }
}

static bool app_phase_lock_task_take_pending(SignalVoteResult *result)
{
    bool valid;
    taskENTER_CRITICAL();
    valid = s_pending_result_valid;
    if (valid)
    {
        *result = s_pending_result;
        s_pending_result_valid = false;
    }
    taskEXIT_CRITICAL();
    return valid;
}

static void app_phase_lock_task_stop_session(bool *capture_running)
{
    bool was_running = g_app_phase_lock_status.running;
    if (*capture_running || AD9226_IsRunning())
    {
        (void)AD9226_Stop();
    }
    *capture_running = false;
    AppDacOutput_Stop();
    AppPhaseLock_End();
    if (was_running)
    {
        (void)AppOutput_PublishPhaseLockStatus(
            (const AppPhaseLockStatus *)&g_app_phase_lock_status);
    }
}

bool AppPhaseLockTask_RequestStart(const SignalVoteResult *result)
{
    if ((result == NULL) || !result->valid
        || (s_phase_lock_task_id == NULL))
    {
        return false;
    }

    taskENTER_CRITICAL();
    s_pending_result = *result;
    s_pending_result_valid = true;
    taskEXIT_CRITICAL();
    return (osThreadFlagsSet(s_phase_lock_task_id,
                             APP_PHASE_LOCK_TASK_EVENT_START)
            & osFlagsError) == 0U;
}

bool AppPhaseLockTask_Stop(uint32_t timeout_ticks)
{
    if (s_phase_lock_task_id == NULL)
    {
        return false;
    }
    if (!g_app_phase_lock_status.running)
    {
        return true;
    }

    if ((osThreadFlagsSet(s_phase_lock_task_id,
                          APP_PHASE_LOCK_TASK_EVENT_STOP)
         & osFlagsError) != 0U)
    {
        return false;
    }

    uint32_t start_tick = osKernelGetTickCount();
    while (g_app_phase_lock_status.running
           && ((osKernelGetTickCount() - start_tick) < timeout_ticks))
    {
        osDelay(1U);
    }
    if (g_app_phase_lock_status.running)
    {
        g_app_phase_lock_status.last_error =
            APP_PHASE_LOCK_ERROR_STOP_TIMEOUT;
        return false;
    }
    return true;
}

bool AppPhaseLockTask_IsRunning(void)
{
    return g_app_phase_lock_status.running;
}

bool AppPhaseLockTask_SetOutputPhaseDifference(uint16_t phase_deg)
{
    return AppDacOutput_SetPhaseDifference(phase_deg);
}

uint16_t AppPhaseLockTask_GetOutputPhaseDifference(void)
{
    return AppDacOutput_GetPhaseDifference();
}

void StartPhaseLockTask(void *argument)
{
    (void)argument;

    s_phase_lock_task_id = osThreadGetId();
    g_app_phase_lock_status.ready = SignalSeparationDacPort_Init()
        && AppDacOutput_BindCurrentTask()
        && AppDacOutput_Init();
    if (!g_app_phase_lock_status.ready)
    {
        g_app_phase_lock_status.last_error =
            APP_PHASE_LOCK_ERROR_NOT_READY;
    }

    bool capture_running = false;
    uint32_t capture_start_tick = 0U;
    uint32_t next_capture_tick = 0U;
    uint32_t capture_interval_ticks = app_phase_lock_task_ms_to_ticks(
        APP_PHASE_LOCK_CAPTURE_INTERVAL_MS);
    uint32_t capture_timeout_ticks = app_phase_lock_task_ms_to_ticks(
        APP_PHASE_LOCK_CAPTURE_TIMEOUT_MS);
    uint32_t actual_sample_rate_hz = APP_PHASE_LOCK_SAMPLE_RATE_HZ;
    static AD9226_Frame frame;
    static SignalVoteResult start_result;

    for (;;)
    {
        uint32_t events = osThreadFlagsWait(
            APP_PHASE_LOCK_TASK_EVENT_MASK,
            osFlagsWaitAny,
            1U);
        if ((events & osFlagsError) != 0U)
        {
            events = 0U;
        }

        if ((events & APP_PHASE_LOCK_TASK_EVENT_STOP) != 0U)
        {
            app_phase_lock_task_stop_session(&capture_running);
        }

        if ((events & APP_PHASE_LOCK_TASK_EVENT_START) != 0U)
        {
            app_phase_lock_task_stop_session(&capture_running);
            s_last_published_lock = false;
            if (g_app_phase_lock_status.ready
                && app_phase_lock_task_take_pending(&start_result)
                && (AD9226_SetSampleRate(APP_PHASE_LOCK_SAMPLE_RATE_HZ,
                                         &actual_sample_rate_hz)
                    == AD9226_STATUS_OK)
                && AppPhaseLock_Begin(&start_result,
                                      osKernelGetSysTimerFreq())
                && AppDacOutput_StartFromVote(&start_result))
            {
                next_capture_tick = osKernelGetTickCount();
            }
            else
            {
                AppDacOutput_Stop();
                AppPhaseLock_End();
                g_app_phase_lock_status.last_error =
                    APP_PHASE_LOCK_ERROR_DAC_START;
            }
        }

        (void)AppDacOutput_ServiceEvents(events);
        if (g_app_phase_lock_status.running
            && !AppDacOutput_IsRunning())
        {
            AppPhaseLock_RecordInvalid(APP_PHASE_LOCK_ERROR_DAC_UPDATE);
            app_phase_lock_task_stop_session(&capture_running);
            continue;
        }
        if (!g_app_phase_lock_status.running)
        {
            continue;
        }

        uint32_t now = osKernelGetTickCount();
        if (!capture_running
            && ((int32_t)(now - next_capture_tick) >= 0))
        {
            if (AD9226_Start() == AD9226_STATUS_OK)
            {
                capture_running = true;
                capture_start_tick = now;
            }
            else
            {
                AppPhaseLock_RecordInvalid(APP_PHASE_LOCK_ERROR_ADC_START);
                next_capture_tick = now
                    + capture_interval_ticks;
            }
        }

        if (capture_running && AD9226_AcquireLatestFrame(&frame))
        {
            (void)AD9226_Stop();
            capture_running = false;
            ++g_app_phase_lock_status.captured_frames;
            app_phase_lock_task_process_frame(&frame,
                                              actual_sample_rate_hz);
            next_capture_tick = osKernelGetTickCount()
                + capture_interval_ticks;
        }
        else if (capture_running
                 && ((now - capture_start_tick)
                     >= capture_timeout_ticks))
        {
            (void)AD9226_Stop();
            capture_running = false;
            AppPhaseLock_RecordInvalid(
                APP_PHASE_LOCK_ERROR_CAPTURE_TIMEOUT);
            next_capture_tick = now + capture_interval_ticks;
        }
    }
}
