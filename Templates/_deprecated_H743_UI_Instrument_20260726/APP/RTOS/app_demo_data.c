#include "app_demo_data.h"

#include "cmsis_os2.h"

#include <string.h>

/*
 * mutex 保护的单槽缓冲:生产端整帧拷入,消费端整帧拷出。
 * 帧率低(演示 12.5 Hz)、帧小(1KB 量级),拷贝成本可忽略;
 * 高吞吐场景请换 osMessageQueue 或双缓冲+信号量(参考 signal_separation 的 AdcFrameReadySem 模式)。
 */
static osMutexId_t s_lock;
static AppDemoFrame s_latest;
static uint32_t s_last_fetched_seq;

void AppDemoData_Init(void)
{
    if (s_lock == NULL)
    {
        s_lock = osMutexNew(NULL);
    }
}

bool AppDemoData_Publish(const AppDemoFrame *frame)
{
    if ((s_lock == NULL) || (frame == NULL))
    {
        return false;
    }
    if (osMutexAcquire(s_lock, 10U) != osOK)
    {
        return false;
    }
    memcpy(&s_latest, frame, sizeof(s_latest));
    (void)osMutexRelease(s_lock);
    return true;
}

bool AppDemoData_TryFetch(AppDemoFrame *out)
{
    bool fresh = false;

    if ((s_lock == NULL) || (out == NULL))
    {
        return false;
    }
    if (osMutexAcquire(s_lock, 0U) != osOK)
    {
        return false;   /* 生产端正在写,下个循环再取 */
    }
    if ((s_latest.seq != 0U) && (s_latest.seq != s_last_fetched_seq))
    {
        memcpy(out, &s_latest, sizeof(s_latest));
        s_last_fetched_seq = s_latest.seq;
        fresh = true;
    }
    (void)osMutexRelease(s_lock);
    return fresh;
}
