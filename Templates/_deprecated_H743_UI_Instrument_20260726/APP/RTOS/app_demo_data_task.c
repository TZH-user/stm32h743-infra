#include "cmsis_os2.h"

#include "app_demo_data.h"

#include <math.h>

/*
 * 覆盖 freertos.c 的 __weak StartMainTask():演示波形发生器。
 * 生成"频率扫动 + 幅度呼吸 + 白噪声"的正弦帧,12.5 Hz 帧率发布。
 *
 * 接真实数据:删掉本文件,用自己的采集任务在数据就绪处调用
 * AppDemoData_Publish()(或直接换成工程自己的数据通道)。
 */

#define DEMO_SAMPLE_RATE_HZ  1000000.0f   /* 标称采样率,仅用于频率标签换算 */
#define DEMO_FRAME_PERIOD_MS 80U

static uint32_t s_lcg_state = 0x12345678U;

static int32_t demo_noise(void)
{
    /* 线性同余伪随机,可重入无库依赖;输出约 ±40。 */
    s_lcg_state = s_lcg_state * 1664525U + 1013904223U;
    return (int32_t)(s_lcg_state >> 24) % 81 - 40;
}

void StartMainTask(void *argument)
{
    (void)argument;

    AppDemoFrame frame = {0};
    float t = 0.0f;

    AppDemoData_Init();

    for (;;)
    {
        /* 周期数在 2~8 之间扫动,幅度在 400~1000 之间呼吸。 */
        float cycles = 5.0f + 3.0f * sinf(t * 0.05f);
        float amp    = 700.0f + 300.0f * sinf(t * 0.13f);

        for (uint32_t i = 0U; i < APP_DEMO_DATA_POINTS; i++)
        {
            float phase = 6.2831853f * cycles * (float)i / (float)APP_DEMO_DATA_POINTS;
            frame.points[i] = (int32_t)(amp * sinf(phase)) + demo_noise();
        }
        frame.seq++;
        frame.freq_hz = cycles * (DEMO_SAMPLE_RATE_HZ / (float)APP_DEMO_DATA_POINTS);
        frame.vpp     = 2.0f * amp;

        (void)AppDemoData_Publish(&frame);

        t += 1.0f;
        osDelay(DEMO_FRAME_PERIOD_MS);
    }
}
