/**
  ******************************************************************************
  * @file    app_dac_output.c
  * @brief   片内双 DAC 波表生成、DMA 启停与最小诊断（UTF-8）。
  ******************************************************************************
  */

#include "app_dac_output.h"

#include "app_dac_output_config.h"
#include "app_trig_lookup.h"
#include "cmsis_os2.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define APP_DAC_OUTPUT_TWO_PI_F       6.28318530717958647692f
#define APP_DAC_OUTPUT_TWO_PI_D       6.28318530717958647692
#define APP_DAC_OUTPUT_Q32_SCALE_D    4294967296.0
#define APP_DAC_OUTPUT_Q15_SCALE      32768L

_Static_assert((APP_DAC_OUTPUT_BUFFER_POINTS % 2U) == 0U,
               "DAC buffer must contain two equal halves");
_Static_assert(APP_DAC_OUTPUT_BUFFER_POINTS <= 65535U,
               "DAC DMA transfer exceeds NDTR range");
_Static_assert((APP_DAC_OUTPUT_HALF_POINTS % 8U) == 0U,
               "DAC half buffer must preserve cache-line alignment");
_Static_assert(APP_DAC_OUTPUT_CENTER_CODE >= APP_DAC_OUTPUT_PEAK_CODE,
               "DAC output range is below zero");
_Static_assert((APP_DAC_OUTPUT_CENTER_CODE + APP_DAC_OUTPUT_PEAK_CODE) <= 4095U,
               "DAC output range exceeds 12-bit full scale");

typedef struct
{
    float frequency_hz;
    float wave_phase_rad;
    uint32_t epoch_sys_timer;
} AppDacOutputPhaseModel;

static AppDacOutputPort s_port;
static bool s_port_configured;

static AppDacOutputPhaseModel
    s_phase_models[APP_DAC_OUTPUT_CHANNEL_COUNT];
static SignalWaveType s_wave_types[APP_DAC_OUTPUT_CHANNEL_COUNT];
static uint64_t s_next_half_sample[2];
static volatile uint32_t s_dma_event_count[2];
static uint32_t s_dma_service_count[2];
static osThreadId_t s_worker_task;
static uint32_t s_output_start_sys_timer;
static uint32_t s_sys_timer_frequency_hz;
static volatile uint32_t s_phase_difference_deg;

volatile AppDacOutputStatus g_app_dac_output_status = {
    .last_error = APP_DAC_OUTPUT_ERROR_NOT_INITIALIZED,
};

static bool app_dac_output_wave_supported(SignalWaveType wave_type)
{
    return (wave_type == SIGNAL_WAVE_SINE)
        || (wave_type == SIGNAL_WAVE_TRIANGLE);
}

static bool app_dac_output_frequency_supported(uint32_t frequency_hz)
{
    return (frequency_hz >= APP_DAC_OUTPUT_MIN_FREQUENCY_HZ)
        && (frequency_hz <= APP_DAC_OUTPUT_MAX_FREQUENCY_HZ);
}

static bool app_dac_output_tracking_frequency_supported(float frequency_hz)
{
    return isfinite(frequency_hz)
        && (frequency_hz >= (float)APP_DAC_OUTPUT_MIN_FREQUENCY_HZ)
        && (frequency_hz <= (float)APP_DAC_OUTPUT_MAX_FREQUENCY_HZ);
}

static bool app_dac_output_quantize_vote_frequency(float frequency_hz,
                                                    uint32_t *quantized_hz)
{
    if ((quantized_hz == NULL) || !isfinite(frequency_hz)
        || (frequency_hz < 0.0f))
    {
        return false;
    }

    uint32_t measured_hz = (uint32_t)(frequency_hz + 0.5f);
    uint32_t rounded_hz = ((measured_hz
                            + (APP_DAC_OUTPUT_FREQUENCY_STEP_HZ / 2U))
                           / APP_DAC_OUTPUT_FREQUENCY_STEP_HZ)
        * APP_DAC_OUTPUT_FREQUENCY_STEP_HZ;
    uint32_t difference_hz = (measured_hz > rounded_hz)
        ? (measured_hz - rounded_hz)
        : (rounded_hz - measured_hz);

    if (!app_dac_output_frequency_supported(rounded_hz)
        || ((rounded_hz % APP_DAC_OUTPUT_FREQUENCY_STEP_HZ) != 0U)
        || (difference_hz > APP_DAC_OUTPUT_VOTE_TOLERANCE_HZ))
    {
        return false;
    }

    *quantized_hz = rounded_hz;
    return true;
}

