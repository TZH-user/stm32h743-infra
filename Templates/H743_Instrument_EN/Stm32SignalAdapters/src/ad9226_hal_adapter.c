/**
  ******************************************************************************
  * @file    ad9226.c
  * @brief   TIM2 + DMA double-buffer acquisition of AD9226 parallel samples.
  ******************************************************************************
  */

#include "ad9226.h"


#include <stddef.h>
#include <string.h>

#define AD9226_DATA_GPIO_PORT              GPIOB
#define AD9226_DATA_GPIO_PINS              ((uint16_t)0x0FFFU)
#define AD9226_DATA_MASK                   0x0FFFU

#define AD9226_CLOCK_GPIO_PORT             GPIOA
#define AD9226_CLOCK_GPIO_PIN              GPIO_PIN_0
#define AD9226_CLOCK_GPIO_AF               GPIO_AF1_TIM2
#define AD9226_CLOCK_TIMER_CHANNEL         TIM_CHANNEL_1
#define AD9226_DMA_TIMER_CHANNEL           TIM_CHANNEL_2
#define AD9226_DMA_TIMER_REQUEST           TIM_DMA_CC2

#define AD9226_DCACHE_LINE_SIZE            32U
#define AD9226_MIN_PERIOD_TICKS             8U
#define AD9226_DMA_MAX_TRANSFER_SAMPLES     65535U

#if ((AD9226_FRAME_SAMPLES == 0U) || \
     (AD9226_FRAME_SAMPLES > AD9226_DMA_MAX_TRANSFER_SAMPLES))
#error "AD9226_FRAME_SAMPLES must be in the range 1..65535"
#endif

#if ((AD9226_FRAME_SAMPLES % 16U) != 0U)
#error "AD9226_FRAME_SAMPLES must be a multiple of 16 samples (32 bytes)"
#endif

#if ((AD9226_DMA_SAMPLE_PHASE_DEN == 0U) || \
     (AD9226_DMA_SAMPLE_PHASE_NUM >= AD9226_DMA_SAMPLE_PHASE_DEN))
#error "AD9226 DMA sample phase must be between 0 and 1 period"
#endif

/*
 * DMA1/DMA2 cannot access DTCM. The template linker script maps
 * .dma_buffer.* into RAM_D2. Each target is cache-line aligned for safe
 * invalidation; the ADC suffix keeps the fixed DAC buffer first in RAM_D2.
 */
__attribute__((section(".dma_buffer.adc"), aligned(AD9226_DCACHE_LINE_SIZE)))
static uint16_t s_dma_buffer0[AD9226_FRAME_SAMPLES];

__attribute__((section(".dma_buffer.adc"), aligned(AD9226_DCACHE_LINE_SIZE)))
static uint16_t s_dma_buffer1[AD9226_FRAME_SAMPLES];

typedef struct
{
    TIM_HandleTypeDef *timer;
    DMA_HandleTypeDef *dma;
    osSemaphoreId_t frame_ready_semaphore;
    uint32_t timer_kernel_clock_hz;
    volatile uint32_t requested_sample_rate_hz;
    volatile uint32_t actual_sample_rate_hz;
    volatile uint32_t completed_frames;
    volatile uint32_t overwritten_frames;
    volatile uint32_t notification_overflows;
    volatile uint32_t dma_errors;
    volatile uint32_t last_dma_error;
    volatile uint32_t published_sequence;
    volatile uint32_t consumed_sequence;
    volatile uint32_t published_completion_sys_timer;
    volatile uint8_t published_buffer_index;
    volatile bool initialized;
    volatile bool running;
} AD9226_Context;

static AD9226_Context s_context;

static uint32_t ad9226_irq_save(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void ad9226_irq_restore(uint32_t primask)
{
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }
}

static void ad9226_invalidate_dcache(const void *address, uint32_t length)
{
#if (__DCACHE_PRESENT == 1U)
    if ((address == NULL) || (length == 0U))
    {
        return;
    }

    uintptr_t start =
        ((uintptr_t)address)
        & ~((uintptr_t)AD9226_DCACHE_LINE_SIZE - 1U);

    uintptr_t end =
        ((uintptr_t)address + (uintptr_t)length
         + (uintptr_t)AD9226_DCACHE_LINE_SIZE - 1U)
        & ~((uintptr_t)AD9226_DCACHE_LINE_SIZE - 1U);

    SCB_InvalidateDCache_by_Addr(
        (uint32_t *)start,
        (int32_t)(end - start)
    );

    __DSB();
    __ISB();
#else
    (void)address;
    (void)length;
#endif
}

