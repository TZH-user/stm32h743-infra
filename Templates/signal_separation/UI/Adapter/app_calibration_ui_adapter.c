/**
  ******************************************************************************
  * @file    app_calibration_ui_adapter.c
  * @brief   校准全页与独占服务的 LVGL 适配实现（UTF-8）。
  ******************************************************************************
  */

#include "app_calibration_ui_adapter.h"

#include "app_calibration_board_port.h"
#include "app_calibration_service.h"
#include "app_output_task.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef enum
{
    APP_CALIBRATION_UI_PAGE_NONE = 0,
    APP_CALIBRATION_UI_PAGE_TEST,
    APP_CALIBRATION_UI_PAGE_CALIBRATION
} AppCalibrationUiPage;

/* 页面绑定每个 LVGL 循环都检测；快照渲染按该周期节流即可。 */
#define APP_CALIBRATION_UI_RENDER_PERIOD_MS  50U

static lv_obj_t *s_screen = NULL;
static AppCalibrationUiPage s_page = APP_CALIBRATION_UI_PAGE_NONE;
static bool s_reenter_pending = false;
static uint32_t s_last_render_tick = 0U;
static lv_obj_t *s_internal_button = NULL;
static lv_obj_t *s_external_button = NULL;
static lv_obj_t *s_source_dropdown = NULL;
static lv_obj_t *s_item_dropdown = NULL;
static lv_obj_t *s_step_label = NULL;
static lv_obj_t *s_error_label = NULL;
static lv_obj_t *s_points_label = NULL;
static lv_obj_t *s_storage_label = NULL;
static lv_obj_t *s_profile_label = NULL;
static lv_obj_t *s_exclusive_label = NULL;
static lv_obj_t *s_safe_label = NULL;
static lv_obj_t *s_progress_bar = NULL;
static lv_obj_t *s_start_button = NULL;
static lv_obj_t *s_stop_button = NULL;
static lv_obj_t *s_export_button = NULL;
static lv_obj_t *s_import_button = NULL;
static lv_obj_t *s_confirm_button = NULL;
static lv_obj_t *s_cancel_button = NULL;
static lv_obj_t *s_apply_button = NULL;
static lv_obj_t *s_apply_label = NULL;
static lv_obj_t *s_adc_toggle_button = NULL;
static lv_obj_t *s_adc_toggle_label = NULL;
static lv_obj_t *s_adc_chart = NULL;
static lv_chart_series_t *s_adc_chart_series = NULL;
static lv_obj_t *s_adc_rate_label = NULL;
static lv_obj_t *s_adc_points_label = NULL;
static lv_obj_t *s_adc_span_label = NULL;
static lv_obj_t *s_adc_x_mid_label = NULL;
static lv_obj_t *s_adc_x_end_label = NULL;
static uint32_t s_last_adc_preview_sequence = 0U;
static bool s_adc_preview_wait_for_new = false;
static lv_obj_t *s_self_test_output[CALIBRATION_SELF_TEST_LEVEL_COUNT];
static lv_obj_t *s_self_test_amplitude[CALIBRATION_SELF_TEST_LEVEL_COUNT];
static lv_obj_t *s_self_test_noise[CALIBRATION_SELF_TEST_LEVEL_COUNT];
static lv_obj_t *s_self_test_result[CALIBRATION_SELF_TEST_LEVEL_COUNT];
static AppCalibrationMode s_mode = APP_CALIBRATION_MODE_INTERNAL;
static AppCalibrationSource s_source = APP_CALIBRATION_SOURCE_DAC1_TO_ADC;
static uint32_t s_request_reject_count = 0U;
static uint32_t s_bind_missing_count = 0U;

static const char *s_self_test_output_names[CALIBRATION_SELF_TEST_LEVEL_COUNT] = {
    "test_self_out_dc_q", "test_self_out_dc_h", "test_self_out_dc_f",
    "test_self_out_ac_q", "test_self_out_ac_h", "test_self_out_ac_f",
};
static const char *s_self_test_amplitude_names[CALIBRATION_SELF_TEST_LEVEL_COUNT] = {
    "test_self_amp_dc_q", "test_self_amp_dc_h", "test_self_amp_dc_f",
    "test_self_amp_ac_q", "test_self_amp_ac_h", "test_self_amp_ac_f",
};
static const char *s_self_test_noise_names[CALIBRATION_SELF_TEST_LEVEL_COUNT] = {
    "test_self_noise_dc_q", "test_self_noise_dc_h", "test_self_noise_dc_f",
    "test_self_noise_ac_q", "test_self_noise_ac_h", "test_self_noise_ac_f",
};
static const char *s_self_test_result_names[CALIBRATION_SELF_TEST_LEVEL_COUNT] = {
    "test_self_result_dc_q", "test_self_result_dc_h", "test_self_result_dc_f",
    "test_self_result_ac_q", "test_self_result_ac_h", "test_self_result_ac_f",
};