static bool app_dac_output_harmonic_ratio(
    const AppDacOutputConfig *config,
    uint8_t *harmonic_ratio)
{
    if ((config == NULL) || (harmonic_ratio == NULL)
        || (config->channels[0].wave_type != SIGNAL_WAVE_SINE)
        || (config->channels[1].wave_type != SIGNAL_WAVE_SINE))
    {
        return false;
    }

    uint32_t frequency_a_hz = config->channels[0].frequency_hz;
    uint32_t frequency_b_hz = config->channels[1].frequency_hz;
    if ((frequency_a_hz == 0U) || (frequency_b_hz <= frequency_a_hz))
    {
        return false;
    }

    uint32_t ratio = frequency_b_hz / frequency_a_hz;
    if ((ratio < 2U) || (ratio > UINT8_MAX)
        || (frequency_b_hz != (frequency_a_hz * ratio)))
    {
        return false;
    }

    *harmonic_ratio = (uint8_t)ratio;
    return true;
}

static uint32_t app_dac_output_phase_step(float frequency_hz)
{
    double step = ((double)frequency_hz
                   * APP_DAC_OUTPUT_Q32_SCALE_D)
        / (double)APP_DAC_OUTPUT_SAMPLE_RATE_HZ;
    return (uint32_t)(step + 0.5);
}

static uint32_t app_dac_output_phase_from_degrees(float phase_deg)
{
    double cycles = (double)phase_deg / 360.0;
    cycles -= floor(cycles);
    return (uint32_t)(cycles * APP_DAC_OUTPUT_Q32_SCALE_D);
}

static int32_t app_dac_output_triangle_q15(uint32_t phase_q32)
{
    uint32_t phase_q16 = phase_q32 >> 16U;
    if (phase_q16 < 16384U)
    {
        return (int32_t)(phase_q16 << 1U);
    }
    if (phase_q16 < 49152U)
    {
        return 65535L - ((int32_t)phase_q16 << 1U);
    }
    return ((int32_t)phase_q16 << 1U) - 131072L;
}

static uint32_t app_dac_output_sample_code(SignalWaveType wave_type,
                                           uint32_t phase_q32)
{
    int32_t normalized_q15;
    if (wave_type == SIGNAL_WAVE_SINE)
    {
        normalized_q15 = AppTrigLookup_SineQ15(phase_q32);
    }
    else
    {
        normalized_q15 = app_dac_output_triangle_q15(phase_q32);
    }

    int32_t code = (int32_t)APP_DAC_OUTPUT_CENTER_CODE
        + (normalized_q15 * (int32_t)APP_DAC_OUTPUT_PEAK_CODE)
            / APP_DAC_OUTPUT_Q15_SCALE;
    if (code < 0)
    {
        code = 0;
    }
    else if (code > 4095)
    {
        code = 4095;
    }
    return (uint32_t)code;
}

static uint32_t app_dac_output_sample_sys_timer(uint64_t sample_index)
{
    uint64_t elapsed_ticks = ((sample_index + 1ULL)
                              * (uint64_t)s_sys_timer_frequency_hz
                              + (APP_DAC_OUTPUT_SAMPLE_RATE_HZ / 2U))
        / APP_DAC_OUTPUT_SAMPLE_RATE_HZ;
    return s_output_start_sys_timer + (uint32_t)elapsed_ticks;
}

static uint32_t app_dac_output_phase_at_sample(
    const AppDacOutputPhaseModel *model,
    uint64_t sample_index)
{
    uint32_t sample_tick = app_dac_output_sample_sys_timer(sample_index);
    int32_t delta_ticks = (int32_t)(sample_tick - model->epoch_sys_timer);
    double cycles = (double)model->wave_phase_rad
            / APP_DAC_OUTPUT_TWO_PI_D
        + ((double)model->frequency_hz * (double)delta_ticks)
            / (double)s_sys_timer_frequency_hz;
    cycles -= floor(cycles);
    return (uint32_t)(cycles * APP_DAC_OUTPUT_Q32_SCALE_D);
}