static void ad9226_configure_clock_pin_af(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = AD9226_CLOCK_GPIO_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = AD9226_CLOCK_GPIO_AF;
    HAL_GPIO_Init(AD9226_CLOCK_GPIO_PORT, &gpio);
}

static void ad9226_drive_clock_pin_low(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(
        AD9226_CLOCK_GPIO_PORT,
        AD9226_CLOCK_GPIO_PIN,
        GPIO_PIN_RESET
    );

    gpio.Pin = AD9226_CLOCK_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(AD9226_CLOCK_GPIO_PORT, &gpio);
}

static void ad9226_configure_data_bus_input(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = AD9226_DATA_GPIO_PINS;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(AD9226_DATA_GPIO_PORT, &gpio);
}

static bool ad9226_dma_configuration_is_valid(const DMA_HandleTypeDef *dma)
{
    if (dma == NULL)
    {
        return false;
    }

    return (dma->Init.Request == DMA_REQUEST_TIM2_CH2)
        && (dma->Init.Direction == DMA_PERIPH_TO_MEMORY)
        && (dma->Init.PeriphInc == DMA_PINC_DISABLE)
        && (dma->Init.MemInc == DMA_MINC_ENABLE)
        && (dma->Init.PeriphDataAlignment == DMA_PDATAALIGN_HALFWORD)
        && (dma->Init.MemDataAlignment == DMA_MDATAALIGN_HALFWORD);
}

static AD9226_Status ad9226_apply_timer_rate(uint32_t requested_hz,
                                             uint32_t *actual_hz)
{
    if ((s_context.timer == NULL)
        || (s_context.timer_kernel_clock_hz == 0U)
        || (requested_hz == 0U)
        || (requested_hz > AD9226_MAX_SAMPLE_RATE_HZ))
    {
        return AD9226_STATUS_INVALID_ARGUMENT;
    }

    uint32_t period_ticks =
        (s_context.timer_kernel_clock_hz + (requested_hz / 2U))
        / requested_hz;

    if (period_ticks < AD9226_MIN_PERIOD_TICKS)
    {
        return AD9226_STATUS_TIMER_CONFIG_ERROR;
    }

    /* TIM2 is a 32-bit timer, so no prescaler is needed in the intended range. */
    uint32_t prescaler = 0U;
    uint32_t auto_reload = period_ticks - 1U;
    uint32_t clock_falling_edge = period_ticks / 2U;
    uint32_t dma_sample_tick =
        (period_ticks * AD9226_DMA_SAMPLE_PHASE_NUM)
        / AD9226_DMA_SAMPLE_PHASE_DEN;

    if (clock_falling_edge == 0U)
    {
        clock_falling_edge = 1U;
    }

    if (dma_sample_tick <= clock_falling_edge)
    {
        dma_sample_tick = clock_falling_edge + 1U;
    }

    if (dma_sample_tick >= period_ticks)
    {
        dma_sample_tick = period_ticks - 1U;
    }

    __HAL_TIM_DISABLE(s_context.timer);
    __HAL_TIM_DISABLE_DMA(s_context.timer, AD9226_DMA_TIMER_REQUEST);

    s_context.timer->Init.Prescaler = prescaler;
    s_context.timer->Init.Period = auto_reload;
    s_context.timer->Instance->PSC = prescaler;
    s_context.timer->Instance->ARR = auto_reload;
    s_context.timer->Instance->CCR1 = clock_falling_edge;
    s_context.timer->Instance->CCR2 = dma_sample_tick;
    s_context.timer->Instance->CNT = 0U;
    s_context.timer->Instance->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(s_context.timer, TIM_FLAG_UPDATE | TIM_FLAG_CC2);

    uint32_t realized_hz =
        s_context.timer_kernel_clock_hz / period_ticks;

    s_context.requested_sample_rate_hz = requested_hz;
    s_context.actual_sample_rate_hz = realized_hz;

    if (actual_hz != NULL)
    {
        *actual_hz = realized_hz;
    }

    return AD9226_STATUS_OK;
}

