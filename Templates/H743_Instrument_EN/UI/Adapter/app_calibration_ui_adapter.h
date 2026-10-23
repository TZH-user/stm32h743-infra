/**
  ******************************************************************************
  * @file    app_calibration_ui_adapter.h
  * @brief   校准全页与校准服务的 LVGL 适配接口（UTF-8）。
  ******************************************************************************
  */

#ifndef UI_ADAPTER_APP_CALIBRATION_UI_ADAPTER_H_
#define UI_ADAPTER_APP_CALIBRATION_UI_ADAPTER_H_

#ifdef __cplusplus
extern "C" {
#endif

/*@brief:检测活动页面、绑定校准控件并刷新服务快照；仅由 DisplayTask 调用。 */
void AppCalibrationUiAdapter_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_ADAPTER_APP_CALIBRATION_UI_ADAPTER_H_ */