static uint32_t app_dac_output_elapsed_us(uint32_t start_timer)
{
    if (s_sys_timer_frequency_hz == 0U)
    {
        return 0U;
    }
    uint32_t elapsed_ticks = osKernelGetSysTimerCount() - start_timer;
    return (uint32_t)(((uint64_t)elapsed_ticks * 1000000ULL)
                      / s_sys_timer_frequency_hz);
}

static void app_dac_output_clean_cache(uint32_t *address, uint32_t points)
{
    SCB_CleanDCache_by_Addr(address,
                           (int32_t)(points * sizeof(uint32_t)));
}

static void app_dac_output_fill_initial(const AppDacOutputConfig *config)
{
    uint32_t phase[APP_DAC_OUTPUT_CHANNEL_COUNT];
    uint32_t step[APP_DAC_OUTPUT_CHANNEL_COUNT];
    for (uint32_t channel = 0U;
         channel < APP_DAC_OUTPUT_CHANNEL_COUNT;
         ++channel)
    {
        phase[channel] = app_dac_output_phase_from_degrees(
            config->channels[channel].phase_deg);
        step[channel] = app_dac_output_phase_step(
            (float)config->channels[channel].frequency_hz);
    }

    for (uint32_t index = 0U;
         index < APP_DAC_OUTPUT_BUFFER_POINTS;
         ++index)
    {
        uint32_t channel_1 = app_dac_output_sample_code(
            config->channels[0].wave_type, phase[0]);
        uint32_t channel_2 = app_dac_output_sample_code(
            config->channels[1].wave_type, phase[1]);
        s_port.buffer[index] = channel_1 | (channel_2 << 16U);
        phase[0] += step[0];
        phase[1] += step[1];
    }
}

static void app_dac_output_fill_half(uint32_t half_index)
{
    uint64_t start_sample = s_next_half_sample[half_index];
    uint32_t phase[APP_DAC_OUTPUT_CHANNEL_COUNT];
    uint32_t step[APP_DAC_OUTPUT_CHANNEL_COUNT];
    for (uint32_t channel = 0U;
         channel < APP_DAC_OUTPUT_CHANNEL_COUNT;
         ++channel)
    {
        phase[channel] = app_dac_output_phase_at_sample(
            &s_phase_models[channel], start_sample);
        step[channel] = app_dac_output_phase_step(
            s_phase_models[channel].frequency_hz);

        /* 每次补数都把参考点推进到当前半缓冲，避免32位系统时基跨周。 */
        s_phase_models[channel].wave_phase_rad =
            (float)((double)phase[channel]
                    * APP_DAC_OUTPUT_TWO_PI_D
                    / APP_DAC_OUTPUT_Q32_SCALE_D);
        s_phase_models[channel].epoch_sys_timer =
            app_dac_output_sample_sys_timer(start_sample);
    }

    uint32_t offset = half_index * APP_DAC_OUTPUT_HALF_POINTS;
    for (uint32_t index = 0U;
         index < APP_DAC_OUTPUT_HALF_POINTS;
         ++index)
    {
        uint32_t channel_1 = app_dac_output_sample_code(
            s_wave_types[0], phase[0]);
        uint32_t channel_2 = app_dac_output_sample_code(
            s_wave_types[1], phase[1]);
        s_port.buffer[offset + index] =
            channel_1 | (channel_2 << 16U);
        phase[0] += step[0];
        phase[1] += step[1];
    }

    app_dac_output_clean_cache(&s_port.buffer[offset],
                               APP_DAC_OUTPUT_HALF_POINTS);
    s_next_half_sample[half_index] += APP_DAC_OUTPUT_BUFFER_POINTS;
}

