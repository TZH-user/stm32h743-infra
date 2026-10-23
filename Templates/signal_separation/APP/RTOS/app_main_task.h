#ifndef APP_RTOS_APP_MAIN_TASK_H_
#define APP_RTOS_APP_MAIN_TASK_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*@brief:CubeMX 创建的 MainTask 入口，只负责单次采集与 FFT 流程调度。
 *@param:argument  CubeMX 任务参数，当前未使用。
 *@return:无返回值。
 */
void StartMainTask(void *argument);

/*@brief:UI 的 RUN 键请求一次八帧识别；运行中重复请求会被拒绝。 */
bool AppMain_RequestStart(void);

/*@brief:UI 的 STOP 键请求终止当前识别；空闲时请求会被忽略。 */
bool AppMain_RequestStop(void);

/*@brief:查询 MainTask 是否正在运行或已有待执行的 RUN 请求。 */
bool AppMain_IsRunning(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_RTOS_APP_MAIN_TASK_H_ */
