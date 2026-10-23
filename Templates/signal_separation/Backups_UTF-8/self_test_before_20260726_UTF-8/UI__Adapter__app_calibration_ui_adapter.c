/**
  ******************************************************************************
  * @file    app_calibration_ui_adapter.c
  * @brief   校准全页与独占服务的 LVGL 适配实现（UTF-8）。
  ******************************************************************************
  */

#include "app_calibration_ui_adapter.h"

#include "app_calibration_board_port.h"
#include "app_calibration_service.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static lv_obj_t *s_screen = NULL;
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
static AppCalibrationMode s_mode = APP_CALIBRATION_MODE_INTERNAL;
static AppCalibrationSource s_source = APP_CALIBRATION_SOURCE_DAC1_TO_ADC;

static bool app_calibration_ui_is_screen(lv_obj_t *screen)
{
    const char *name = (screen != NULL) ? lv_obj_get_name(screen) : NULL;
    return (name != NULL)
        && (strncmp(name, "screen_calibration", 18U) == 0);
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
    (void)AppCalibrationService_SetSelection(s_mode, s_source);
}

static void app_calibration_ui_start_clicked(lv_event_t *event)
{
    (void)event;

    AppCalibrationRunConfig config = {
        .item_id = (s_item_dropdown != NULL)
            ? lv_dropdown_get_selected(s_item_dropdown) : 0U,
        .expected_point_count =
            ((s_mode == APP_CALIBRATION_MODE_INTERNAL)
             && (s_source == APP_CALIBRATION_SOURCE_DAC1_TO_ADC))
                ? AppCalibrationBoardPort_GetExpectedPointCount() : 0U,
    };
    (void)AppCalibrationService_RequestStart(&config);
}

static void app_calibration_ui_stop_clicked(lv_event_t *event)
{
    (void)event;
    (void)AppCalibrationService_RequestFinishSession();
}

static void app_calibration_ui_exit_clicked(lv_event_t *event)
{
    (void)event;
    (void)AppCalibrationService_RequestExit();
}

static void app_calibration_ui_confirm_clicked(lv_event_t *event)
{
    (void)event;
    (void)AppCalibrationService_RequestConfirmCandidate();
}

static void app_calibration_ui_cancel_clicked(lv_event_t *event)
{
    (void)event;
    (void)AppCalibrationService_RequestCancelCandidate();
}

static void app_calibration_ui_apply_clicked(lv_event_t *event)
{
    (void)event;

    AppCalibrationSnapshot snapshot;
    AppCalibrationService_GetSnapshot(&snapshot);
    if (snapshot.runtime_applied)
    {
        (void)AppCalibrationService_RequestUndoApply();
    }
    else
    {
        (void)AppCalibrationService_RequestApplyNow();
    }
}

static void app_calibration_ui_bind(lv_obj_t *screen)
{
    s_screen = screen;
    s_internal_button = app_calibration_ui_find_named(
        screen, "cal_internal_button");
    s_external_button = app_calibration_ui_find_named(
        screen, "cal_external_button");
    s_source_dropdown = app_calibration_ui_find_named(
        screen, "cal_source_dropdown");
    s_item_dropdown = app_calibration_ui_find_named(
        screen, "cal_item_dropdown");
    s_start_button = app_calibration_ui_find_named(screen, "cal_start_button");
    s_stop_button = app_calibration_ui_find_named(screen, "cal_stop_button");
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

    lv_obj_t *back_button = app_calibration_ui_find_button(screen, "BACK");
    lv_obj_t *home_button = app_calibration_ui_find_button(screen, "HOME");
    uint32_t first_bar = 0U;
    s_progress_bar = app_calibration_ui_find_type(
        screen, &lv_bar_class, &first_bar);
    s_step_label = app_calibration_ui_find_named(screen, "cal_step_label");
    s_error_label = app_calibration_ui_find_named(screen, "cal_error_label");
    s_points_label = app_calibration_ui_find_named(screen, "cal_points_label");
    s_storage_label = app_calibration_ui_find_named(
        screen, "cal_storage_label");
    s_profile_label = app_calibration_ui_find_named(
        screen, "cal_profile_label");
    s_exclusive_label = app_calibration_ui_find_named(
        screen, "cal_exclusive_label");
    s_safe_label = app_calibration_ui_find_named(screen, "cal_safe_label");

    lv_obj_t *selection_objects[] = {
        s_internal_button, s_external_button, s_source_dropdown
    };
    for (uint32_t index = 0U;
         index < (sizeof(selection_objects) / sizeof(selection_objects[0]));
         ++index)
    {
        if (selection_objects[index] != NULL)
        {
            lv_obj_add_event_cb(selection_objects[index],
                                app_calibration_ui_selection_changed,
                                (selection_objects[index] == s_source_dropdown)
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

    if (s_stop_button != NULL)
    {
        lv_obj_add_event_cb(s_stop_button,
                            app_calibration_ui_stop_clicked,
                            LV_EVENT_CLICKED,
                            NULL);
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

    if (s_source_dropdown != NULL)
    {
        lv_dropdown_set_selected(s_source_dropdown, (uint32_t)s_source);
    }
    (void)AppCalibrationService_RequestEnter(s_mode, s_source);
}

static const char *app_calibration_ui_state_text(AppCalibrationState state)
{
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

    bool selection_locked = snapshot.state != APP_CALIBRATION_STATE_READY;
    app_calibration_ui_set_disabled(s_internal_button, selection_locked);
    app_calibration_ui_set_disabled(s_external_button, selection_locked);
    app_calibration_ui_set_disabled(s_source_dropdown, selection_locked);
    app_calibration_ui_set_disabled(
        s_start_button,
        (snapshot.state != APP_CALIBRATION_STATE_READY)
            || !snapshot.can_start);
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
    if (!app_calibration_ui_is_screen(active_screen))
    {
        if (s_screen != NULL)
        {
            if (AppCalibrationService_IsExclusive())
            {
                (void)AppCalibrationService_RequestExit();
            }
            s_screen = NULL;
        }
        return;
    }

    if (active_screen != s_screen)
    {
        app_calibration_ui_bind(active_screen);
    }
    app_calibration_ui_render();
}
