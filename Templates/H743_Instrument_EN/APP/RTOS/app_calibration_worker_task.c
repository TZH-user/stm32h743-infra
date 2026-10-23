/**
  ******************************************************************************
  * @file    app_calibration_worker_task.c
  * @brief   校准/QSPI 阻塞操作专用 worker 任务实现（UTF-8）。
  ******************************************************************************
  */

#include "app_calibration_worker_task.h"

#include "app_calibration_board_port.h"
#include "app_calibration_service.h"

#include "FreeRTOS.h"
#include "task.h"

/*
 * configTOTAL_HEAP_SIZE 只有 40KiB，现有 6 个 CubeMX 任务栈已占约 27KiB。
 * worker 栈若从 FreeRTOS 堆动态分配会耗尽内核堆（创建失败时校准服务
 * 永不初始化，测试/校准页面全部失效）。因此这里必须使用静态栈与静态
 * 控制块，完全不占用内核堆。
 */
#define APP_CALIBRATION_WORKER_STACK_BYTES  8192U

static osThreadId_t s_worker_handle = NULL;
static StaticTask_t s_worker_control_block;
/* uint64_t 保证 8 字节对齐，满足 Cortex-M7 双字栈对齐要求。 */
static uint64_t s_worker_stack[APP_CALIBRATION_WORKER_STACK_BYTES / 8U];

/*
 * 优先级必须低于 DisplayTask（Normal）：
 * worker 内的 ADC 等待、继电器切换和 QSPI 擦写即使全部超时，
 * 也只会拖慢校准流程本身，不会再抢占 LVGL 渲染与触摸响应。
 */
static const osThreadAttr_t s_worker_attributes = {
    .name = "CalWorkerTask",
    .cb_mem = &s_worker_control_block,
    .cb_size = sizeof(s_worker_control_block),
    .stack_mem = s_worker_stack,
    .stack_size = sizeof(s_worker_stack),
    .priority = (osPriority_t)osPriorityBelowNormal,
};

static void app_calibration_worker_entry(void *argument)
{
    (void)argument;

    /*
     * Port 注册与服务初始化都会访问 QSPI（恢复上电生效配置），
     * 必须与后续 Run 循环同线程，避免与 DisplayTask 竞争存储。
     */
    AppCalibrationBoardPort_Register();
    AppCalibrationService_Init();

    for (;;)
    {
        AppCalibrationService_Run();
        /* 板级内部回环每次只推进一个安全步骤，异常时立即恢复双低继电器。 */
        AppCalibrationBoardPort_Run();
        osDelay(5U);
    }
}

bool AppCalibrationWorkerTask_Start(void)
{
    if (s_worker_handle != NULL)
    {
        return true;
    }

    s_worker_handle = osThreadNew(app_calibration_worker_entry,
                                  NULL,
                                  &s_worker_attributes);
    return s_worker_handle != NULL;
}

osThreadId_t AppCalibrationWorkerTask_GetHandle(void)
{
    return s_worker_handle;
}
