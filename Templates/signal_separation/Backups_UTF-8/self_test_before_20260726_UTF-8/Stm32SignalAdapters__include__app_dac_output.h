#ifndef APP_DAC_OUTPUT_H_
#define APP_DAC_OUTPUT_H_

#include <stdbool.h>
#include <stdint.h>

#include "signal_vote.h"
#include "app_dac_output_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_DAC_OUTPUT_CHANNEL_COUNT  2U

#define APP_DAC_OUTPUT_EVENT_HALF     (1UL << 8)
#define APP_DAC_OUTPUT_EVENT_FULL     (1UL << 9)
#define APP_DAC_OUTPUT_EVENT_MASK     \
    (APP_DAC_OUTPUT_EVENT_HALF | APP_DAC_OUTPUT_EVENT_FULL)

typedef enum
{
    APP_DAC_OUTPUT_ERROR_NONE = 0,
    APP_DAC_OUTPUT_ERROR_NOT_INITIALIZED,
    APP_DAC_OUTPUT_ERROR_INVALID_ARGUMENT,
    APP_DAC_OUTPUT_ERROR_UNSUPPORTED_FREQUENCY,
    APP_DAC_OUTPUT_ERROR_UNSUPPORTED_WAVE,
    APP_DAC_OUTPUT_ERROR_HAL_INIT,
    APP_DAC_OUTPUT_ERROR_DMA_START,
    APP_DAC_OUTPUT_ERROR_TIMER_START,
    APP_DAC_OUTPUT_ERROR_DMA_TRANSFER,
    APP_DAC_OUTPUT_ERROR_DMA_UNDERRUN,
    APP_DAC_OUTPUT_ERROR_WORKER_NOT_BOUND,
    APP_DAC_OUTPUT_ERROR_REFILL_OVERRUN
} AppDacOutputError;

/* 直接参数入口；当前默认相位为 0，后续相位发挥项可复用该字段。 */
typedef struct
{
    uint32_t frequency_hz;
    SignalWaveType wave_type;
    float phase_deg;
} AppDacOutputChannelConfig;

typedef struct
{
    AppDacOutputChannelConfig channels[APP_DAC_OUTPUT_CHANNEL_COUNT];
} AppDacOutputConfig;

/* 相位采用DAC波形约定：0 rad 表示正弦/三角波从零点向上。 */
typedef struct
{
    float frequency_hz;
    float wave_phase_rad;
    uint32_t epoch_sys_timer;
} AppDacOutputPhaseReference;

/* GDB 可直接查看的运行快照，不保存历史波表副本。 */
typedef struct
{
    uint32_t frequency_hz[APP_DAC_OUTPUT_CHANNEL_COUNT];
    SignalWaveType wave_type[APP_DAC_OUTPUT_CHANNEL_COUNT];
    uint32_t start_count;
    uint32_t stop_count;
    uint32_t rejected_count;
    uint32_t dma_error_count;
    uint32_t underrun_count;
    uint32_t half_refill_count;
    uint32_t full_refill_count;
    uint32_t refill_overrun_count;
    uint32_t refill_time_last_us;
    uint32_t refill_time_max_us;
    uint32_t worker_signal_error_count;
    uint32_t hal_error_code;
    uint32_t underrun_tick_ms;
    uint32_t underrun_dma_remaining;
    uint32_t underrun_dma_cr;
    uint32_t underrun_dma_fcr;
    uint32_t underrun_dma_lisr;
    uint32_t underrun_dma_error;
    uint32_t underrun_dma_state;
    float tracked_frequency_hz[APP_DAC_OUTPUT_CHANNEL_COUNT];
    uint16_t phase_difference_deg;
    uint8_t harmonic_ratio;
    AppDacOutputError last_error;
    bool initialized;
    bool running;
    bool phase_control_eligible;
    bool phase_control_active;
} AppDacOutputStatus;

extern volatile AppDacOutputStatus g_app_dac_output_status;

bool AppDacOutput_Init(void);

/*@brief:把当前任务绑定为DAC半缓冲补数任务。
 *@return:当前已处于RTOS任务上下文时返回 true。
 */
bool AppDacOutput_BindCurrentTask(void);

bool AppDacOutput_Start(const AppDacOutputConfig *config);
bool AppDacOutput_StartFromVote(const SignalVoteResult *result);

/*@brief:设置 B' 相对 A' 的初相位差，合法范围为 0..180 度、步进 5 度。
 *@param:phase_deg  题目定义的相位差 φ。
 *@return:参数位于合法网格时返回 true。
 */
bool AppDacOutput_SetPhaseDifference(uint16_t phase_deg);

/*@brief:读取最近一次接受的相位差设定。 */
uint16_t AppDacOutput_GetPhaseDifference(void);

/*@brief:更新两个输出分量在同一系统时基下的频率和相位参考。
 *@param:references  两个通道的波形相位参考。
 *@return:参数有效且DAC正在运行时返回 true。
 */
bool AppDacOutput_UpdatePhaseReferences(
    const AppDacOutputPhaseReference references[APP_DAC_OUTPUT_CHANNEL_COUNT]);

/*@brief:处理DMA半传输/全传输事件并补充已经消费的半缓冲。
 *@param:events  当前任务收到的线程标志。
 *@return:本次完成补数的半缓冲数量。
 */
uint32_t AppDacOutput_ServiceEvents(uint32_t events);

void AppDacOutput_Stop(void);
bool AppDacOutput_IsRunning(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DAC_OUTPUT_H_ */
