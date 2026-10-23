/**
  ******************************************************************************
  * @file    app_ui_adapter.h
  * @brief   单帧 ADC、E7 正式结果与信号分离装置 UI 适配接口（UTF-8）。
  ******************************************************************************
  */

#ifndef UI_ADAPTER_APP_UI_ADAPTER_H_
#define UI_ADAPTER_APP_UI_ADAPTER_H_

#include "lvgl.h"
#include "signal_dual_sine.h"
#include "signal_fft.h"
#include "signal_harmonic.h"
#include "signal_joint_model.h"
#include "signal_vote.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*@brief:绑定示波器页面；只能从 LVGL DisplayTask 调用。 */
bool AppUiAdapter_Init(lv_obj_t *monitor_screen);

/*@brief:周期刷新活动页面；只能从 LVGL DisplayTask 调用。 */
void AppUiAdapter_Run(void);

/*@brief:页面刚切换时立即重绑定控件；只能从 LVGL DisplayTask 调用。 */
void AppUiAdapter_NotifyPageChanged(void);

/*@brief:提交一帧 ADC 数据和同帧正式 FFT 结果；不调用 LVGL。 */
bool AppUiAdapter_SubmitAdcFrame(const uint16_t *samples,
                                 uint32_t sample_count,
                                 uint32_t sample_rate_hz,
                                 const SignalFftResult *fft_result,
                                 const SignalDualSineResult *dual_result,
                                 const SignalHarmonicResult *harmonic_result,
                                 const SignalJointResult *joint_result,
                                 const SignalVoteResult *vote_result);

#ifdef __cplusplus
}
#endif

#endif /* UI_ADAPTER_APP_UI_ADAPTER_H_ */
