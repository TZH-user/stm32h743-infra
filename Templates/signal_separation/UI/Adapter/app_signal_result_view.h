/**
  ******************************************************************************
  * @file    app_signal_result_view.h
  * @brief   E7 两路识别结果与错误诊断显示接口（UTF-8）。
  ******************************************************************************
  */

#ifndef UI_ADAPTER_APP_SIGNAL_RESULT_VIEW_H_
#define UI_ADAPTER_APP_SIGNAL_RESULT_VIEW_H_

#include "lvgl.h"
#include "signal_joint_model.h"
#include "signal_vote.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool vote_present;
    bool vote_valid;
    SignalDetectError vote_error;
    SignalWaveType wave_types[SIGNAL_JOINT_COMPONENT_COUNT];
    uint32_t component_frequency_hz[SIGNAL_JOINT_COMPONENT_COUNT];
    uint32_t component_amplitude_q100[SIGNAL_JOINT_COMPONENT_COUNT];
    uint32_t component_frequency_votes[SIGNAL_JOINT_COMPONENT_COUNT];
    uint32_t component_type_votes[SIGNAL_JOINT_COMPONENT_COUNT];
    uint32_t requested_frames;
    uint32_t valid_frames;
    uint32_t reason_mask;
    uint32_t residual_q100000;
    uint32_t confidence_q1000;
    uint32_t session_time_ms;
    uint32_t candidate_count;
    uint32_t evaluated_pairs;
    uint32_t evaluated_models;
} AppSignalResultViewData;

/*@brief:绑定当前屏幕中的结果控件；只能从 DisplayTask 调用。 */
void AppSignalResultView_Bind(lv_obj_t *screen);

/*@brief:刷新结果卡片和错误说明；只能从 DisplayTask 调用。 */
void AppSignalResultView_Render(const AppSignalResultViewData *data);

/*@brief:在锁相输出期间刷新两路跟踪频率、相位误差和锁定状态。 */
void AppSignalResultView_RefreshPhaseLock(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_ADAPTER_APP_SIGNAL_RESULT_VIEW_H_ */
