#include "app_phase_lock.h"

#include "app_phase_lock_config.h"

#include <math.h>
#include <string.h>

#define APP_PHASE_LOCK_TWO_PI_F  6.28318530717958647692f

typedef struct
{
    float nominal_frequency_hz;
    float tracked_frequency_hz;
    float cosine_phase_rad;
    uint32_t epoch_sys_timer;
    SignalWaveType wave_type;
    bool reference_valid;
} AppPhaseLockChannel;

static AppPhaseLockChannel s_channels[SIGNAL_JOINT_COMPONENT_COUNT];
static uint32_t s_sys_timer_hz;

volatile AppPhaseLockStatus g_app_phase_lock_status;

/*@brief:把相位约束到[-pi, pi)。 */
static float app_phase_lock_wrap_pi(float phase_rad)
{
    while (phase_rad >= 3.14159265358979323846f)
    {
        phase_rad -= APP_PHASE_LOCK_TWO_PI_F;
    }
    while (phase_rad < -3.14159265358979323846f)
    {
        phase_rad += APP_PHASE_LOCK_TWO_PI_F;
    }
    return phase_rad;
}

static float app_phase_lock_clamp(float value,
                                  float minimum,
                                  float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

bool AppPhaseLock_Begin(const SignalVoteResult *result,
                        uint32_t sys_timer_hz)
{
    if ((result == NULL) || !result->valid || (sys_timer_hz == 0U))
    {
        g_app_phase_lock_status.last_error =
            APP_PHASE_LOCK_ERROR_INVALID_ARGUMENT;
        return false;
    }

    memset(s_channels, 0, sizeof(s_channels));
    AppPhaseLockStatus status = {0};
    for (uint32_t channel = 0U;
         channel < SIGNAL_JOINT_COMPONENT_COUNT;
         ++channel)
    {
        const SignalVoteComponent *source = &result->components[channel];
        if (!isfinite(source->frequency_hz)
            || (source->frequency_hz <= 0.0f)
            || ((source->type != SIGNAL_WAVE_SINE)
                && (source->type != SIGNAL_WAVE_TRIANGLE)))
        {
            g_app_phase_lock_status.last_error =
                APP_PHASE_LOCK_ERROR_INVALID_ARGUMENT;
            return false;
        }

        s_channels[channel].nominal_frequency_hz = source->frequency_hz;
        s_channels[channel].tracked_frequency_hz = source->frequency_hz;
        s_channels[channel].wave_type = source->type;
        status.channels[channel].nominal_frequency_hz = source->frequency_hz;
        status.channels[channel].tracked_frequency_hz = source->frequency_hz;
        status.channels[channel].wave_type = source->type;
    }

    s_sys_timer_hz = sys_timer_hz;
    status.start_count = g_app_phase_lock_status.start_count + 1U;
    status.stop_count = g_app_phase_lock_status.stop_count;
    status.ready = true;
    status.running = true;
    status.last_error = APP_PHASE_LOCK_ERROR_NONE;
    g_app_phase_lock_status = status;
    return true;
}

void AppPhaseLock_GetSeeds(
    SignalJointComponent seeds[SIGNAL_JOINT_COMPONENT_COUNT])
{
    if (seeds == NULL)
    {
        return;
    }

    memset(seeds, 0,
           sizeof(SignalJointComponent) * SIGNAL_JOINT_COMPONENT_COUNT);
    for (uint32_t channel = 0U;
         channel < SIGNAL_JOINT_COMPONENT_COUNT;
         ++channel)
    {
        seeds[channel].frequency_hz =
            s_channels[channel].tracked_frequency_hz;
        seeds[channel].type = s_channels[channel].wave_type;
        seeds[channel].source_candidate_rank = channel + 1U;
    }
}

bool AppPhaseLock_Update(
    const SignalJointResult *result,
    uint32_t sample0_sys_timer,
    AppPhaseLockReference references[SIGNAL_JOINT_COMPONENT_COUNT])
{
    if ((result == NULL) || !result->valid || (references == NULL)
        || !g_app_phase_lock_status.running || (s_sys_timer_hz == 0U))
    {
        AppPhaseLock_RecordInvalid(APP_PHASE_LOCK_ERROR_MODEL);
        return false;
    }

    bool all_locked = true;
    for (uint32_t channel = 0U;
         channel < SIGNAL_JOINT_COMPONENT_COUNT;
         ++channel)
    {
        const SignalJointComponent *measurement =
            &result->components[channel];
        AppPhaseLockChannel *tracker = &s_channels[channel];
        volatile AppPhaseLockChannelStatus *status =
            &g_app_phase_lock_status.channels[channel];
        if (!isfinite(measurement->phase_rad)
            || !isfinite(measurement->amplitude_code))
        {
            AppPhaseLock_RecordInvalid(APP_PHASE_LOCK_ERROR_MODEL);
            return false;
        }

        float phase_error = 0.0f;
        if (!tracker->reference_valid)
        {
            tracker->cosine_phase_rad = measurement->phase_rad;
            tracker->epoch_sys_timer = sample0_sys_timer;
            tracker->reference_valid = true;
        }
        else
        {
            uint32_t delta_ticks =
                sample0_sys_timer - tracker->epoch_sys_timer;
            float delta_seconds = (float)delta_ticks
                / (float)s_sys_timer_hz;
            if (delta_seconds <= 0.0f)
            {
                AppPhaseLock_RecordInvalid(
                    APP_PHASE_LOCK_ERROR_INVALID_ARGUMENT);
                return false;
            }

            float predicted_phase = app_phase_lock_wrap_pi(
                tracker->cosine_phase_rad
                + APP_PHASE_LOCK_TWO_PI_F
                    * tracker->tracked_frequency_hz * delta_seconds);
            phase_error = app_phase_lock_wrap_pi(
                measurement->phase_rad - predicted_phase);

            float frequency_step = APP_PHASE_LOCK_FREQUENCY_GAIN
                * phase_error
                / (APP_PHASE_LOCK_TWO_PI_F * delta_seconds);
            frequency_step = app_phase_lock_clamp(
                frequency_step,
                -APP_PHASE_LOCK_MAX_FREQUENCY_STEP_HZ,
                APP_PHASE_LOCK_MAX_FREQUENCY_STEP_HZ);
            tracker->tracked_frequency_hz += frequency_step;
            float minimum_frequency_hz = app_phase_lock_clamp(
                tracker->nominal_frequency_hz
                    - APP_PHASE_LOCK_MAX_FREQUENCY_OFFSET_HZ,
                (float)APP_PHASE_LOCK_MIN_OUTPUT_FREQUENCY_HZ,
                (float)APP_PHASE_LOCK_MAX_OUTPUT_FREQUENCY_HZ);
            float maximum_frequency_hz = app_phase_lock_clamp(
                tracker->nominal_frequency_hz
                    + APP_PHASE_LOCK_MAX_FREQUENCY_OFFSET_HZ,
                (float)APP_PHASE_LOCK_MIN_OUTPUT_FREQUENCY_HZ,
                (float)APP_PHASE_LOCK_MAX_OUTPUT_FREQUENCY_HZ);
            tracker->tracked_frequency_hz = app_phase_lock_clamp(
                tracker->tracked_frequency_hz,
                minimum_frequency_hz,
                maximum_frequency_hz);
            tracker->cosine_phase_rad = app_phase_lock_wrap_pi(
                predicted_phase
                + APP_PHASE_LOCK_PHASE_GAIN * phase_error);
            tracker->epoch_sys_timer = sample0_sys_timer;
        }

        status->tracked_frequency_hz = tracker->tracked_frequency_hz;
        status->cosine_phase_rad = tracker->cosine_phase_rad;
        status->phase_error_rad = phase_error;
        status->amplitude_code = measurement->amplitude_code;
        if (tracker->reference_valid
            && (fabsf(phase_error) <= APP_PHASE_LOCK_LOCK_ERROR_RAD))
        {
            ++status->good_frame_streak;
        }
        else
        {
            status->good_frame_streak = 0U;
        }
        if (fabsf(phase_error) >= APP_PHASE_LOCK_UNLOCK_ERROR_RAD)
        {
            status->locked = false;
        }
        else if (status->good_frame_streak
                 >= APP_PHASE_LOCK_REQUIRED_GOOD_FRAMES)
        {
            status->locked = true;
        }
        all_locked = all_locked && status->locked;

        references[channel].frequency_hz = tracker->tracked_frequency_hz;
        references[channel].cosine_phase_rad = tracker->cosine_phase_rad;
        references[channel].epoch_sys_timer = sample0_sys_timer;
    }

    ++g_app_phase_lock_status.valid_frames;
    g_app_phase_lock_status.consecutive_invalid_frames = 0U;
    g_app_phase_lock_status.residual = result->best_residual;
    g_app_phase_lock_status.all_locked = all_locked;
    g_app_phase_lock_status.last_error = APP_PHASE_LOCK_ERROR_NONE;
    return true;
}

void AppPhaseLock_RecordInvalid(AppPhaseLockError error)
{
    ++g_app_phase_lock_status.invalid_frames;
    ++g_app_phase_lock_status.consecutive_invalid_frames;
    g_app_phase_lock_status.last_error = error;
    if (g_app_phase_lock_status.consecutive_invalid_frames
        >= APP_PHASE_LOCK_MAX_INVALID_FRAMES)
    {
        g_app_phase_lock_status.all_locked = false;
        for (uint32_t channel = 0U;
             channel < SIGNAL_JOINT_COMPONENT_COUNT;
             ++channel)
        {
            g_app_phase_lock_status.channels[channel].locked = false;
            g_app_phase_lock_status.channels[channel].good_frame_streak = 0U;
        }
    }
}

void AppPhaseLock_End(void)
{
    if (g_app_phase_lock_status.running)
    {
        ++g_app_phase_lock_status.stop_count;
    }
    g_app_phase_lock_status.running = false;
    g_app_phase_lock_status.all_locked = false;
}
