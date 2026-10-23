#ifndef APP_SERIAL_CONTROL_APP_UI_REMOTE_CONTROL_H_
#define APP_SERIAL_CONTROL_APP_UI_REMOTE_CONTROL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    APP_UI_REMOTE_ACTION_RUN = 0,
    APP_UI_REMOTE_ACTION_STOP,
    APP_UI_REMOTE_ACTION_PHASE_DECREASE,
    APP_UI_REMOTE_ACTION_PHASE_INCREASE,
    APP_UI_REMOTE_ACTION_CALIBRATION_START,
    APP_UI_REMOTE_ACTION_CALIBRATION_STOP,
    APP_UI_REMOTE_ACTION_PAGE_MONITOR,
    APP_UI_REMOTE_ACTION_PAGE_SPECTRUM,
    APP_UI_REMOTE_ACTION_PAGE_RESULTS,
    APP_UI_REMOTE_ACTION_PAGE_CALIBRATION,
    APP_UI_REMOTE_ACTION_PAGE_SYSTEM
} AppUiRemoteAction;

typedef struct
{
    uint32_t queued_count;
    uint32_t executed_count;
    uint32_t rejected_count;
    uint32_t dropped_count;
    AppUiRemoteAction last_action;
    bool last_result;
} AppUiRemoteStats;

/** Create the UI command queue after osKernelInitialize(). */
bool AppUiRemoteControl_Init(void);

/** Queue one semantic UI touch action from task context. */
bool AppUiRemoteControl_Post(AppUiRemoteAction action);

/** Execute queued actions. Call only from DisplayTask. */
void AppUiRemoteControl_Run(void);

void AppUiRemoteControl_GetStats(AppUiRemoteStats *stats);
const char *AppUiRemoteControl_ActionName(AppUiRemoteAction action);

#ifdef __cplusplus
}
#endif

#endif /* APP_SERIAL_CONTROL_APP_UI_REMOTE_CONTROL_H_ */
