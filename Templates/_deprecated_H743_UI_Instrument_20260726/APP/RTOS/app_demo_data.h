/**
  ******************************************************************************
  * @file    app_demo_data.h
  * @brief   演示数据帧的发布/读取接口(任务间 mutex 双缓冲,UTF-8)。
  *
  * 数据流模式示范:生产任务 Publish -> 本模块缓冲 -> DisplayTask TryFetch。
  * 接入真实采集时保持本接口不变,替换生产端即可(见 README)。
  ******************************************************************************
  */

#ifndef APP_DEMO_DATA_H
#define APP_DEMO_DATA_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_DEMO_DATA_POINTS  256U

typedef struct
{
    uint32_t seq;                             /* 帧序号,每 Publish 递增 */
    int32_t  points[APP_DEMO_DATA_POINTS];    /* 波形点,int32 直接对接 lv_chart ext array */
    float    freq_hz;                         /* 标称频率(演示值) */
    float    vpp;                             /* 标称峰峰值(演示值) */
} AppDemoFrame;

/* 生产任务启动时调用一次;重复调用无害。 */
void AppDemoData_Init(void);

/* 生产端:发布一帧(内部加锁拷贝,不持有指针)。未 Init 时安全返回 false。 */
bool AppDemoData_Publish(const AppDemoFrame *frame);

/* 消费端:取最新帧;无新帧(seq 未变化)或未 Init 时返回 false。 */
bool AppDemoData_TryFetch(AppDemoFrame *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_DEMO_DATA_H */
