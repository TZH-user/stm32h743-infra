#include "cmsis_os2.h"

#include "app_display.h"
#include "app_lvgl.h"
#include "app_touch_gt9xx.h"

/*
 * 本文件覆盖 freertos.c 里 CubeMX 生成的 __weak StartDisplayTask()/StartTouchTask()。
 * 显示启动逻辑留在用户文件里,CubeMX 重新 Generate Code 时不会被冲掉。
 *
 * 模板职责保持最小:点亮屏幕 → 初始化 LVGL → 周期驱动 LVGL 主循环。
 * 业务初始化(算法 worker、通信任务等)加在各自任务文件里,不要堆进本函数。
 */
void StartDisplayTask(void *argument)
{
  (void)argument;

  App_DisplayInit();
  App_DisplayRunPowerOnPattern();
  App_LvglInit();

  /* 5ms 节拍对最小 UI 足够;页面变复杂后按实际负载调整。 */
  for(;;) {
    App_LvglRun();
    osDelay(5);
  }
}

/*
 * 触摸任务:初始化 GT9xx → 等待 EXTI 通知 → 任务上下文里读寄存器 → 更新共享状态,
 * LVGL 的 indev 回调只读共享状态,不做 I2C 访问。
 */
void StartTouchTask(void *argument)
{
  (void)argument;

  App_TouchTaskRun();
}
