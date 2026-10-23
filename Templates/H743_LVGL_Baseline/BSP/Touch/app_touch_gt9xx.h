#ifndef APP_TOUCH_GT9XX_H
#define APP_TOUCH_GT9XX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint32_t read_error_count;        /* I2C 读取失败累计 */
  uint32_t consecutive_error_count; /* 当前连续失败次数 */
  uint32_t stale_release_count;     /* pressed 超时强制释放累计 */
  uint32_t reinit_count;            /* 连续失败触发的重新初始化累计 */
  uint32_t last_valid_tick;         /* 最近一次有效数据帧的内核 tick */
} App_TouchDiagnostics;

bool App_TouchInit(void);
/* 任意任务调用：复制触摸驱动运行诊断计数。 */
void App_TouchGetDiagnostics(App_TouchDiagnostics *diagnostics);
/* 兼容旧调用路径：主动轮询一次 GT9xx 并更新缓存触摸状态。 */
void App_TouchPoll(void);
/* LVGL indev 读取入口：返回最近一次采样缓存，不在这里做 IIC 通信。 */
bool App_TouchGetPoint(uint16_t *x, uint16_t *y);
uint8_t App_TouchGetPointCount(void);
/* EXTI ISR 调用：仅做任务通知，避免在中断里执行位操作 IIC。 */
void App_TouchNotifyFromISR(uint16_t gpio_pin);
/* TouchTask 主循环：等待中断通知后在任务上下文读取 GT9xx。 */
void App_TouchTaskRun(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TOUCH_GT9XX_H */