bool AppDacOutput_ConfigurePort(const AppDacOutputPort *port)
{
    if ((port == NULL)
        || (port->dac == NULL)
        || (port->trigger_timer == NULL)
        || (port->dma_lisr == NULL)
        || (port->buffer == NULL)
        || (port->buffer_points < APP_DAC_OUTPUT_BUFFER_POINTS)
        || (((uintptr_t)port->buffer & 31U) != 0U)
        || (port->dma_memory_start >= port->dma_memory_end)
        || ((uintptr_t)port->buffer < port->dma_memory_start)
        || (((uintptr_t)port->buffer
             + (APP_DAC_OUTPUT_BUFFER_POINTS * sizeof(uint32_t)))
            > port->dma_memory_end)
        || (port->timer_kernel_clock_hz == 0U)
        || g_app_dac_output_status.running)
    {
        return false;
    }

    uint64_t timer_divisor =
        ((uint64_t)port->trigger_timer->Init.Prescaler + 1ULL)
        * ((uint64_t)port->trigger_timer->Init.Period + 1ULL);
    if ((timer_divisor == 0ULL)
        || (((uint64_t)port->timer_kernel_clock_hz % timer_divisor) != 0ULL)
        || ((port->timer_kernel_clock_hz / timer_divisor)
            != APP_DAC_OUTPUT_SAMPLE_RATE_HZ))
    {
        return false;
    }

    s_port = *port;
    s_port_configured = true;
    return true;
}

bool AppDacOutput_Init(void)
{
    if (g_app_dac_output_status.initialized)
    {
        return true;
    }

    /* HAL资源与DMA可达内存由工程适配层注入。 */
    if (!s_port_configured
        || (s_port.dac->Instance == NULL)
        || (s_port.dac->DMA_Handle1 == NULL)
        || (HAL_DAC_GetState(s_port.dac) == HAL_DAC_STATE_RESET))
    {
        g_app_dac_output_status.last_error = APP_DAC_OUTPUT_ERROR_HAL_INIT;
        return false;
    }

    s_sys_timer_frequency_hz = osKernelGetSysTimerFreq();
    if (s_sys_timer_frequency_hz == 0U)
    {
        g_app_dac_output_status.last_error = APP_DAC_OUTPUT_ERROR_HAL_INIT;
        return false;
    }

    g_app_dac_output_status.hal_error_code = HAL_DAC_GetError(s_port.dac);
    g_app_dac_output_status.last_error = APP_DAC_OUTPUT_ERROR_NONE;
    g_app_dac_output_status.initialized = true;
    return true;
}

bool AppDacOutput_BindCurrentTask(void)
{
    s_worker_task = osThreadGetId();
    return s_worker_task != NULL;
}

bool AppDacOutput_SetPhaseDifference(uint16_t phase_deg)
{
    /* 题目要求先设相位再启动；运行中改参考会造成DAC相位瞬态跳变。 */
    if (g_app_dac_output_status.running)
    {
        return false;
    }

    if ((phase_deg > APP_DAC_OUTPUT_PHASE_MAX_DEG)
        || (((phase_deg - APP_DAC_OUTPUT_PHASE_MIN_DEG)
             % APP_DAC_OUTPUT_PHASE_STEP_DEG) != 0U))
    {
        ++g_app_dac_output_status.rejected_count;
        g_app_dac_output_status.last_error =
            APP_DAC_OUTPUT_ERROR_INVALID_ARGUMENT;
        return false;
    }

    s_phase_difference_deg = phase_deg;
    __DMB();
    g_app_dac_output_status.phase_difference_deg = phase_deg;
    return true;
}

uint16_t AppDacOutput_GetPhaseDifference(void)
{
    __DMB();
    return (uint16_t)s_phase_difference_deg;
}