static AppCalibrationUiPage app_calibration_ui_page(lv_obj_t *screen)
{
    const char *name = (screen != NULL) ? lv_obj_get_name(screen) : NULL;
    if (name == NULL)
    {
        return APP_CALIBRATION_UI_PAGE_NONE;
    }
    if (strncmp(name, "screen_test", 11U) == 0)
    {
        return APP_CALIBRATION_UI_PAGE_TEST;
    }
    if (strncmp(name, "screen_calibration", 18U) == 0)
    {
        return APP_CALIBRATION_UI_PAGE_CALIBRATION;
    }
    return APP_CALIBRATION_UI_PAGE_NONE;
}

static lv_obj_t *app_calibration_ui_find_label(lv_obj_t *parent,
                                                const char *text)
{
    if ((parent == NULL) || (text == NULL))
    {
        return NULL;
    }

    if (lv_obj_check_type(parent, &lv_label_class)
        && (strcmp(lv_label_get_text(parent), text) == 0))
    {
        return parent;
    }

    uint32_t child_count = lv_obj_get_child_count(parent);
    for (uint32_t index = 0U; index < child_count; ++index)
    {
        lv_obj_t *result = app_calibration_ui_find_label(
            lv_obj_get_child(parent, (int32_t)index), text);
        if (result != NULL)
        {
            return result;
        }
    }
    return NULL;
}

static lv_obj_t *app_calibration_ui_find_named(lv_obj_t *parent,
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
        lv_obj_t *result = app_calibration_ui_find_named(
            lv_obj_get_child(parent, (int32_t)index), name);
        if (result != NULL)
        {
            return result;
        }
    }
    return NULL;
}

static lv_obj_t *app_calibration_ui_find_button(lv_obj_t *parent,
                                                 const char *label_text)
{
    if (parent == NULL)
    {
        return NULL;
    }

    if (lv_obj_check_type(parent, &lv_button_class))
    {
        lv_obj_t *label = app_calibration_ui_find_label(parent, label_text);
        if (label != NULL)
        {
            return parent;
        }
    }

    uint32_t child_count = lv_obj_get_child_count(parent);
    for (uint32_t index = 0U; index < child_count; ++index)
    {
        lv_obj_t *result = app_calibration_ui_find_button(
            lv_obj_get_child(parent, (int32_t)index), label_text);
        if (result != NULL)
        {
            return result;
        }
    }
    return NULL;
}

static lv_obj_t *app_calibration_ui_find_type(lv_obj_t *parent,
                                               const lv_obj_class_t *class_p,
                                               uint32_t *remaining)
{
    if ((parent == NULL) || (class_p == NULL) || (remaining == NULL))
    {
        return NULL;
    }

    if (lv_obj_check_type(parent, class_p))
    {
        if (*remaining == 0U)
        {
            return parent;
        }
        --(*remaining);
    }

    uint32_t child_count = lv_obj_get_child_count(parent);
    for (uint32_t index = 0U; index < child_count; ++index)
    {
        lv_obj_t *result = app_calibration_ui_find_type(
            lv_obj_get_child(parent, (int32_t)index), class_p, remaining);
        if (result != NULL)
        {
            return result;
        }
    }
    return NULL;
}

static void app_calibration_ui_set_disabled(lv_obj_t *object,
                                            bool disabled)
{
    if (object == NULL)
    {
        return;
    }

    if (disabled)
    {
        lv_obj_add_state(object, LV_STATE_DISABLED);
    }
    else
    {
        lv_obj_remove_state(object, LV_STATE_DISABLED);
    }
}

/*
 * P1-03 整改：业务请求被服务拒绝时不再静默丢弃返回值，
 * 记录计数并输出拒绝现场（服务状态与错误码）供定位。
 */
static void app_calibration_ui_report_reject(const char *action)
{
    AppCalibrationSnapshot snapshot;
    AppCalibrationService_GetSnapshot(&snapshot);
    ++s_request_reject_count;
    (void)AppOutput_Logf("[UI] REJECT %s state=%u error=%u\r\n",
                         action,
                         (unsigned int)snapshot.state,
                         (unsigned int)snapshot.error);
}

static void app_calibration_ui_selection_changed(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (target == s_internal_button)
    {
        s_mode = APP_CALIBRATION_MODE_INTERNAL;
    }
    else if (target == s_external_button)
    {
        s_mode = APP_CALIBRATION_MODE_EXTERNAL;
    }
    else if (target == s_source_dropdown)
    {
        uint32_t selected = lv_dropdown_get_selected(s_source_dropdown);
        if (selected <= (uint32_t)APP_CALIBRATION_SOURCE_EXTERNAL_INPUT)
        {
            s_source = (AppCalibrationSource)selected;
        }
    }
    if (!AppCalibrationService_SetSelection(s_mode, s_source))
    {
        app_calibration_ui_report_reject("SELECT");
    }
}

static uint32_t app_calibration_ui_selected_item(void)
{
    if (s_page == APP_CALIBRATION_UI_PAGE_TEST)
    {
        return (uint32_t)APP_CALIBRATION_ITEM_OUTPUT_SELF_TEST;
    }

    uint32_t selected = (s_item_dropdown != NULL)
        ? lv_dropdown_get_selected(s_item_dropdown) : 0U;
    return selected + (uint32_t)APP_CALIBRATION_ITEM_GAIN_OFFSET;
}

