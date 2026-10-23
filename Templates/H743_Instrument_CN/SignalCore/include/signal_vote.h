#ifndef APP_SIGNAL_DETECT_SIGNAL_VOTE_H_
#define APP_SIGNAL_DETECT_SIGNAL_VOTE_H_

#include <stdbool.h>
#include <stdint.h>

#include "signal_joint_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* E7 多帧识别状态与最终错误码（UTF-8）。 */
typedef enum
{
    SIGNAL_DETECT_IDLE = 0,
    SIGNAL_DETECT_ARMED,
    SIGNAL_DETECT_WAIT_FRAME,
    SIGNAL_DETECT_PROCESS_FRAME,
    SIGNAL_DETECT_COLLECT_RESULT,
    SIGNAL_DETECT_FINALIZE,
    SIGNAL_DETECT_FINISHED,
    SIGNAL_DETECT_ERROR
} SignalDetectState;

typedef enum
{
    SIGNAL_DETECT_ERR_NONE = 0,
    SIGNAL_DETECT_ERR_FRAME_SIZE,
    SIGNAL_DETECT_ERR_LOW_AMPLITUDE,
    SIGNAL_DETECT_ERR_CLIPPING,
    SIGNAL_DETECT_ERR_NOT_ENOUGH_VALID_FRAMES,
    SIGNAL_DETECT_ERR_MODEL_AMBIGUOUS,
    SIGNAL_DETECT_ERR_CAPTURE_TIMEOUT,
    SIGNAL_DETECT_ERR_STOPPED,
    SIGNAL_DETECT_ERR_INTERNAL
} SignalDetectError;

typedef struct
{
    float frequency_hz;
    float amplitude_code;
    float phase_rad;
    SignalWaveType type;
    uint32_t frequency_votes;
    uint32_t type_votes;
} SignalVoteComponent;

typedef struct
{
    SignalDetectState state;
    SignalDetectError error;
    uint32_t requested_frames;
    uint32_t processed_frames;
    uint32_t valid_frames;
    uint32_t invalid_frames;
    uint32_t valid_frame_mask;
    uint32_t invalid_reason_mask;
    uint32_t busy_start_requests;
    uint32_t stop_requests;
    uint32_t sample_rate_hz;
    SignalVoteComponent components[SIGNAL_JOINT_COMPONENT_COUNT];
    float median_residual;
    float median_model_confidence;
    float total_confidence;
    uint32_t analysis_time_us;
    uint32_t session_time_ms;
    bool valid;
} SignalVoteResult;

/* E7 重复启动诊断只保存汇总，不保存历史帧或完整会话副本。 */
typedef struct
{
    uint32_t session_count;
    uint32_t valid_session_count;
    uint32_t invalid_session_count;
    uint32_t inconsistent_session_count;
    uint32_t minimum_valid_frames;
    uint32_t reference_frequency_hz[SIGNAL_JOINT_COMPONENT_COUNT];
    SignalWaveType reference_type[SIGNAL_JOINT_COMPONENT_COUNT];
    float minimum_confidence;
    float maximum_confidence;
    uint32_t maximum_session_time_ms;
} SignalVoteDiagnostics;

/* GDB 和 UI 均可只读此最终快照。 */
extern volatile SignalVoteResult g_signal_vote_result;
extern volatile SignalVoteDiagnostics g_signal_vote_diagnostics;

void SignalVote_Begin(void);
void SignalVote_SetState(SignalDetectState state);
void SignalVote_AddFrame(const SignalJointResult *result,
                         uint32_t analysis_time_us);
void SignalVote_AddInvalidFrame(SignalDetectError error);
bool SignalVote_Finalize(uint32_t session_time_ms,
                         uint32_t busy_start_requests,
                         uint32_t stop_requests,
                         SignalVoteResult *result);
const char *SignalVote_ErrorString(SignalDetectError error);

#ifdef __cplusplus
}
#endif

#endif /* APP_SIGNAL_DETECT_SIGNAL_VOTE_H_ */
