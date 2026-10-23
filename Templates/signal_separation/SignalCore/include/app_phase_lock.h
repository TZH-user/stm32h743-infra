#ifndef APP_PHASE_LOCK_H_
#define APP_PHASE_LOCK_H_

#include <stdbool.h>
#include <stdint.h>

#include "signal_vote.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    APP_PHASE_LOCK_ERROR_NONE = 0,
    APP_PHASE_LOCK_ERROR_NOT_READY,
    APP_PHASE_LOCK_ERROR_INVALID_ARGUMENT,
    APP_PHASE_LOCK_ERROR_DAC_START,
    APP_PHASE_LOCK_ERROR_ADC_START,
    APP_PHASE_LOCK_ERROR_CAPTURE_TIMEOUT,
    APP_PHASE_LOCK_ERROR_PREPROCESS,
    APP_PHASE_LOCK_ERROR_MODEL,
    APP_PHASE_LOCK_ERROR_DAC_UPDATE,
    APP_PHASE_LOCK_ERROR_STOP_TIMEOUT
} AppPhaseLockError;

typedef struct
{
    float nominal_frequency_hz;
    float tracked_frequency_hz;
    float cosine_phase_rad;
    float phase_error_rad;
    float amplitude_code;
    SignalWaveType wave_type;
    uint32_t good_frame_streak;
    bool locked;
} AppPhaseLockChannelStatus;

typedef struct
{
    AppPhaseLockChannelStatus channels[SIGNAL_JOINT_COMPONENT_COUNT];
    uint32_t start_count;
    uint32_t stop_count;
    uint32_t captured_frames;
    uint32_t valid_frames;
    uint32_t invalid_frames;
    uint32_t consecutive_invalid_frames;
    uint32_t processing_time_last_us;
    uint32_t processing_time_max_us;
    float residual;
    AppPhaseLockError last_error;
    bool ready;
    bool running;
    bool all_locked;
} AppPhaseLockStatus;

typedef struct
{
    float frequency_hz;
    float cosine_phase_rad;
    uint32_t epoch_sys_timer;
} AppPhaseLockReference;

extern volatile AppPhaseLockStatus g_app_phase_lock_status;

/*@brief:用一次稳定识别结果初始化两个DPLL通道。
 *@param:result          E7稳定识别结果。
 *@param:sys_timer_hz    CMSIS-RTOS系统高分辨率计时频率。
 *@return:输入结果有效时返回 true。
 */
bool AppPhaseLock_Begin(const SignalVoteResult *result,
                        uint32_t sys_timer_hz);

/*@brief:生成下一帧已知模型估计所需的频率和类型种子。 */
void AppPhaseLock_GetSeeds(
    SignalJointComponent seeds[SIGNAL_JOINT_COMPONENT_COUNT]);

/*@brief:根据新一帧联合估计结果更新两个DPLL。
 *@param:result             已知双分量模型结果。
 *@param:sample0_sys_timer  当前帧第一个采样点的系统时基。
 *@param:references         输出两个余弦相位参考。
 *@return:当前帧通过并完成DPLL更新时返回 true。
 */
bool AppPhaseLock_Update(
    const SignalJointResult *result,
    uint32_t sample0_sys_timer,
    AppPhaseLockReference references[SIGNAL_JOINT_COMPONENT_COUNT]);

/*@brief:记录一次采集或模型无效，并按连续失败次数撤销锁定状态。 */
void AppPhaseLock_RecordInvalid(AppPhaseLockError error);

/*@brief:结束当前锁相会话并保留最终诊断快照。 */
void AppPhaseLock_End(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_PHASE_LOCK_H_ */