static void app_calibration_ui_start_clicked(lv_event_t *event)
{
    (void)event;

    uint32_t item_id = app_calibration_ui_selected_item();
    AppCalibrationRunConfig config = {
        .item_id = item_id,
        .expected_point_count =
            ((s_mode == APP_CALIBRATION_MODE_INTERNAL)
             && (s_source == APP_CALIBRATION_SOURCE_DAC1_TO_ADC))
                ? AppCalibrationBoardPort_GetExpectedPointCountForItem(
                    item_id) : 0U,
    };
    if (!AppCalibrationService_RequestStart(&config))
    {
        app_calibration_ui_report_reject("START");
    }
}

static void app_calibration_ui_stop_clicked(lv_event_t *event)
{
    (void)event;
    if (!AppCalibrationService_RequestFinishSession())
    {
        app_calibration_ui_report_reject("STOP");
    }
}

static void app_calibration_ui_exit_clicked(lv_event_t *event)
{
    (void)event;
    if (s_page == APP_CALIBRATION_UI_PAGE_TEST)
    {
        AppCalibrationBoardPort_SetAdcPreviewEnabled(false);
    }
    if (!AppCalibrationService_RequestExit())
    {
        app_calibration_ui_report_reject("EXIT");
    }
}

static void app_calibration_ui_adc_toggle_clicked(lv_event_t *event)
{
    (void)event;
    bool enabled = AppCalibrationBoardPort_GetAdcPreviewEnabled();
    bool next_enabled = !enabled;
    /*
     * 打开显示时立即渲染缓存中的最近一帧（测试结束后仍可回看末帧），
     * 后续新采集帧按序列号照常刷新；不再强制等待新帧。
     */
    s_adc_preview_wait_for_new = false;
    s_last_adc_preview_sequence = 0U;
    AppCalibrationBoardPort_SetAdcPreviewEnabled(next_enabled);
}

static void app_calibration_ui_confirm_clicked(lv_event_t *event)
{
    (void)event;
    if (!AppCalibrationService_RequestConfirmCandidate())
    {
        app_calibration_ui_report_reject("CONFIRM");
    }
}

static void app_calibration_ui_cancel_clicked(lv_event_t *event)
{
    (void)event;
    if (!AppCalibrationService_RequestCancelCandidate())
    {
        app_calibration_ui_report_reject("CANCEL");
    }
}

static void app_calibration_ui_apply_clicked(lv_event_t *event)
{
    (void)event;

    AppCalibrationSnapshot snapshot;
    AppCalibrationService_GetSnapshot(&snapshot);
    if (snapshot.runtime_applied)
    {
        if (!AppCalibrationService_RequestUndoApply())
        {
            app_calibration_ui_report_reject("UNDO");
        }
    }
    else
    {
        if (!AppCalibrationService_RequestApplyNow())
        {
            app_calibration_ui_report_reject("APPLY");
        }
    }
}

