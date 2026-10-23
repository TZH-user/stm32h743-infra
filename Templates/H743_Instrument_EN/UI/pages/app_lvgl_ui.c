#include "app_lvgl_ui.h"

#include "app_calibration_ui_adapter.h"
#include "app_instrument_screen.h"
#include "app_ui_adapter.h"
#include "signal_separation_ui.h"

void App_LvglUiInit(void)
{
  signal_separation_ui_set_target(
      SIGNAL_SEPARATION_UI_TARGET_H743_RGB_800X480);
  signal_separation_ui_init(NULL);

  /*
   * Designer 保留可视化页面源；运行时 Monitor 使用非生成实现承载动态数据。
   * 该替换位于应用层，后续 Generate Code 不会覆盖。
   */
  if (screen_monitor != NULL)
  {
    lv_obj_delete(screen_monitor);
    screen_monitor = NULL;
  }
  screen_monitor = AppInstrumentScreen_Create();

  if (screen_monitor != NULL)
  {
    (void)AppUiAdapter_Init(screen_monitor);
    lv_screen_load(screen_monitor);
  }
}

void App_LvglUiRun(void)
{
  AppUiAdapter_Run();
  /*
   * P1-07 整改：校准/测试页逻辑独立驱动，不再嵌在 Monitor Adapter
   * 内部。即使 Monitor 模板绑定失败或对象名变更，Test/Calibration
   * 页面的按钮回调与状态刷新也不受影响。
   */
  AppCalibrationUiAdapter_Run();
}