static bool ad9226_wait_update_events(uint32_t event_count)
{
    if ((s_context.timer == NULL) || (event_count == 0U))
    {
        return true;
    }

    for (uint32_t i = 0U; i < event_count; ++i)
    {
        __HAL_TIM_CLEAR_FLAG(s_context.timer, TIM_FLAG_UPDATE);

        uint32_t timeout = SystemCoreClock / 10U;
        while (__HAL_TIM_GET_FLAG(s_context.timer, TIM_FLAG_UPDATE) == RESET)
        {
            if (timeout == 0U)
            {
                return false;
            }
            --timeout;
        }
    }

    __HAL_TIM_CLEAR_FLAG(s_context.timer, TIM_FLAG_UPDATE);
    return true;
}

static void ad9226_publish_completed_frame(DMA_HandleTypeDef *dma)
{
    if ((dma == NULL) || (dma != s_context.dma) || !s_context.running)
    {
        return;
    }

    /*
     * CT points to the buffer currently being filled after the hardware switch.
     * Therefore the opposite target is the just-completed stable frame.
     */
    uint8_t completed_index =
        ((((DMA_Stream_TypeDef *)dma->Instance)->CR & DMA_SxCR_CT) != 0U)
        ? 0U
        : 1U;

    uint16_t *completed_buffer =
        (completed_index == 0U) ? s_dma_buffer0 : s_dma_buffer1;

    ad9226_invalidate_dcache(
        completed_buffer,
        sizeof(s_dma_buffer0)
    );

    if (s_context.published_sequence != s_context.consumed_sequence)
    {
        ++s_context.overwritten_frames;
    }

    s_context.published_buffer_index = completed_index;
    s_context.published_completion_sys_timer = osKernelGetSysTimerCount();
    ++s_context.completed_frames;
    s_context.published_sequence = s_context.completed_frames;

    if (s_context.frame_ready_semaphore != NULL)
    {
        if (osSemaphoreRelease(s_context.frame_ready_semaphore) != osOK)
        {
            ++s_context.notification_overflows;
        }
    }
}

static void ad9226_dma_complete_callback(DMA_HandleTypeDef *dma)
{
    ad9226_publish_completed_frame(dma);
}

static void ad9226_dma_error_callback(DMA_HandleTypeDef *dma)
{
    if ((dma == NULL) || (dma != s_context.dma))
    {
        return;
    }

    s_context.last_dma_error = dma->ErrorCode;
    ++s_context.dma_errors;
    s_context.running = false;

    if (s_context.timer != NULL)
    {
        __HAL_TIM_DISABLE_DMA(s_context.timer, AD9226_DMA_TIMER_REQUEST);
        __HAL_TIM_DISABLE(s_context.timer);
    }

    if (s_context.frame_ready_semaphore != NULL)
    {
        (void)osSemaphoreRelease(s_context.frame_ready_semaphore);
    }
}

AD9226_Status AD9226_Init(const AD9226_Config *config)
{
    if ((config == NULL)
        || (config->timer == NULL)
        || (config->dma == NULL)
        || (config->frame_ready_semaphore == NULL)
        || (config->timer_kernel_clock_hz == 0U)
        || (config->initial_sample_rate_hz == 0U))
    {
        return AD9226_STATUS_INVALID_ARGUMENT;
    }

    if (config->timer->Instance != TIM2)
    {
        return AD9226_STATUS_TIMER_CONFIG_ERROR;
    }

    if (!ad9226_dma_configuration_is_valid(config->dma))
    {
        return AD9226_STATUS_DMA_CONFIG_ERROR;
    }

    memset(&s_context, 0, sizeof(s_context));

    s_context.timer = config->timer;
    s_context.dma = config->dma;
    s_context.frame_ready_semaphore = config->frame_ready_semaphore;
    s_context.timer_kernel_clock_hz = config->timer_kernel_clock_hz;

    ad9226_drive_clock_pin_low();
    ad9226_configure_data_bus_input();

    AD9226_Status status = ad9226_apply_timer_rate(
        config->initial_sample_rate_hz,
        NULL
    );

    if (status != AD9226_STATUS_OK)
    {
        memset(&s_context, 0, sizeof(s_context));
        return status;
    }

    s_context.initialized = true;
    return AD9226_STATUS_OK;
}