static void app_calibration_ui_bind(lv_obj_t *screen)
{
    AppCalibrationUiPage previous_page = s_page;
    AppCalibrationUiPage next_page = app_calibration_ui_page(screen);
    if ((previous_page == APP_CALIBRATION_UI_PAGE_TEST)
        && (next_page != APP_CALIBRATION_UI_PAGE_TEST))
    {
        AppCalibrationBoardPort_SetAdcPreviewEnabled(false);
    }

    s_screen = screen;
    s_page = next_page;
    const char *prefix = (s_page == APP_CALIBRATION_UI_PAGE_TEST)
        ? "test" : "cal";
    char name[40];

    (void)snprintf(name, sizeof(name), "%s_internal_button", prefix);
    s_internal_button = app_calibration_ui_find_named(
        screen, name);
    (void)snprintf(name, sizeof(name), "%s_external_button", prefix);
    s_external_button = app_calibration_ui_find_named(
        screen, name);
    (void)snprintf(name, sizeof(name), "%s_source_dropdown", prefix);
    s_source_dropdown = app_calibration_ui_find_named(
        screen, name);
    (void)snprintf(name, sizeof(name), "%s_item_dropdown", prefix);
    s_item_dropdown = app_calibration_ui_find_named(
        screen, name);
    (void)snprintf(name, sizeof(name), "%s_start_button", prefix);
    s_start_button = app_calibration_ui_find_named(screen, name);
    (void)snprintf(name, sizeof(name), "%s_stop_button", prefix);
    s_stop_button = app_calibration_ui_find_named(screen, name);
    s_export_button = app_calibration_ui_find_named(
        screen, "cal_export_button");
    s_import_button = app_calibration_ui_find_named(
        screen, "cal_import_button");
    s_confirm_button = app_calibration_ui_find_named(
        screen, "cal_confirm_button");
    s_cancel_button = app_calibration_ui_find_named(
        screen, "cal_cancel_button");
    s_apply_button = app_calibration_ui_find_named(screen, "cal_apply_button");
    s_apply_label = app_calibration_ui_find_label(s_apply_button, "APPLY NOW");

    s_adc_toggle_button = app_calibration_ui_find_named(
        screen, "test_adc_toggle_button");
    s_adc_toggle_label = app_calibration_ui_find_named(
        screen, "test_adc_toggle_label");
    s_adc_chart = app_calibration_ui_find_named(screen, "test_adc_chart");
    s_adc_chart_series = (s_adc_chart != NULL)
        ? lv_chart_get_series_next(s_adc_chart, NULL) : NULL;
    s_adc_rate_label = app_calibration_ui_find_named(
        screen, "test_adc_rate_label");
    s_adc_points_label = app_calibration_ui_find_named(
        screen, "test_adc_points_label");
    s_adc_span_label = app_calibration_ui_find_named(
        screen, "test_adc_span_label");
    s_adc_x_mid_label = app_calibration_ui_find_named(
        screen, "test_adc_x_mid_label");
    s_adc_x_end_label = app_calibration_ui_find_named(
        screen, "test_adc_x_end_label");
    s_last_adc_preview_sequence = 0U;

    lv_obj_t *back_button = app_calibration_ui_find_button(screen, "BACK");
    lv_obj_t *home_button = app_calibration_ui_find_button(screen, "HOME");
    uint32_t first_bar = 0U;
    s_progress_bar = app_calibration_ui_find_type(
        screen, &lv_bar_class, &first_bar);
    (void)snprintf(name, sizeof(name), "%s_step_label", prefix);
    s_step_label = app_calibration_ui_find_named(screen, name);
    (void)snprintf(name, sizeof(name), "%s_error_label", prefix);
    s_error_label = app_calibration_ui_find_named(screen, name);
    (void)snprintf(name, sizeof(name), "%s_points_label", prefix);
    s_points_label = app_calibration_ui_find_named(screen, name);
    s_storage_label = app_calibration_ui_find_named(
        screen, "cal_storage_label");
    s_profile_label = app_calibration_ui_find_named(
        screen, "cal_profile_label");
    s_exclusive_label = app_calibration_ui_find_named(
        screen, (s_page == APP_CALIBRATION_UI_PAGE_TEST)
            ? "test_exclusive_label" : "cal_exclusive_label");
    s_safe_label = app_calibration_ui_find_named(
        screen, (s_page == APP_CALIBRATION_UI_PAGE_TEST)
            ? "test_safe_label" : "cal_safe_label");
    for (uint32_t level = 0U;
         level < CALIBRATION_SELF_TEST_LEVEL_COUNT;
         ++level)
    {
        s_self_test_output[level] = app_calibration_ui_find_named(
            screen, s_self_test_output_names[level]);
        s_self_test_amplitude[level] = app_calibration_ui_find_named(
            screen, s_self_test_amplitude_names[level]);
        s_self_test_noise[level] = app_calibration_ui_find_named(
            screen, s_self_test_noise_names[level]);
        s_self_test_result[level] = app_calibration_ui_find_named(
            screen, s_self_test_result_names[level]);
    }

    /*
     * P2-04 整改：控件靠运行时对象名查找，Designer 改名会静默失效；
     * 绑定完成后立即统计本页应有控件的缺失数量并输出诊断。
     */
    {
        struct
        {
            lv_obj_t *object;
            const char *label;
            bool required;
        } checks[] = {
            {s_internal_button, "internal", true},
            {s_external_button, "external", true},
            {s_source_dropdown, "source", true},
            {s_item_dropdown, "item",
             s_page == APP_CALIBRATION_UI_PAGE_CALIBRATION},
            {s_start_button, "start", true},
            {s_stop_button, "stop", true},
            {s_step_label, "step", true},
            {s_error_label, "error", true},
            {s_points_label, "points", true},
            {s_exclusive_label, "exclusive", true},
            {s_safe_label, "safe", true},
            {s_adc_toggle_button, "adc_toggle",
             s_page == APP_CALIBRATION_UI_PAGE_TEST},
            {s_adc_chart, "adc_chart",
             s_page == APP_CALIBRATION_UI_PAGE_TEST},
            {s_storage_label, "storage",
             s_page == APP_CALIBRATION_UI_PAGE_CALIBRATION},
            {s_profile_label, "profile",
             s_page == APP_CALIBRATION_UI_PAGE_CALIBRATION},
            {s_export_button, "export",
             s_page == APP_CALIBRATION_UI_PAGE_CALIBRATION},
            {s_confirm_button, "confirm",
             s_page == APP_CALIBRATION_UI_PAGE_CALIBRATION},
            {s_cancel_button, "cancel",
             s_page == APP_CALIBRATION_UI_PAGE_CALIBRATION},
            {s_apply_button, "apply",
             s_page == APP_CALIBRATION_UI_PAGE_CALIBRATION},
        };
        uint32_t missing = 0U;
        const char *first_missing = NULL;
        for (uint32_t index = 0U;
             index < (sizeof(checks) / sizeof(checks[0]));
             ++index)
        {
            if (checks[index].required && (checks[index].object == NULL))
            {
                ++missing;
                if (first_missing == NULL)
                {
                    first_missing = checks[index].label;
                }
            }
        }
        s_bind_missing_count = missing;
        if (missing != 0U)
        {
            (void)AppOutput_Logf(
                "[UI] BIND %s missing=%lu first=%s\r\n",
                (s_page == APP_CALIBRATION_UI_PAGE_TEST) ? "TEST" : "CAL",
                (unsigned long)missing,
                first_missing);
        }
    }

    lv_obj_t *selection_objects[] = {
        s_internal_button, s_external_button, s_source_dropdown,
        s_item_dropdown
    };
    for (uint32_t index = 0U;
         index < (sizeof(selection_objects) / sizeof(selection_objects[0]));
         ++index)
    {
        if (selection_objects[index] != NULL)
        {
            lv_obj_add_event_cb(selection_objects[index],
                                app_calibration_ui_selection_changed,
                                ((selection_objects[index] == s_source_dropdown)
                                 || (selection_objects[index]
                                     == s_item_dropdown))
                                    ? LV_EVENT_VALUE_CHANGED
                                    : LV_EVENT_CLICKED,
                                NULL);
        }
    }
    if (s_start_button != NULL)
    {
        lv_obj_add_event_cb(s_start_button,
                            app_calibration_ui_start_clicked,
                            LV_EVENT_CLICKED,
                            NULL);
    }

    if (s_stop_button != NULL)
    {
        lv_obj_add_event_cb(s_stop_button,
                            app_calibration_ui_stop_clicked,
                            LV_EVENT_CLICKED,
                            NULL);
    }

    lv_obj_t *exit_buttons[] = {back_button, home_button};
    for (uint32_t index = 0U;
         index < (sizeof(exit_buttons) / sizeof(exit_buttons[0]));
         ++index)
    {
        if (exit_buttons[index] != NULL)
        {
            lv_obj_add_event_cb(exit_buttons[index],
                                app_calibration_ui_exit_clicked,
                                LV_EVENT_CLICKED,
                                NULL);
        }
    }

    if (s_confirm_button != NULL)
    {
        lv_obj_add_event_cb(s_confirm_button,
                            app_calibration_ui_confirm_clicked,
                            LV_EVENT_CLICKED,
                            NULL);
    }
    if (s_cancel_button != NULL)
    {
        lv_obj_add_event_cb(s_cancel_button,
                            app_calibration_ui_cancel_clicked,
                            LV_EVENT_CLICKED,
                            NULL);
    }
    if (s_apply_button != NULL)
    {
        lv_obj_add_event_cb(s_apply_button,
                            app_calibration_ui_apply_clicked,
                            LV_EVENT_CLICKED,
                            NULL);
    }
    if (s_adc_toggle_button != NULL)
    {
        lv_obj_add_event_cb(s_adc_toggle_button,
                            app_calibration_ui_adc_toggle_clicked,
                            LV_EVENT_CLICKED,
                            NULL);
    }

    if (s_source_dropdown != NULL)
    {
        lv_dropdown_set_selected(s_source_dropdown, (uint32_t)s_source);
    }

    AppCalibrationSnapshot snapshot = {0};
    AppCalibrationService_GetSnapshot(&snapshot);
    bool role_changed = (previous_page != APP_CALIBRATION_UI_PAGE_NONE)
        && (previous_page != s_page);
    if (snapshot.state == APP_CALIBRATION_STATE_IDLE)
    {
        /*
         * worker 任务尚在初始化（QSPI 恢复配置）时 RequestEnter 会被
         * 就绪门禁拒绝，此时必须转入 pending 由 Run() 持续重试，
         * 否则页面会永远停留在 IDLE 且所有按钮不可用。
         */
        s_reenter_pending =
            !AppCalibrationService_RequestEnter(s_mode, s_source);
    }
    else
    {
        s_reenter_pending = true;
        if (role_changed && snapshot.exclusive)
        {
            (void)AppCalibrationService_RequestExit();
        }
    }
}

