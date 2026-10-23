#ifndef APP_RTOS_APP_PHASE_LOCK_TASK_H_
#define APP_RTOS_APP_PHASE_LOCK_TASK_H_

#include <stdbool.h>
#include <stdint.h>

#include "signal_vote.h"

#ifdef __cplusplus
extern "C" {
#endif

/*@brief:CubeMX创建的PhaseLockTask入口，负责锁相采集与DAC补数事件。
 *@param:argument  CubeMX任务参数，当前未使用。
 *@return:无返回值。
 */
void StartPhaseLockTask(void *argument);

/*@brief:把一次稳定识别结果交给PhaseLockTask并异步启动输出。 */
bool AppPhaseLockTask_RequestStart(const SignalVoteResult *result);

/*@brief:请求停止锁相和DAC，并在有界时间内等待任务确认。
 *@param:timeout_ticks  最大等待RTOS tick数。
 *@return:停止完成时返回 true。
 */
bool AppPhaseLockTask_Stop(uint32_t timeout_ticks);

/*@brief:查询PhaseLockTask是否正在持有ADC/DAC输出会话。 */
bool AppPhaseLockTask_IsRunning(void);

/*@brief:设置题目定义的 B' 相对 A' 初相位差。 */
bool AppPhaseLockTask_SetOutputPhaseDifference(uint16_t phase_deg);

/*@brief:读取最近一次接受的输出初相位差。 */
uint16_t AppPhaseLockTask_GetOutputPhaseDifference(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_RTOS_APP_PHASE_LOCK_TASK_H_ */