AD9226_Status AD9226_Start(void)
{
    if (!s_context.initialized)
    {
        return AD9226_STATUS_NOT_INITIALIZED;
    }

    if (s_context.running)
    {
        return AD9226_STATUS_BUSY;
    }

    if (!ad9226_dma_configuration_is_valid(s_context.dma))
    {
        return AD9226_STATUS_DMA_CONFIG_ERROR;
    }

    ad9226_configure_data_bus_input();
    ad9226_configure_clock_pin_af();

    while (osSemaphoreAcquire(s_context.frame_ready_semaphore, 0U) == osOK)
    {
        /* Drain a stale notification left by a previous stop/error. */
    }

    s_context.published_sequence = 0U;
    s_context.consumed_sequence = 0U;
    s_context.published_completion_sys_timer = 0U;
    s_context.published_buffer_index = 0U;
    s_context.completed_frames = 0U;
    s_context.overwritten_frames = 0U;
    s_context.notification_overflows = 0U;
    s_context.dma_errors = 0U;
    s_context.last_dma_error = HAL_DMA_ERROR_NONE;

    s_context.dma->XferCpltCallback = ad9226_dma_complete_callback;
    s_context.dma->XferM1CpltCallback = ad9226_dma_complete_callback;
    s_context.dma->XferHalfCpltCallback = NULL;
    s_context.dma->XferM1HalfCpltCallback = NULL;
    s_context.dma->XferErrorCallback = ad9226_dma_error_callback;
    s_context.dma->Parent = s_context.timer;

    __HAL_TIM_DISABLE_DMA(s_context.timer, AD9226_DMA_TIMER_REQUEST);
    __HAL_TIM_DISABLE(s_context.timer);
    __HAL_TIM_SET_COUNTER(s_context.timer, 0U);
    __HAL_TIM_CLEAR_FLAG(
        s_context.timer,
        TIM_FLAG_UPDATE | TIM_FLAG_CC2
    );

    if (HAL_DMAEx_MultiBufferStart_IT(
            s_context.dma,
            (uint32_t)(uintptr_t)&AD9226_DATA_GPIO_PORT->IDR,
            (uint32_t)(uintptr_t)s_dma_buffer0,
            (uint32_t)(uintptr_t)s_dma_buffer1,
            AD9226_FRAME_SAMPLES
        ) != HAL_OK)
    {
        ad9226_drive_clock_pin_low();
        ad9226_configure_data_bus_input();
        return AD9226_STATUS_DMA_START_ERROR;
    }

    s_context.running = true;

    if (HAL_TIM_PWM_Start(
            s_context.timer,
            AD9226_CLOCK_TIMER_CHANNEL
        ) != HAL_OK)
    {
        s_context.running = false;
        (void)HAL_DMA_Abort(s_context.dma);
        ad9226_drive_clock_pin_low();
        ad9226_configure_data_bus_input();
        return AD9226_STATUS_TIMER_START_ERROR;
    }

    /* CH2 has no external pin; it generates one DMA request per sample. */
    if (HAL_TIM_OC_Start(
            s_context.timer,
            AD9226_DMA_TIMER_CHANNEL
        ) != HAL_OK)
    {
        (void)HAL_TIM_PWM_Stop(
            s_context.timer,
            AD9226_CLOCK_TIMER_CHANNEL
        );
        s_context.running = false;
        (void)HAL_DMA_Abort(s_context.dma);
        ad9226_drive_clock_pin_low();
        ad9226_configure_data_bus_input();
        return AD9226_STATUS_TIMER_START_ERROR;
    }

    if (!ad9226_wait_update_events(AD9226_PRIME_CLOCK_CYCLES))
    {
        (void)AD9226_Stop();
        return AD9226_STATUS_TIMER_START_ERROR;
    }

    __HAL_TIM_CLEAR_FLAG(s_context.timer, TIM_FLAG_CC2);
    __HAL_TIM_ENABLE_DMA(s_context.timer, AD9226_DMA_TIMER_REQUEST);

    return AD9226_STATUS_OK;
}

AD9226_Status AD9226_Stop(void)
{
    if (!s_context.initialized)
    {
        return AD9226_STATUS_NOT_INITIALIZED;
    }

    /* 停止清理期间到达的迟到DMA中断不得再发布为新帧。 */
    s_context.running = false;

    __HAL_TIM_DISABLE_DMA(s_context.timer, AD9226_DMA_TIMER_REQUEST);
    (void)HAL_TIM_OC_Stop(
        s_context.timer,
        AD9226_DMA_TIMER_CHANNEL
    );
    (void)HAL_TIM_PWM_Stop(
        s_context.timer,
        AD9226_CLOCK_TIMER_CHANNEL
    );
    __HAL_TIM_DISABLE(s_context.timer);

    if (s_context.dma != NULL)
    {
        (void)HAL_DMA_Abort(s_context.dma);
    }

    ad9226_drive_clock_pin_low();
    ad9226_configure_data_bus_input();

    return AD9226_STATUS_OK;
}