static const char *app_calibration_ui_state_text(AppCalibrationState state)
{
    if ((s_page == APP_CALIBRATION_UI_PAGE_CALIBRATION)
        && (state == APP_CALIBRATION_STATE_READY))
    {
        return "READY / CAL LOGIC PENDING";
    }

    switch (state)
    {
        case APP_CALIBRATION_STATE_IDLE:
            return "IDLE / PRESS START";
        case APP_CALIBRATION_STATE_ACQUIRE_EXCLUSIVE:
            return "ACQUIRE EXCLUSIVE";
        case APP_CALIBRATION_STATE_WAIT_FOR_IDLE:
            return "WAIT FOR ADDA IDLE";
        case APP_CALIBRATION_STATE_READY:
            return "READY / HARDWARE LOCKED";
        case APP_CALIBRATION_STATE_PREPARE_SESSION:
            return "PREPARE SESSION";
        case APP_CALIBRATION_STATE_SESSION_ACTIVE:
            return "MEASUREMENT ACTIVE";
        case APP_CALIBRATION_STATE_REVIEW:
            return "REVIEW / EXPORT OR IMPORT";
        case APP_CALIBRATION_STATE_STOPPING:
            return "RESTORE SAFE STATE";
        case APP_CALIBRATION_STATE_ERROR:
        default:
            return "CALIBRATION ERROR";
    }
}