bool AppDacOutput_Start(const AppDacOutputConfig *config)
{
    if (config == NULL)
    {
        ++g_app_dac_output_status.rejected_count;
        g_app_dac_output_status.last_error =
            APP_DAC_OUTPUT_ERROR_INVALID_ARGUMENT;
        return false;
    }

    for (uint32_t channel = 0U;
         channel < APP_DAC_OUTPUT_CHANNEL_COUNT;
         ++channel)
    {
        if (!app_dac_output_frequency_supported(
                config->channels[channel].frequency_hz))
        {
            ++g_app_dac_output_status.rejected_count;
            g_app_dac_output_status.last_error =
                APP_DAC_OUTPUT_ERROR_UNSUPPORTED_FREQUENCY;
            return false;
        }
        if (!app_dac_output_wave_supported(
                config->channels[channel].wave_type))
        {
            ++g_app_dac_output_status.rejected_count;
            g_app_dac_output_status.last_error =
                APP_DAC_OUTPUT_ERROR_UNSUPPORTED_WAVE;
            return false;
        }
        if (!isfinite(config->channels[channel].phase_deg))
        {
            ++g_app_dac_output_status.rejected_count;
            g_app_dac_output_status.last_error =
                APP_DAC_OUTPUT_ERROR_INVALID_ARGUMENT;
            return false;
        }
    }

    if (!AppDacOutput_Init())
    {
        return false;
    }

    if (s_worker_task == NULL)
    {
        g_app_dac_output_status.last_error =
            APP_DAC_OUTPUT_ERROR_WORKER_NOT_BOUND;
        return false;
    }

    AppDacOutput_Stop();
    g_app_dac_output_status.phase_control_eligible = false;
    g_app_dac_output_status.phase_control_active = false;
    g_app_dac_output_status.harmonic_ratio = 0U;
    app_dac_output_fill_initial(config);
    app_dac_output_clean_cache(s_port.buffer,
                               APP_DAC_OUTPUT_BUFFER_POINTS);
    memset((void *)s_dma_event_count, 0, sizeof(s_dma_event_count));
    memset(s_dma_service_count, 0, sizeof(s_dma_service_count));
    (void)osThreadFlagsClear(APP_DAC_OUTPUT_EVENT_MASK);

    s_port.dac->ErrorCode = HAL_DAC_ERROR_NONE;
    __HAL_DAC_CLEAR_FLAG(s_port.dac, DAC_FLAG_DMAUDR1 | DAC_FLAG_DMAUDR2);
    if (HAL_DACEx_DualStart_DMA(s_port.dac,
                               s_port.dac_channel,
                               s_port.buffer,
                               APP_DAC_OUTPUT_BUFFER_POINTS,
                               DAC_ALIGN_12B_R) != HAL_OK)
    {
        g_app_dac_output_status.hal_error_code = HAL_DAC_GetError(s_port.dac);
        g_app_dac_output_status.last_error =
            APP_DAC_OUTPUT_ERROR_DMA_START;
        return false;
    }

    s_output_start_sys_timer = osKernelGetSysTimerCount();
    if (HAL_TIM_Base_Start(s_port.trigger_timer) != HAL_OK)
    {
        (void)HAL_DACEx_DualStop_DMA(s_port.dac, s_port.dac_channel);
        g_app_dac_output_status.hal_error_code = HAL_DAC_GetError(s_port.dac);
        g_app_dac_output_status.last_error =
            APP_DAC_OUTPUT_ERROR_TIMER_START;
        return false;
    }

    for (uint32_t channel = 0U;
         channel < APP_DAC_OUTPUT_CHANNEL_COUNT;
         ++channel)
    {
        g_app_dac_output_status.frequency_hz[channel] =
            config->channels[channel].frequency_hz;
        g_app_dac_output_status.wave_type[channel] =
            config->channels[channel].wave_type;
        g_app_dac_output_status.tracked_frequency_hz[channel] =
            (float)config->channels[channel].frequency_hz;
        s_wave_types[channel] = config->channels[channel].wave_type;
        s_phase_models[channel].frequency_hz =
            (float)config->channels[channel].frequency_hz;
        s_phase_models[channel].wave_phase_rad =
            config->channels[channel].phase_deg
            * APP_DAC_OUTPUT_TWO_PI_F / 360.0f;
        s_phase_models[channel].epoch_sys_timer =
            app_dac_output_sample_sys_timer(0U);
    }
    s_next_half_sample[0] = APP_DAC_OUTPUT_BUFFER_POINTS;
    s_next_half_sample[1] = APP_DAC_OUTPUT_BUFFER_POINTS
        + APP_DAC_OUTPUT_HALF_POINTS;
    ++g_app_dac_output_status.start_count;
    g_app_dac_output_status.hal_error_code = HAL_DAC_GetError(s_port.dac);
    g_app_dac_output_status.last_error = APP_DAC_OUTPUT_ERROR_NONE;
    g_app_dac_output_status.running = true;
    return true;
}

