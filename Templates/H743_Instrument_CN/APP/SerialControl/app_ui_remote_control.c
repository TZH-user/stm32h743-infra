#include "app_ui_remote_control.h"

#include "app_calibration_ui_adapter.h"
#include "app_calibration_service.h"
#include "app_main_task.h"
#include "app_output_task.h"
#include "app_phase_lock_task.h"
#include "app_ui_adapter.h"
#include "cmsis_os2.h"
#include "lvgl.h"
#include "signal_separation_ui.h"
#include "stm32h7xx_hal.h"

#include <stddef.h>
#include <string.h>

#define APP_UI_REMOTE_QUEUE_DEPTH       8U
#define APP_UI_REMOTE_MAX_PER_CYCLE     4U

static osMessageQueueId_t s_ui_command_queue = NULL;
static volatile AppUiRemoteStats s_ui_stats;

static lv_obj_t *app_ui_remote_find_named(lv_obj_t *parent,
                                          const char *name)
{
    if ((parent == NULL) || (name == NULL))
    {
        return NULL;
    }

    const char *object_name = lv_obj_get_name(parent);
    if ((object_name != NULL) && (strcmp(object_name, name) == 0))
    {
        return parent;
    }

    uint32_t child_count = lv_obj_get_child_count(parent);
    for (uint32_t index = 0U; index < child_count; ++index)
    {
        lv_obj_t *result = app_ui_remote_find_named(
            lv_obj_get_child(parent, (int32_t)index), name);
        if (result != NULL)
        {
            return result;
        }
    }

    return NULL;
}

static bool app_ui_remote_click_named(const char *name)
{
    lv_obj_t *target = app_ui_remote_find_named(lv_screen_active(), name);
    if ((target == NULL) || lv_obj_has_state(target, LV_STATE_DISABLED))
    {
        return false;
    }

    return lv_obj_send_event(target, LV_EVENT_CLICKED, NULL) == LV_RESULT_OK;
}

/*
 * P1-05 整改：与触摸导航（lv_obj_add_screen_create_event）行为对齐，
 * 串口创建的动态页面在被卸载时自动删除，避免反复 PAGE 命令泄漏
 * LVGL heap 并最终触发 malloc 断言死循环。
 */
static void app_ui_remote_screen_unloaded(lv_event_t *event)
{
    lv_obj_delete(lv_event_get_target_obj(event));
}

static bool app_ui_remote_load_page(AppUiRemoteAction action)
{
    lv_obj_t *screen = NULL;
    bool dynamic_screen = true;

    switch (action)
    {
        case APP_UI_REMOTE_ACTION_PAGE_MONITOR:
            screen = screen_monitor;
            dynamic_screen = false;
            break;

        case APP_UI_REMOTE_ACTION_PAGE_SPECTRUM:
            screen = screen_spectrum_create();
            break;

        case APP_UI_REMOTE_ACTION_PAGE_TEST:
            screen = screen_test_create();
            break;

        case APP_UI_REMOTE_ACTION_PAGE_CALIBRATION:
            screen = screen_calibration_create();
            break;

        case APP_UI_REMOTE_ACTION_PAGE_SYSTEM:
            screen = screen_system_create();
            break;

        default:
            return false;
    }

    if (screen == NULL)
    {
        return false;
    }

    if (dynamic_screen)
    {
        lv_obj_add_event_cb(screen,
                            app_ui_remote_screen_unloaded,
                            LV_EVENT_SCREEN_UNLOADED,
                            NULL);
    }
    lv_screen_load(screen);

    /*
     * P1-04 整改：页面加载后立即驱动两个 Adapter 完成控件绑定，
     * 使同一命令批次内紧随其后的 TAP 能命中已注册回调的按钮。
     */
    AppUiAdapter_NotifyPageChanged();
    AppCalibrationUiAdapter_Run();
    return true;
}

static bool app_ui_remote_execute(AppUiRemoteAction action)
{
    switch (action)
    {
        case APP_UI_REMOTE_ACTION_RUN:
            return app_ui_remote_click_named("ui_start_button");

        case APP_UI_REMOTE_ACTION_STOP:
            return app_ui_remote_click_named("ui_stop_button");

        case APP_UI_REMOTE_ACTION_PHASE_DECREASE:
            return app_ui_remote_click_named("ui_phase_decrease");

        case APP_UI_REMOTE_ACTION_PHASE_INCREASE:
            return app_ui_remote_click_named("ui_phase_increase");

        case APP_UI_REMOTE_ACTION_CALIBRATION_START:
            return app_ui_remote_click_named("test_start_button")
                || app_ui_remote_click_named("cal_start_button");

        case APP_UI_REMOTE_ACTION_CALIBRATION_STOP:
            return app_ui_remote_click_named("test_stop_button")
                || app_ui_remote_click_named("cal_stop_button");

        case APP_UI_REMOTE_ACTION_PAGE_MONITOR:
        case APP_UI_REMOTE_ACTION_PAGE_SPECTRUM:
        case APP_UI_REMOTE_ACTION_PAGE_TEST:
        case APP_UI_REMOTE_ACTION_PAGE_CALIBRATION:
        case APP_UI_REMOTE_ACTION_PAGE_SYSTEM:
            return app_ui_remote_load_page(action);

        default:
            return false;
    }
}