static const char *app_calibration_ui_error_text(AppCalibrationError error)
{
    switch (error)
    {
        case APP_CALIBRATION_ERROR_NONE:
            return "--";
        case APP_CALIBRATION_ERROR_BUSY:
            return "BUSY";
        case APP_CALIBRATION_ERROR_RELAY_NOT_CONFIGURED:
            return "RELAY NOT CONFIGURED";
        case APP_CALIBRATION_ERROR_SOURCE_NOT_SUPPORTED:
            return "SOURCE NOT ROUTED";
        case APP_CALIBRATION_ERROR_ADC_NOT_CONFIGURED:
            return "ADC NOT CONFIGURED";
        case APP_CALIBRATION_ERROR_DAC_NOT_CONFIGURED:
            return "DAC NOT CONFIGURED";
        case APP_CALIBRATION_ERROR_STORAGE_NOT_CONFIGURED:
            return "QSPI NOT READY";
        case APP_CALIBRATION_ERROR_SAFE_STOP_TIMEOUT:
            return "SAFE STOP TIMEOUT";
        case APP_CALIBRATION_ERROR_PORT_IO:
            return "PORT I/O ERROR";
        case APP_CALIBRATION_ERROR_PROFILE_INVALID:
            return "PROFILE INVALID";
        case APP_CALIBRATION_ERROR_NO_ROLLBACK_PROFILE:
            return "NO OLD PROFILE";
        default:
            return "UNKNOWN ERROR";
    }
}

static const char *app_calibration_ui_self_test_result_text(
    const CalibrationSelfTestResult *result)
{
    if (result->state == CALIBRATION_SELF_TEST_NOT_RUN)
    {
        return "NOT RUN";
    }
    if (result->state == CALIBRATION_SELF_TEST_RUNNING)
    {
        return "RUNNING";
    }
    if (result->state == CALIBRATION_SELF_TEST_PASS)
    {
        return "PASS";
    }
    if (result->state == CALIBRATION_SELF_TEST_BLOCKED)
    {
        return (result->reason
                == CALIBRATION_SELF_TEST_REASON_TRANSFER_PLATEAU)
            ? "PLATEAU" : "BLOCKED";
    }
    if (result->state == CALIBRATION_SELF_TEST_ABORTED)
    {
        return "ABORTED";
    }

    switch (result->reason)
    {
        case CALIBRATION_SELF_TEST_REASON_OUTPUT_IO:
            return "I/O ERR";
        case CALIBRATION_SELF_TEST_REASON_AMPLITUDE:
            return "AMP ERR";
        case CALIBRATION_SELF_TEST_REASON_NOISE:
            return "NOISE";
        case CALIBRATION_SELF_TEST_REASON_CLIPPED:
            return "CLIPPED";
        case CALIBRATION_SELF_TEST_REASON_TRANSFER_PLATEAU:
            return "PLATEAU";
        case CALIBRATION_SELF_TEST_REASON_NO_RESPONSE:
            return "NO RESP";
        default:
            return "ABNORMAL";
    }
}

static uint32_t app_calibration_ui_self_test_color(
    CalibrationSelfTestState state)
{
    switch (state)
    {
        case CALIBRATION_SELF_TEST_PASS:
            return 0x72eaa5;
        case CALIBRATION_SELF_TEST_RUNNING:
            return 0x80d6ff;
        case CALIBRATION_SELF_TEST_BLOCKED:
            return 0xff9f43;
        case CALIBRATION_SELF_TEST_ABNORMAL:
        case CALIBRATION_SELF_TEST_ABORTED:
            return 0xff6b6b;
        case CALIBRATION_SELF_TEST_NOT_RUN:
        default:
            return 0x9aa4ad;
    }
}

static void app_calibration_ui_render_self_test(void)
{
    AppCalibrationSelfTestStatus status = {0};
    AppCalibrationBoardPort_GetSelfTestStatus(&status);
    for (uint32_t level = 0U;
         level < CALIBRATION_SELF_TEST_LEVEL_COUNT;
         ++level)
    {
        const CalibrationSelfTestResult *result = &status.results[level];
        char text[24];
        if (s_self_test_output[level] != NULL)
        {
            const char *output_text = "--";
            if (result->state == CALIBRATION_SELF_TEST_RUNNING)
            {
                output_text = "RUN";
            }
            else if ((result->state != CALIBRATION_SELF_TEST_NOT_RUN)
                     && (result->state != CALIBRATION_SELF_TEST_BLOCKED))
            {
                output_text = result->output_ok ? "OK" : "FAULT";
            }
            lv_label_set_text(s_self_test_output[level], output_text);
        }
        if (s_self_test_amplitude[level] != NULL)
        {
            if (result->sample_count == 0U)
            {
                lv_label_set_text(s_self_test_amplitude[level], "--");
            }
            else
            {
                (void)snprintf(text, sizeof(text), "%+.1f%%",
                               (double)result->amplitude_error_percent);
                lv_label_set_text(s_self_test_amplitude[level], text);
            }
        }
        if (s_self_test_noise[level] != NULL)
        {
            if (result->sample_count == 0U)
            {
                lv_label_set_text(s_self_test_noise[level], "--");
            }
            else
            {
                (void)snprintf(text, sizeof(text), "%.1f c",
                               (double)result->noise_rms_code);
                lv_label_set_text(s_self_test_noise[level], text);
            }
        }
        if (s_self_test_result[level] != NULL)
        {
            lv_label_set_text(s_self_test_result[level],
                              app_calibration_ui_self_test_result_text(result));
            lv_obj_set_style_text_color(
                s_self_test_result[level],
                lv_color_hex(app_calibration_ui_self_test_color(result->state)),
                0);
        }
    }
}

