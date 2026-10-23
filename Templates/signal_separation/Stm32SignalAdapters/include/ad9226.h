/**
  ******************************************************************************
  * @file    ad9226.h
  * @brief   AD9226 parallel ADC acquisition driver for STM32H743.
  ******************************************************************************
  */

#ifndef BSP_ADC_AD9226_H_
#define BSP_ADC_AD9226_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os2.h"
#include "stm32h7xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

#include "ad9226_config.h"

#define AD9226_SAMPLE_MASK  0x0FFFU

static inline uint16_t AD9226_RawToCode(uint16_t raw_sample)
{
    return raw_sample & AD9226_SAMPLE_MASK;
}

typedef enum
{
    AD9226_STATUS_OK = 0,
    AD9226_STATUS_INVALID_ARGUMENT,
    AD9226_STATUS_NOT_INITIALIZED,
    AD9226_STATUS_BUSY,
    AD9226_STATUS_TIMER_CONFIG_ERROR,
    AD9226_STATUS_DMA_CONFIG_ERROR,
    AD9226_STATUS_DMA_START_ERROR,
    AD9226_STATUS_TIMER_START_ERROR,
    AD9226_STATUS_NOT_RUNNING
} AD9226_Status;

typedef struct
{
    TIM_HandleTypeDef *timer;
    DMA_HandleTypeDef *dma;
    osSemaphoreId_t frame_ready_semaphore;
    uint32_t timer_kernel_clock_hz;
    uint32_t initial_sample_rate_hz;
} AD9226_Config;

typedef struct
{
    const uint16_t *samples;
    uint32_t sample_count;
    uint32_t sequence;
    uint32_t completion_sys_timer;
    uint8_t buffer_index;
} AD9226_Frame;

typedef struct
{
    uint32_t requested_sample_rate_hz;
    uint32_t actual_sample_rate_hz;
    uint32_t completed_frames;
    uint32_t overwritten_frames;
    uint32_t notification_overflows;
    uint32_t dma_errors;
    uint32_t last_dma_error;
    bool initialized;
    bool running;
} AD9226_Stats;

/**
 * @brief Bind the CubeMX TIM2/DMA handles and prepare the driver.
 * @note  Call from task context after the RTOS semaphore has been created.
 */
AD9226_Status AD9226_Init(const AD9226_Config *config);

/**
 * @brief Start continuous timer-clocked, DMA double-buffer acquisition.
 *
 * Start sequence:
 * 1. keep PB0..PB11 configured as ADC data inputs;
 * 2. start TIM2_CH1 sampling clock;
 * 3. prime the seven-cycle AD9226 pipeline;
 * 4. enable TIM2_CH2-triggered DMA reads from GPIOB->IDR.
 */
AD9226_Status AD9226_Start(void);

/**
 * @brief Stop acquisition, drive PA0 low and keep PB3 as ADC input.
 */
AD9226_Status AD9226_Stop(void);

/**
 * @brief Change the sampling rate while stopped.
 * @param requested_hz Requested ADC clock/sample rate in hertz.
 * @param actual_hz Optional returned realizable timer rate.
 */
AD9226_Status AD9226_SetSampleRate(uint32_t requested_hz,
                                  uint32_t *actual_hz);

/**
 * @brief Fetch the newest completed DMA frame once.
 *
 * The returned pointer refers directly to one DMA ping-pong buffer. Consume or
 * copy it promptly: at continuous rates that buffer can be reused after the
 * other buffer has completed.
 */
bool AD9226_AcquireLatestFrame(AD9226_Frame *frame);

/**
 * @brief Copy the newest completed frame into application-owned memory.
 * @return Number of copied samples, or zero when no new frame is available.
 */
uint32_t AD9226_CopyLatestFrame(uint16_t *destination,
                               uint32_t destination_samples,
                               uint32_t *sequence);

/** @brief Return a snapshot of driver counters and current state. */
void AD9226_GetStats(AD9226_Stats *stats);

/** @brief Return true while timer and DMA acquisition are active. */
bool AD9226_IsRunning(void);

/** @brief Return a readable status string for bring-up logging. */
const char *AD9226_StatusString(AD9226_Status status);

#ifdef __cplusplus
}
#endif

#endif /* BSP_ADC_AD9226_H_ */