bool AppDacOutput_StartFromVote(const SignalVoteResult *result)
{
    if ((result == NULL) || !result->valid)
    {
        ++g_app_dac_output_status.rejected_count;
        g_app_dac_output_status.last_error =
            APP_DAC_OUTPUT_ERROR_INVALID_ARGUMENT;
        return false;
    }

    AppDacOutputConfig config = {0};
    for (uint32_t channel = 0U;
         channel < APP_DAC_OUTPUT_CHANNEL_COUNT;
         ++channel)
    {
        if (!app_dac_output_quantize_vote_frequency(
                result->components[channel].frequency_hz,
                &config.channels[channel].frequency_hz))
        {
            ++g_app_dac_output_status.rejected_count;
            g_app_dac_output_status.last_error =
                APP_DAC_OUTPUT_ERROR_UNSUPPORTED_FREQUENCY;
            return false;
        }
        config.channels[channel].wave_type =
            result->components[channel].type;
        config.channels[channel].phase_deg = 0.0f;
    }

    uint8_t harmonic_ratio = 0U;
    bool phase_control_eligible = app_dac_output_harmonic_ratio(
        &config, &harmonic_ratio);
    if (phase_control_eligible)
    {
        config.channels[1].phase_deg =
            (float)AppDacOutput_GetPhaseDifference();
    }

    if (!AppDacOutput_Start(&config))
    {
        return false;
    }

    g_app_dac_output_status.phase_control_eligible =
        phase_control_eligible;
    g_app_dac_output_status.phase_control_active =
        phase_control_eligible;
    g_app_dac_output_status.harmonic_ratio =
        phase_control_eligible ? harmonic_ratio : 0U;
    return true;
}

bool AppDacOutput_UpdatePhaseReferences(
    const AppDacOutputPhaseReference references[APP_DAC_OUTPUT_CHANNEL_COUNT])
{
    if ((references == NULL) || !g_app_dac_output_status.running)
    {
        return false;
    }

    for (uint32_t channel = 0U;
         channel < APP_DAC_OUTPUT_CHANNEL_COUNT;
         ++channel)
    {
        if (!app_dac_output_tracking_frequency_supported(
                references[channel].frequency_hz)
            || !isfinite(references[channel].wave_phase_rad))
        {
            return false;
        }
    }

    for (uint32_t channel = 0U;
         channel < APP_DAC_OUTPUT_CHANNEL_COUNT;
         ++channel)
    {
        s_phase_models[channel].frequency_hz =
            references[channel].frequency_hz;
        s_phase_models[channel].wave_phase_rad =
            references[channel].wave_phase_rad;
        s_phase_models[channel].epoch_sys_timer =
            references[channel].epoch_sys_timer;
        g_app_dac_output_status.tracked_frequency_hz[channel] =
            references[channel].frequency_hz;
    }
    return true;
}

uint32_t AppDacOutput_ServiceEvents(uint32_t events)
{
    if (!g_app_dac_output_status.running)
    {
        return 0U;
    }

    uint32_t serviced = 0U;
    for (uint32_t half = 0U; half < 2U; ++half)
    {
        uint32_t flag = (half == 0U)
            ? APP_DAC_OUTPUT_EVENT_HALF : APP_DAC_OUTPUT_EVENT_FULL;
        if ((events & flag) == 0U)
        {
            continue;
        }

        uint32_t event_count = s_dma_event_count[half];
        if ((event_count - s_dma_service_count[half]) > 1U)
        {
            ++g_app_dac_output_status.refill_overrun_count;
            g_app_dac_output_status.last_error =
                APP_DAC_OUTPUT_ERROR_REFILL_OVERRUN;
        }

        uint32_t refill_start = osKernelGetSysTimerCount();
        app_dac_output_fill_half(half);
        uint32_t refill_time_us = app_dac_output_elapsed_us(refill_start);
        g_app_dac_output_status.refill_time_last_us = refill_time_us;
        if (refill_time_us > g_app_dac_output_status.refill_time_max_us)
        {
            g_app_dac_output_status.refill_time_max_us = refill_time_us;
        }
        s_dma_service_count[half] = event_count;
        if (half == 0U)
        {
            ++g_app_dac_output_status.half_refill_count;
        }
        else
        {
            ++g_app_dac_output_status.full_refill_count;
        }
        ++serviced;
    }
    return serviced;
}