static void app_calibration_ui_render_adc_preview(void)
{
    if ((s_page != APP_CALIBRATION_UI_PAGE_TEST)
        || (s_adc_toggle_button == NULL))
    {
        return;
    }

    bool enabled = AppCalibrationBoardPort_GetAdcPreviewEnabled();
    if (s_adc_toggle_label != NULL)
    {
        lv_label_set_text(s_adc_toggle_label,
                          enabled ? "ADC DISPLAY ON" : "ADC DISPLAY OFF");
    }
    lv_obj_set_style_bg_color(
        s_adc_toggle_button,
        lv_color_hex(enabled ? 0x176b87 : 0x7a5b16),
        0);

    if (!enabled)
    {
        if (s_adc_rate_label != NULL)
        {
            lv_label_set_text(s_adc_rate_label, "-- MS/s");
        }
        if (s_adc_points_label != NULL)
        {
            lv_label_set_text(s_adc_points_label, "--");
        }
        if (s_adc_span_label != NULL)
        {
            lv_label_set_text(s_adc_span_label, "DISPLAY OFF");
        }
        if (s_adc_x_mid_label != NULL)
        {
            lv_label_set_text(s_adc_x_mid_label, "-- ms");
        }
        if (s_adc_x_end_label != NULL)
        {
            lv_label_set_text(s_adc_x_end_label, "-- ms");
        }
        return;
    }

    AppCalibrationAdcPreview preview = {0};
    if (!AppCalibrationBoardPort_GetAdcPreview(&preview)
        || (s_adc_preview_wait_for_new
            && (preview.sequence == s_last_adc_preview_sequence)))
    {
        if (s_adc_span_label != NULL)
        {
            lv_label_set_text(s_adc_span_label, "WAIT FOR ADC");
        }
        return;
    }

    s_adc_preview_wait_for_new = false;
    if ((preview.sequence != s_last_adc_preview_sequence)
        && (s_adc_chart != NULL) && (s_adc_chart_series != NULL))
    {
        static int32_t chart_values[APP_CALIBRATION_ADC_PREVIEW_POINTS];
        for (uint32_t index = 0U; index < preview.point_count; ++index)
        {
            chart_values[index] = preview.samples[index];
        }
        lv_chart_set_point_count(s_adc_chart, preview.point_count);
        lv_chart_set_series_values(s_adc_chart,
                                   s_adc_chart_series,
                                   chart_values,
                                   preview.point_count);
        lv_chart_refresh(s_adc_chart);
        s_last_adc_preview_sequence = preview.sequence;
    }

    double rate_msps = (double)preview.sample_rate_hz / 1000000.0;
    double span_ms = ((double)preview.source_sample_count * 1000.0)
        / (double)preview.sample_rate_hz;
    char text[40];
    if (s_adc_rate_label != NULL)
    {
        (void)snprintf(text, sizeof(text), "%.3f MS/s", rate_msps);
        lv_label_set_text(s_adc_rate_label, text);
    }
    if (s_adc_points_label != NULL)
    {
        (void)snprintf(text,
                       sizeof(text),
                       "%lu src / %u plot",
                       (unsigned long)preview.source_sample_count,
                       (unsigned int)preview.point_count);
        lv_label_set_text(s_adc_points_label, text);
    }
    if (s_adc_span_label != NULL)
    {
        (void)snprintf(text, sizeof(text), "%.3f ms", span_ms);
        lv_label_set_text(s_adc_span_label, text);
    }
    if (s_adc_x_mid_label != NULL)
    {
        (void)snprintf(text, sizeof(text), "%.3f ms", span_ms * 0.5);
        lv_label_set_text(s_adc_x_mid_label, text);
    }
    if (s_adc_x_end_label != NULL)
    {
        (void)snprintf(text, sizeof(text), "%.3f ms", span_ms);
        lv_label_set_text(s_adc_x_end_label, text);
    }
}

