#ifndef APP_SIGNAL_DETECT_SIGNAL_JOINT_MODEL_H_
#define APP_SIGNAL_DETECT_SIGNAL_JOINT_MODEL_H_

#include <stdbool.h>
#include <stdint.h>

#include "signal_fft.h"
#include "signal_harmonic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIGNAL_JOINT_COMPONENT_COUNT  2U

/* E6 联合模型中的单个信号分量（UTF-8）。 */
typedef struct
{
    float frequency_hz;
    float amplitude_code;
    float phase_rad;
    SignalWaveType type;
    uint32_t source_candidate_rank;
} SignalJointComponent;

/* E6 四模型比较结果；残差均为相对输入 RMS 的归一化数值。 */
typedef struct
{
    uint32_t sample_rate_hz;
    uint32_t sample_count;
    uint32_t evaluated_pairs;
    uint32_t evaluated_models;
    SignalJointComponent components[SIGNAL_JOINT_COMPONENT_COUNT];
    float input_rms_code;
    float model_rms_code;
    float residual_rms_code;
    float best_residual;
    float second_residual;
    float residual_gap;
    float confidence;
    uint32_t processing_time_us;
    bool valid;
} SignalJointResult;

extern volatile SignalJointResult g_signal_joint_result;

void SignalJoint_ResetResult(void);

bool SignalJoint_Analyze(const float *unwindowed_samples,
                         uint32_t sample_count,
                         const SignalFftResult *fft_result,
                         SignalJointResult *result);

/*@brief:按已知频率与波形类型用单次双频I/Q相关估计幅值和相位。
 *@param:unwindowed_samples  已去直流但未加窗的输入帧。
 *@param:sample_count        输入采样点数。
 *@param:sample_rate_hz      实际采样率，单位 Hz。
 *@param:seed_components     两个分量的频率与类型种子。
 *@param:result              输出跟踪结果。
 *@return:两个分量的幅值和相位均有效时返回 true。
 */
bool SignalJoint_TrackKnown(
    const float *unwindowed_samples,
    uint32_t sample_count,
    uint32_t sample_rate_hz,
    const SignalJointComponent seed_components[SIGNAL_JOINT_COMPONENT_COUNT],
    SignalJointResult *result);

void SignalJoint_PublishResult(const SignalJointResult *result,
                               uint32_t processing_time_us);

#ifdef __cplusplus
}
#endif

#endif /* APP_SIGNAL_DETECT_SIGNAL_JOINT_MODEL_H_ */