AD9226_Status AD9226_SetSampleRate(uint32_t requested_hz,
                                  uint32_t *actual_hz)
{
    if (!s_context.initialized)
    {
        return AD9226_STATUS_NOT_INITIALIZED;
    }

    if (s_context.running)
    {
        return AD9226_STATUS_BUSY;
    }

    return ad9226_apply_timer_rate(requested_hz, actual_hz);
}

bool AD9226_AcquireLatestFrame(AD9226_Frame *frame)
{
    if ((frame == NULL) || !s_context.initialized)
    {
        return false;
    }

    uint32_t primask = ad9226_irq_save();

    uint32_t published_sequence = s_context.published_sequence;
    if ((published_sequence == 0U)
        || (published_sequence == s_context.consumed_sequence))
    {
        ad9226_irq_restore(primask);
        return false;
    }

    uint8_t buffer_index = s_context.published_buffer_index;
    uint32_t completion_sys_timer =
        s_context.published_completion_sys_timer;
    s_context.consumed_sequence = published_sequence;

    ad9226_irq_restore(primask);

    frame->samples =
        (buffer_index == 0U) ? s_dma_buffer0 : s_dma_buffer1;
    frame->sample_count = AD9226_FRAME_SAMPLES;
    frame->sequence = published_sequence;
    frame->completion_sys_timer = completion_sys_timer;
    frame->buffer_index = buffer_index;

    return true;
}

uint32_t AD9226_CopyLatestFrame(uint16_t *destination,
                               uint32_t destination_samples,
                               uint32_t *sequence)
{
    if ((destination == NULL)
        || (destination_samples < AD9226_FRAME_SAMPLES))
    {
        return 0U;
    }

    AD9226_Frame frame;
    if (!AD9226_AcquireLatestFrame(&frame))
    {
        return 0U;
    }

    memcpy(
        destination,
        frame.samples,
        AD9226_FRAME_SAMPLES * sizeof(uint16_t)
    );

    for (uint32_t i = 0U; i < AD9226_FRAME_SAMPLES; ++i)
    {
        destination[i] &= AD9226_DATA_MASK;
    }

    if (sequence != NULL)
    {
        *sequence = frame.sequence;
    }

    return AD9226_FRAME_SAMPLES;
}

void AD9226_GetStats(AD9226_Stats *stats)
{
    if (stats == NULL)
    {
        return;
    }

    uint32_t primask = ad9226_irq_save();

    stats->requested_sample_rate_hz =
        s_context.requested_sample_rate_hz;
    stats->actual_sample_rate_hz =
        s_context.actual_sample_rate_hz;
    stats->completed_frames = s_context.completed_frames;
    stats->overwritten_frames = s_context.overwritten_frames;
    stats->notification_overflows = s_context.notification_overflows;
    stats->dma_errors = s_context.dma_errors;
    stats->last_dma_error = s_context.last_dma_error;
    stats->initialized = s_context.initialized;
    stats->running = s_context.running;

    ad9226_irq_restore(primask);
}

bool AD9226_IsRunning(void)
{
    return s_context.running;
}

const char *AD9226_StatusString(AD9226_Status status)
{
    switch (status)
    {
        case AD9226_STATUS_OK:
            return "ok";
        case AD9226_STATUS_INVALID_ARGUMENT:
            return "invalid argument";
        case AD9226_STATUS_NOT_INITIALIZED:
            return "not initialized";
        case AD9226_STATUS_BUSY:
            return "busy";
        case AD9226_STATUS_TIMER_CONFIG_ERROR:
            return "timer config error";
        case AD9226_STATUS_DMA_CONFIG_ERROR:
            return "dma config error";
        case AD9226_STATUS_DMA_START_ERROR:
            return "dma start error";
        case AD9226_STATUS_TIMER_START_ERROR:
            return "timer start error";
        case AD9226_STATUS_NOT_RUNNING:
            return "not running";
        default:
            return "unknown";
    }
}