void AppDacOutput_Stop(void)
{
    g_app_dac_output_status.phase_control_active = false;
    if (!g_app_dac_output_status.initialized
        || !g_app_dac_output_status.running)
    {
        return;
    }

    (void)HAL_TIM_Base_Stop(s_port.trigger_timer);
    (void)HAL_DACEx_DualStop_DMA(s_port.dac, s_port.dac_channel);
    ++g_app_dac_output_status.stop_count;
    g_app_dac_output_status.hal_error_code = HAL_DAC_GetError(s_port.dac);
    g_app_dac_output_status.running = false;
}

bool AppDacOutput_IsRunning(void)
{
    return g_app_dac_output_status.running;
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *dac_handle)
{
    if (!s_port_configured || (dac_handle != s_port.dac)
        || !g_app_dac_output_status.running)
    {
        return;
    }

    ++s_dma_event_count[0];
    if ((s_worker_task == NULL)
        || ((osThreadFlagsSet(s_worker_task, APP_DAC_OUTPUT_EVENT_HALF)
             & osFlagsError) != 0U))
    {
        ++g_app_dac_output_status.worker_signal_error_count;
    }
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *dac_handle)
{
    if (!s_port_configured || (dac_handle != s_port.dac)
        || !g_app_dac_output_status.running)
    {
        return;
    }

    ++s_dma_event_count[1];
    if ((s_worker_task == NULL)
        || ((osThreadFlagsSet(s_worker_task, APP_DAC_OUTPUT_EVENT_FULL)
             & osFlagsError) != 0U))
    {
        ++g_app_dac_output_status.worker_signal_error_count;
    }
}

void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef *dac_handle)
{
    if (!s_port_configured || (dac_handle != s_port.dac))
    {
        return;
    }
    (void)HAL_TIM_Base_Stop(s_port.trigger_timer);
    ++g_app_dac_output_status.dma_error_count;
    g_app_dac_output_status.hal_error_code = HAL_DAC_GetError(dac_handle);
    g_app_dac_output_status.last_error =
        APP_DAC_OUTPUT_ERROR_DMA_TRANSFER;
    g_app_dac_output_status.running = false;
}

void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef *dac_handle)
{
    if (!s_port_configured || (dac_handle != s_port.dac))
    {
        return;
    }
    DMA_HandleTypeDef *dma_handle = dac_handle->DMA_Handle1;
    if ((dma_handle != NULL) && (dma_handle->Instance != NULL))
    {
        DMA_Stream_TypeDef *stream = dma_handle->Instance;
        g_app_dac_output_status.underrun_dma_remaining = stream->NDTR;
        g_app_dac_output_status.underrun_dma_cr = stream->CR;
        g_app_dac_output_status.underrun_dma_fcr = stream->FCR;
        g_app_dac_output_status.underrun_dma_error = dma_handle->ErrorCode;
        g_app_dac_output_status.underrun_dma_state = dma_handle->State;
    }
    g_app_dac_output_status.underrun_tick_ms = HAL_GetTick();
    g_app_dac_output_status.underrun_dma_lisr = *s_port.dma_lisr;
    (void)HAL_TIM_Base_Stop(s_port.trigger_timer);
    ++g_app_dac_output_status.underrun_count;
    g_app_dac_output_status.hal_error_code = HAL_DAC_GetError(dac_handle);
    g_app_dac_output_status.last_error =
        APP_DAC_OUTPUT_ERROR_DMA_UNDERRUN;
    g_app_dac_output_status.running = false;
}