/*
 * P2-05 整改（最小闭环）：LVGL 事件派发成功不代表业务受理。
 * TAP 类动作执行后追加相关业务快照，供上位机核对真实受理结果。
 */
static void app_ui_remote_report_business(AppUiRemoteAction action)
{
    switch (action)
    {
        case APP_UI_REMOTE_ACTION_RUN:
        case APP_UI_REMOTE_ACTION_STOP:
            (void)AppOutput_Logf(
                "[CTRL] MAIN running=%u tracking=%u\r\n",
                (unsigned int)(AppMain_IsRunning() ? 1U : 0U),
                (unsigned int)(AppPhaseLockTask_IsRunning() ? 1U : 0U));
            break;

        case APP_UI_REMOTE_ACTION_PHASE_DECREASE:
        case APP_UI_REMOTE_ACTION_PHASE_INCREASE:
            (void)AppOutput_Logf(
                "[CTRL] PHASE deg=%u\r\n",
                (unsigned int)AppPhaseLockTask_GetOutputPhaseDifference());
            break;

        case APP_UI_REMOTE_ACTION_CALIBRATION_START:
        case APP_UI_REMOTE_ACTION_CALIBRATION_STOP:
        {
            AppCalibrationSnapshot snapshot;
            AppCalibrationService_GetSnapshot(&snapshot);
            (void)AppOutput_Logf(
                "[CTRL] CAL state=%u error=%u can_start=%u\r\n",
                (unsigned int)snapshot.state,
                (unsigned int)snapshot.error,
                (unsigned int)(snapshot.can_start ? 1U : 0U));
            break;
        }

        default:
            break;
    }
}

bool AppUiRemoteControl_Init(void)
{
    if (s_ui_command_queue != NULL)
    {
        return true;
    }

    static const osMessageQueueAttr_t attributes = {
        .name = "UiControlQueue"
    };

    s_ui_command_queue = osMessageQueueNew(
        APP_UI_REMOTE_QUEUE_DEPTH,
        sizeof(AppUiRemoteAction),
        &attributes);
    return s_ui_command_queue != NULL;
}

bool AppUiRemoteControl_Post(AppUiRemoteAction action)
{
    if (s_ui_command_queue == NULL)
    {
        ++s_ui_stats.dropped_count;
        return false;
    }

    if (osMessageQueuePut(s_ui_command_queue, &action, 0U, 0U) != osOK)
    {
        ++s_ui_stats.dropped_count;
        return false;
    }

    ++s_ui_stats.queued_count;
    return true;
}

void AppUiRemoteControl_Run(void)
{
    if (s_ui_command_queue == NULL)
    {
        return;
    }

    for (uint32_t count = 0U;
         count < APP_UI_REMOTE_MAX_PER_CYCLE;
         ++count)
    {
        AppUiRemoteAction action;
        if (osMessageQueueGet(s_ui_command_queue,
                              &action,
                              NULL,
                              0U) != osOK)
        {
            break;
        }

        bool result = app_ui_remote_execute(action);
        s_ui_stats.last_action = action;
        s_ui_stats.last_result = result;
        if (result)
        {
            ++s_ui_stats.executed_count;
        }
        else
        {
            ++s_ui_stats.rejected_count;
        }
        __DMB();

        (void)AppOutput_Logf("[CTRL] %s UI %s\r\n",
                            result ? "OK" : "ERR",
                            AppUiRemoteControl_ActionName(action));
        app_ui_remote_report_business(action);
    }
}

void AppUiRemoteControl_GetStats(AppUiRemoteStats *stats)
{
    if (stats == NULL)
    {
        return;
    }

    __DMB();
    stats->queued_count = s_ui_stats.queued_count;
    stats->executed_count = s_ui_stats.executed_count;
    stats->rejected_count = s_ui_stats.rejected_count;
    stats->dropped_count = s_ui_stats.dropped_count;
    stats->last_action = s_ui_stats.last_action;
    stats->last_result = s_ui_stats.last_result;
}

const char *AppUiRemoteControl_ActionName(AppUiRemoteAction action)
{
    switch (action)
    {
        case APP_UI_REMOTE_ACTION_RUN:
            return "TAP RUN";
        case APP_UI_REMOTE_ACTION_STOP:
            return "TAP STOP";
        case APP_UI_REMOTE_ACTION_PHASE_DECREASE:
            return "TAP PHASE-";
        case APP_UI_REMOTE_ACTION_PHASE_INCREASE:
            return "TAP PHASE+";
        case APP_UI_REMOTE_ACTION_CALIBRATION_START:
            return "TAP CALSTART";
        case APP_UI_REMOTE_ACTION_CALIBRATION_STOP:
            return "TAP CALSTOP";
        case APP_UI_REMOTE_ACTION_PAGE_MONITOR:
            return "PAGE MONITOR";
        case APP_UI_REMOTE_ACTION_PAGE_SPECTRUM:
            return "PAGE SPECTRUM";
        case APP_UI_REMOTE_ACTION_PAGE_TEST:
            return "PAGE TEST";
        case APP_UI_REMOTE_ACTION_PAGE_CALIBRATION:
            return "PAGE CALIBRATION";
        case APP_UI_REMOTE_ACTION_PAGE_SYSTEM:
            return "PAGE SYSTEM";
        default:
            return "UNKNOWN";
    }
}