static void app_calibration_ui_render(void)
{
    AppCalibrationSnapshot snapshot;
    char text[64];
    AppCalibrationService_GetSnapshot(&snapshot);

    if (s_step_label != NULL)
    {
        lv_label_set_text(s_step_label,
                          app_calibration_ui_state_text(snapshot.state));
    }
    if (s_error_label != NULL)
    {
        lv_label_set_text(s_error_label,
                          app_calibration_ui_error_text(snapshot.error));
    }
    if (s_storage_label != NULL)
    {
        lv_label_set_text(s_storage_label,
                          snapshot.qspi_ready ? "QSPI READY" : "QSPI NOT READY");
        lv_obj_set_style_text_color(
            s_storage_label,
            lv_color_hex(snapshot.qspi_ready ? 0x72eaa5 : 0xff9f43),
            0);
    }
    if (s_profile_label != NULL)
    {
        if (snapshot.pending_present)
        {
            (void)snprintf(text,
                           sizeof(text),
                           "ACTIVE: %lu   PENDING: %lu",
                           (unsigned long)snapshot.active_profile_sequence,
                           (unsigned long)snapshot.pending_profile_sequence);
        }
        else
        {
            (void)snprintf(text,
                           sizeof(text),
                           "ACTIVE: %lu   PENDING: NONE",
                           (unsigned long)snapshot.active_profile_sequence);
        }
        lv_label_set_text(s_profile_label, text);
    }
    if (s_points_label != NULL)
    {
        if (snapshot.expected_point_count == 0U)
        {
            (void)snprintf(text,
                           sizeof(text),
                           "%lu / --",
                           (unsigned long)snapshot.stored_point_count);
        }
        else
        {
            (void)snprintf(text,
                           sizeof(text),
                           "%lu / %lu",
                           (unsigned long)snapshot.stored_point_count,
                           (unsigned long)snapshot.expected_point_count);
        }
        lv_label_set_text(s_points_label, text);
    }
    if (s_exclusive_label != NULL)
    {
        lv_label_set_text(s_exclusive_label,
                          snapshot.exclusive ? "EXCLUSIVE" : "RELEASED");
    }
    if (s_safe_label != NULL)
    {
        lv_label_set_text(s_safe_label,
                          snapshot.exclusive ? "SAFE" : "IDLE");
    }
    if (s_progress_bar != NULL)
    {
        lv_bar_set_value(s_progress_bar, snapshot.progress_percent, true);
    }
    if (s_apply_label != NULL)
    {
        lv_label_set_text(s_apply_label,
                          snapshot.runtime_applied
                              ? "UNDO APPLY" : "APPLY NOW");
    }
    if (s_page == APP_CALIBRATION_UI_PAGE_TEST)
    {
        app_calibration_ui_render_self_test();
        app_calibration_ui_render_adc_preview();
    }

    uint32_t selected_item = app_calibration_ui_selected_item();
    bool self_test_review =
        (snapshot.state == APP_CALIBRATION_STATE_REVIEW)
        && (selected_item
            == (uint32_t)APP_CALIBRATION_ITEM_OUTPUT_SELF_TEST);
    bool start_allowed =
        (s_page == APP_CALIBRATION_UI_PAGE_TEST)
        && ((snapshot.state == APP_CALIBRATION_STATE_READY)
            || self_test_review);
    /*
     * 输出自检不产生待确认校准数据，REVIEW（自检完成）状态下允许
     * 重新选择模式/来源后直接重测；其他校准项目仍锁定到 READY。
     */
    bool selection_locked =
        (snapshot.state != APP_CALIBRATION_STATE_READY)
        && !self_test_review;
    app_calibration_ui_set_disabled(s_internal_button, selection_locked);
    app_calibration_ui_set_disabled(s_external_button, selection_locked);
    app_calibration_ui_set_disabled(s_source_dropdown, selection_locked);
    app_calibration_ui_set_disabled(s_item_dropdown, selection_locked);
    app_calibration_ui_set_disabled(
        s_start_button,
        !start_allowed || !snapshot.can_start);
    app_calibration_ui_set_disabled(
        s_stop_button,
        snapshot.state != APP_CALIBRATION_STATE_SESSION_ACTIVE);

    app_calibration_ui_set_disabled(
        s_export_button,
        !snapshot.qspi_ready || !snapshot.session_present);
    /* 导入/导出传输协议未确定前，IMPORT 保持硬禁用，避免空按钮。 */
    app_calibration_ui_set_disabled(s_import_button, true);
    app_calibration_ui_set_disabled(
        s_confirm_button,
        !snapshot.qspi_ready || !snapshot.candidate_present);
    app_calibration_ui_set_disabled(
        s_cancel_button,
        !snapshot.candidate_present);
    app_calibration_ui_set_disabled(
        s_apply_button,
        !snapshot.runtime_applied && !snapshot.pending_present);
}

void AppCalibrationUiAdapter_Run(void)
{
    lv_obj_t *active_screen = lv_screen_active();
    AppCalibrationUiPage active_page = app_calibration_ui_page(active_screen);
    if (active_page == APP_CALIBRATION_UI_PAGE_NONE)
    {
        if (s_screen != NULL)
        {
            if (s_page == APP_CALIBRATION_UI_PAGE_TEST)
            {
                AppCalibrationBoardPort_SetAdcPreviewEnabled(false);
            }
            if (AppCalibrationService_IsExclusive())
            {
                (void)AppCalibrationService_RequestExit();
            }
            s_screen = NULL;
            s_page = APP_CALIBRATION_UI_PAGE_NONE;
            s_reenter_pending = false;
        }
        return;
    }

    bool just_bound = false;
    if (active_screen != s_screen)
    {
        /* P1-06 整改：页面加载当个循环即完成绑定，消除未绑定窗口。 */
        app_calibration_ui_bind(active_screen);
        just_bound = true;
    }

    if (s_reenter_pending)
    {
        AppCalibrationSnapshot snapshot = {0};
        AppCalibrationService_GetSnapshot(&snapshot);
        if (snapshot.state == APP_CALIBRATION_STATE_IDLE)
        {
            /* 服务未就绪（worker 初始化中）会拒绝进入，保持 pending 重试。 */
            s_reenter_pending =
                !AppCalibrationService_RequestEnter(s_mode, s_source);
        }
    }

    uint32_t now = lv_tick_get();
    if (!just_bound
        && ((now - s_last_render_tick)
            < APP_CALIBRATION_UI_RENDER_PERIOD_MS))
    {
        return;
    }
    s_last_render_tick = now;
    app_calibration_ui_render();
}
