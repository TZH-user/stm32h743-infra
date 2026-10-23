#include "cmsis_os2.h"

#include "app_display.h"
#include "app_calibration_worker_task.h"
#include "app_lvgl.h"
#include "app_touch_gt9xx.h"
#include "app_ui_monitor.h"
#include "app_ui_remote_control.h"

/*
 * 这个文件用于覆盖 freertos.c 里 CubeMX 生成的 __weak StartDisplayTask()。
 * 这样显示启动逻辑就能留在用户文件里，后续再次 Generate Code 时更稳。
 */
void StartDisplayTask(void *argument)
{
  (void)argument;

  /*
   * P0-01 整改后任务职责收敛为：
   * 1. 完成最小显示 bring-up（背光、可选彩条）
   * 2. 初始化 LVGL
   * 3. 周期性驱动 LVGL 主循环与串口 UI 命令
   * 校准状态机、板级回环和 QSPI 存储全部由 CalWorkerTask 执行，
   * 本任务内不允许出现任何阻塞式外设或存储访问。
   */
  App_DisplayInit();
  App_DisplayRunPowerOnPattern();
  App_LvglInit();
  (void)AppCalibrationWorkerTask_Start();

  /*
   * 这里使用固定 5ms 的任务节拍即可满足当前最小 UI。
   * 后面如果引入更复杂页面、动画或触摸输入，再根据实际负载调整。
   */
  for(;;) {
    /* UI 监视：记录心跳、循环耗时并周期采样 LVGL heap。 */
    AppUiMonitor_LoopBegin();
    /* 串口控制命令必须在 DisplayTask 中转换成 LVGL 事件。 */
    AppUiRemoteControl_Run();
    /* LVGL 主循环保持轻量，避免在该任务里做阻塞式外设读写。 */
    App_LvglRun();
    AppUiMonitor_LoopEnd();
    osDelay(5);
  }
}

/*
 * Dedicated touch task:
 * 1. wait for EXTI notification from ISR
 * 2. perform software I2C register read in task context
 * 3. update shared touch state for LVGL indev read callback
 */
void StartTouchTask(void *argument)
{
  (void)argument;

  /* 触摸任务内部完成：初始化 -> 等待 EXTI -> 读取 GT9xx -> 更新共享状态。 */
  App_TouchTaskRun();
}
