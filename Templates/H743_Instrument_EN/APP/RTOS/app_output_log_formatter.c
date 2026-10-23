/**
  ******************************************************************************
  * @file    app_output_log_formatter.c
  * @brief   E3-E7 and phase-lock text formatting for PrintfTask.
  ******************************************************************************
  */

#include "app_output_task_internal.h"

#include "Types/RtosTypes.h"
#include "app_analysis_config.h"
#include "app_dac_output.h"
#include "app_dac_output_config.h"
#include "cmsis_os2.h"

#include <string.h>

/*@brief:在 PrintfTask 栈上将正式候选结果格式化为可读文本。 */
static void app_output_emit_pending_signal_result(void)
{
    if (!g_app_output_context.signal_result_pending)
    {
        return;
    }

    const SignalFftResult *result = &g_app_output_context.pending_signal_result;
    (void)AppOutput_Logf(
        "E3_RESULT,VALID=%u,COUNT=%lu,FS=%lu,FFT_US=%lu,RFFT_US=%lu,CAND_US=%lu\n",
        result->valid ? 1U : 0U,
        (unsigned long)result->selected_candidate_count,
        (unsigned long)result->sample_rate_hz,
        (unsigned long)result->processing_time_us,
        (unsigned long)result->transform_time_us,
        (unsigned long)result->candidate_time_us);

    for (uint32_t index = 0U;
         index < result->selected_candidate_count;
         ++index)
    {
        const SignalFrequencyCandidate *candidate = &result->selected[index];
        uint32_t frequency_hz = (uint32_t)(
            candidate->measured_frequency_hz + 0.5f);
        uint32_t amplitude_q100 = (uint32_t)(
            candidate->amplitude_code * 100.0f + 0.5f);
        int32_t snr_q100 = (int32_t)(candidate->snr_db * 100.0f
                                    + ((candidate->snr_db >= 0.0f)
                                       ? 0.5f : -0.5f));
        bool snr_negative = snr_q100 < 0;
        uint32_t snr_absolute = snr_negative
            ? (uint32_t)(-snr_q100)
            : (uint32_t)snr_q100;

#if (APP_ANALYSIS_DEFAULT_FREQUENCY_UNIT == 1U)
        (void)AppOutput_Logf(
            "E3_CAND,RANK=%lu,FREQ=%lu.%03lu,AMP=%lu.%02lu,SNR=%s%lu.%02lu\n",
            (unsigned long)candidate->rank,
            (unsigned long)(frequency_hz / 1000U),
            (unsigned long)(frequency_hz % 1000U),
            (unsigned long)(amplitude_q100 / 100U),
            (unsigned long)(amplitude_q100 % 100U),
            snr_negative ? "-" : "",
            (unsigned long)(snr_absolute / 100U),
            (unsigned long)(snr_absolute % 100U));
#else
        (void)AppOutput_Logf(
            "E3_CAND,RANK=%lu,FREQ=%lu,AMP=%lu.%02lu,SNR=%s%lu.%02lu\n",
            (unsigned long)candidate->rank,
            (unsigned long)frequency_hz,
            (unsigned long)(amplitude_q100 / 100U),
            (unsigned long)(amplitude_q100 % 100U),
            snr_negative ? "-" : "",
            (unsigned long)(snr_absolute / 100U),
            (unsigned long)(snr_absolute % 100U));
#endif
    }

    (void)AppOutput_Logf("E3_END\n");
    g_app_output_context.signal_result_pending = false;
    __DMB();
}

bool AppOutput_PublishSignalResult(const SignalFftResult *result)
{
    if ((result == NULL) || (g_app_output_context.active_mode != APP_OUTPUT_MODE_LOG)
        || g_app_output_context.signal_result_pending)
    {
        return false;
    }

    g_app_output_context.pending_signal_result = *result;
    __DMB();
    g_app_output_context.signal_result_pending = true;
    return true;
}

/*@brief:在 PrintfTask 栈上输出 E4 双正弦模型，频率默认按 kHz 数值显示。 */
static void app_output_emit_pending_dual_sine_result(void)
{
    if (!g_app_output_context.dual_sine_result_pending)
    {
        return;
    }

    const SignalDualSineResult *result =
        &g_app_output_context.pending_dual_sine_result;
    uint32_t residual_q100000 = (uint32_t)(
        result->normalized_residual * 100000.0f + 0.5f);
    uint32_t input_rms_q100 = (uint32_t)(
        result->input_rms_code * 100.0f + 0.5f);
    uint32_t model_rms_q100 = (uint32_t)(
        result->model_rms_code * 100.0f + 0.5f);
    uint32_t error_rms_q100 = (uint32_t)(
        result->residual_rms_code * 100.0f + 0.5f);

    (void)AppOutput_Logf(
        "E4_RESULT,VALID=%u,COUNT=%u,PAIRS=%lu,RESIDUAL=%lu.%05lu,E4_US=%lu\n",
        result->valid ? 1U : 0U,
        result->valid ? SIGNAL_DUAL_SINE_TONE_COUNT : 0U,
        (unsigned long)result->evaluated_pairs,
        (unsigned long)(residual_q100000 / 100000U),
        (unsigned long)(residual_q100000 % 100000U),
        (unsigned long)result->processing_time_us);
    (void)AppOutput_Logf(
        "E4_RMS,INPUT=%lu.%02lu,MODEL=%lu.%02lu,ERROR=%lu.%02lu\n",
        (unsigned long)(input_rms_q100 / 100U),
        (unsigned long)(input_rms_q100 % 100U),
        (unsigned long)(model_rms_q100 / 100U),
        (unsigned long)(model_rms_q100 % 100U),
        (unsigned long)(error_rms_q100 / 100U),
        (unsigned long)(error_rms_q100 % 100U));

    uint32_t tone_count = result->valid
        ? SIGNAL_DUAL_SINE_TONE_COUNT : 0U;
    for (uint32_t index = 0U; index < tone_count; ++index)
    {
        const SignalDualSineTone *tone = &result->tones[index];
        uint32_t frequency_hz = (uint32_t)(tone->frequency_hz + 0.5f);
        uint32_t amplitude_q100 = (uint32_t)(
            tone->estimate.amplitude * 100.0f + 0.5f);
        int32_t phase_q100 = (int32_t)(
            tone->estimate.phase_rad * 5729.57795f
            + ((tone->estimate.phase_rad >= 0.0f) ? 0.5f : -0.5f));
        bool phase_negative = phase_q100 < 0;
        uint32_t phase_absolute = phase_negative
            ? (uint32_t)(-phase_q100)
            : (uint32_t)phase_q100;

#if (APP_ANALYSIS_DEFAULT_FREQUENCY_UNIT == 1U)
        (void)AppOutput_Logf(
            "E4_TONE,RANK=%lu,FREQ=%lu.%03lu,AMP=%lu.%02lu,PHASE=%s%lu.%02lu\n",
            (unsigned long)(index + 1U),
            (unsigned long)(frequency_hz / 1000U),
            (unsigned long)(frequency_hz % 1000U),
            (unsigned long)(amplitude_q100 / 100U),
            (unsigned long)(amplitude_q100 % 100U),
            phase_negative ? "-" : "",
            (unsigned long)(phase_absolute / 100U),
            (unsigned long)(phase_absolute % 100U));
#else
        (void)AppOutput_Logf(
            "E4_TONE,RANK=%lu,FREQ=%lu,AMP=%lu.%02lu,PHASE=%s%lu.%02lu\n",
            (unsigned long)(index + 1U),
            (unsigned long)frequency_hz,
            (unsigned long)(amplitude_q100 / 100U),
            (unsigned long)(amplitude_q100 % 100U),
            phase_negative ? "-" : "",
            (unsigned long)(phase_absolute / 100U),
            (unsigned long)(phase_absolute % 100U));
#endif
    }

    if (result->valid)
    {
        int32_t relative_q100 = (int32_t)(
            result->relative_phase_rad * 5729.57795f
            + ((result->relative_phase_rad >= 0.0f) ? 0.5f : -0.5f));
        bool relative_negative = relative_q100 < 0;
        uint32_t relative_absolute = relative_negative
            ? (uint32_t)(-relative_q100)
            : (uint32_t)relative_q100;
        (void)AppOutput_Logf("E4_REL_PHASE=%s%lu.%02lu\n",
                             relative_negative ? "-" : "",
                             (unsigned long)(relative_absolute / 100U),
                             (unsigned long)(relative_absolute % 100U));
    }
    (void)AppOutput_Logf("E4_END\n");
    g_app_output_context.dual_sine_result_pending = false;
    __DMB();
}

bool AppOutput_PublishDualSineResult(const SignalDualSineResult *result)
{
    if ((result == NULL) || (g_app_output_context.active_mode != APP_OUTPUT_MODE_LOG)
        || g_app_output_context.dual_sine_result_pending)
    {
        return false;
    }

    g_app_output_context.pending_dual_sine_result = *result;
    __DMB();
    g_app_output_context.dual_sine_result_pending = true;
    return true;
}

/*@brief:输出 E5 谐波比例、分类分数和最终波形类型。 */
static void app_output_emit_pending_harmonic_result(void)
{
    if (!g_app_output_context.harmonic_result_pending)
    {
        return;
    }

    const SignalHarmonicResult *result =
        &g_app_output_context.pending_harmonic_result;
    (void)AppOutput_Logf(
        "E5_RESULT,VALID=%u,COUNT=%lu,E5_US=%lu\n",
        result->valid ? 1U : 0U,
        (unsigned long)result->component_count,
        (unsigned long)result->processing_time_us);

    for (uint32_t index = 0U; index < result->component_count; ++index)
    {
        const SignalHarmonicComponent *component =
            &result->components[index];
        uint32_t frequency_hz = (uint32_t)(component->frequency_hz + 0.5f);
        uint32_t ratio3_q10000 = (uint32_t)(
            component->feature.a3 * 10000.0f
            / component->feature.a1 + 0.5f);
        uint32_t ratio5_q10000 = (uint32_t)(
            component->feature.a5 * 10000.0f
            / component->feature.a1 + 0.5f);
        uint32_t sine_q1000 = (uint32_t)(
            component->feature.sine_score * 1000.0f + 0.5f);
        uint32_t triangle_q1000 = (uint32_t)(
            component->feature.triangle_score * 1000.0f + 0.5f);
        uint32_t confidence_q1000 = (uint32_t)(
            component->confidence * 1000.0f + 0.5f);

#if (APP_ANALYSIS_DEFAULT_FREQUENCY_UNIT == 1U)
        (void)AppOutput_Logf(
            "E5_WAVE,RANK=%lu,FREQ=%lu.%03lu,TYPE=%s,A3_A1=%lu.%04lu,A5_A1=%lu.%04lu\n",
            (unsigned long)(index + 1U),
            (unsigned long)(frequency_hz / 1000U),
            (unsigned long)(frequency_hz % 1000U),
            SignalHarmonic_WaveTypeString(component->type),
            (unsigned long)(ratio3_q10000 / 10000U),
            (unsigned long)(ratio3_q10000 % 10000U),
            (unsigned long)(ratio5_q10000 / 10000U),
            (unsigned long)(ratio5_q10000 % 10000U));
#else
        (void)AppOutput_Logf(
            "E5_WAVE,RANK=%lu,FREQ=%lu,TYPE=%s,A3_A1=%lu.%04lu,A5_A1=%lu.%04lu\n",
            (unsigned long)(index + 1U),
            (unsigned long)frequency_hz,
            SignalHarmonic_WaveTypeString(component->type),
            (unsigned long)(ratio3_q10000 / 10000U),
            (unsigned long)(ratio3_q10000 % 10000U),
            (unsigned long)(ratio5_q10000 / 10000U),
            (unsigned long)(ratio5_q10000 % 10000U));
#endif
        (void)AppOutput_Logf(
            "E5_SCORE,RANK=%lu,SINE=%lu.%03lu,TRI=%lu.%03lu,CONF=%lu.%03lu\n",
            (unsigned long)(index + 1U),
            (unsigned long)(sine_q1000 / 1000U),
            (unsigned long)(sine_q1000 % 1000U),
            (unsigned long)(triangle_q1000 / 1000U),
            (unsigned long)(triangle_q1000 % 1000U),
            (unsigned long)(confidence_q1000 / 1000U),
            (unsigned long)(confidence_q1000 % 1000U));
    }

    (void)AppOutput_Logf("E5_END\n");
    g_app_output_context.harmonic_result_pending = false;
    __DMB();
}

bool AppOutput_PublishHarmonicResult(const SignalHarmonicResult *result)
{
    if ((result == NULL) || (g_app_output_context.active_mode != APP_OUTPUT_MODE_LOG)
        || g_app_output_context.harmonic_result_pending)
    {
        return false;
    }

    g_app_output_context.pending_harmonic_result = *result;
    __DMB();
    g_app_output_context.harmonic_result_pending = true;
    return true;
}

/*@brief:输出 E6 最优/次优残差、置信度和两路联合类型。 */
static void app_output_emit_pending_joint_result(void)
{
    if (!g_app_output_context.joint_result_pending)
    {
        return;
    }

    const SignalJointResult *result = &g_app_output_context.pending_joint_result;
    uint32_t best_q100000 = (uint32_t)(
        result->best_residual * 100000.0f + 0.5f);
    uint32_t second_q100000 = (uint32_t)(
        result->second_residual * 100000.0f + 0.5f);
    uint32_t gap_q100000 = (uint32_t)(
        result->residual_gap * 100000.0f + 0.5f);
    uint32_t confidence_q1000 = (uint32_t)(
        result->confidence * 1000.0f + 0.5f);

    (void)AppOutput_Logf(
        "E6_RESULT,VALID=%u,COUNT=%u,PAIRS=%lu,MODELS=%lu,E6_US=%lu\n",
        result->valid ? 1U : 0U,
        result->valid ? SIGNAL_JOINT_COMPONENT_COUNT : 0U,
        (unsigned long)result->evaluated_pairs,
        (unsigned long)result->evaluated_models,
        (unsigned long)result->processing_time_us);
    (void)AppOutput_Logf(
        "E6_MODEL,BEST=%lu.%05lu,SECOND=%lu.%05lu,GAP=%lu.%05lu,CONF=%lu.%03lu\n",
        (unsigned long)(best_q100000 / 100000U),
        (unsigned long)(best_q100000 % 100000U),
        (unsigned long)(second_q100000 / 100000U),
        (unsigned long)(second_q100000 % 100000U),
        (unsigned long)(gap_q100000 / 100000U),
        (unsigned long)(gap_q100000 % 100000U),
        (unsigned long)(confidence_q1000 / 1000U),
        (unsigned long)(confidence_q1000 % 1000U));

    if (result->evaluated_models > 0U)
    {
        for (uint32_t index = 0U;
             index < SIGNAL_JOINT_COMPONENT_COUNT;
             ++index)
        {
            const SignalJointComponent *component =
                &result->components[index];
            uint32_t frequency_hz = (uint32_t)(
                component->frequency_hz + 0.5f);
            uint32_t amplitude_q100 = (uint32_t)(
                component->amplitude_code * 100.0f + 0.5f);
            int32_t phase_q100 = (int32_t)(
                component->phase_rad * 5729.57795f
                + ((component->phase_rad >= 0.0f) ? 0.5f : -0.5f));
            bool phase_negative = phase_q100 < 0;
            uint32_t phase_absolute = phase_negative
                ? (uint32_t)(-phase_q100) : (uint32_t)phase_q100;

#if (APP_ANALYSIS_DEFAULT_FREQUENCY_UNIT == 1U)
            (void)AppOutput_Logf(
                "E6_COMPONENT,RANK=%lu,FREQ=%lu.%03lu,TYPE=%s,AMP=%lu.%02lu,PHASE=%s%lu.%02lu\n",
                (unsigned long)(index + 1U),
                (unsigned long)(frequency_hz / 1000U),
                (unsigned long)(frequency_hz % 1000U),
                SignalHarmonic_WaveTypeString(component->type),
                (unsigned long)(amplitude_q100 / 100U),
                (unsigned long)(amplitude_q100 % 100U),
                phase_negative ? "-" : "",
                (unsigned long)(phase_absolute / 100U),
                (unsigned long)(phase_absolute % 100U));
#else
            (void)AppOutput_Logf(
                "E6_COMPONENT,RANK=%lu,FREQ=%lu,TYPE=%s,AMP=%lu.%02lu,PHASE=%s%lu.%02lu\n",
                (unsigned long)(index + 1U),
                (unsigned long)frequency_hz,
                SignalHarmonic_WaveTypeString(component->type),
                (unsigned long)(amplitude_q100 / 100U),
                (unsigned long)(amplitude_q100 % 100U),
                phase_negative ? "-" : "",
                (unsigned long)(phase_absolute / 100U),
                (unsigned long)(phase_absolute % 100U));
#endif
        }
    }

    (void)AppOutput_Logf("E6_END\n");
    g_app_output_context.joint_result_pending = false;
    __DMB();
}

bool AppOutput_PublishJointResult(const SignalJointResult *result)
{
    if ((result == NULL) || (g_app_output_context.active_mode != APP_OUTPUT_MODE_LOG)
        || g_app_output_context.joint_result_pending)
    {
        return false;
    }

    g_app_output_context.pending_joint_result = *result;
    __DMB();
    g_app_output_context.joint_result_pending = true;
    return true;
}

/*@brief:输出 E7 状态路径、投票质量、两路稳定分类和最终摘要。 */
static void app_output_emit_pending_vote_result(void)
{
    if (!g_app_output_context.vote_result_pending)
    {
        return;
    }

    const SignalVoteResult *result = &g_app_output_context.pending_vote_result;
    const char *final_state = result->valid ? "FINISHED" : "ERROR";
    uint32_t residual_q100000 = (uint32_t)(
        result->median_residual * 100000.0f + 0.5f);
    uint32_t confidence_q1000 = (uint32_t)(
        result->total_confidence * 1000.0f + 0.5f);

    (void)AppOutput_Logf(
        "E7_STATE,PATH=IDLE>ARMED>WAIT_FRAME>PROCESS_FRAME>COLLECT_RESULT>FINALIZE>%s\n",
        final_state);
    (void)AppOutput_Logf(
        "E7_RESULT,VALID=%u,FRAMES=%lu,VALID_FRAMES=%lu,INVALID_FRAMES=%lu,MASK=0x%02lX,E7_MS=%lu\n",
        result->valid ? 1U : 0U,
        (unsigned long)result->processed_frames,
        (unsigned long)result->valid_frames,
        (unsigned long)result->invalid_frames,
        (unsigned long)result->valid_frame_mask,
        (unsigned long)result->session_time_ms);
    (void)AppOutput_Logf(
        "E7_QUALITY,RES=%lu.%05lu,CONF=%lu.%03lu,BUSY=%lu,STOP=%lu,REASONS=0x%lX,ERR=%s\n",
        (unsigned long)(residual_q100000 / 100000U),
        (unsigned long)(residual_q100000 % 100000U),
        (unsigned long)(confidence_q1000 / 1000U),
        (unsigned long)(confidence_q1000 % 1000U),
        (unsigned long)result->busy_start_requests,
        (unsigned long)result->stop_requests,
        (unsigned long)result->invalid_reason_mask,
        SignalVote_ErrorString(result->error));

    for (uint32_t index = 0U;
         index < SIGNAL_JOINT_COMPONENT_COUNT;
         ++index)
    {
        const SignalVoteComponent *component = &result->components[index];
        uint32_t frequency_hz = (uint32_t)(component->frequency_hz + 0.5f);
        uint32_t amplitude_q100 = (uint32_t)(
            component->amplitude_code * 100.0f + 0.5f);
#if (APP_ANALYSIS_DEFAULT_FREQUENCY_UNIT == 1U)
        (void)AppOutput_Logf(
            "E7_COMPONENT,RANK=%lu,FREQ=%lu.%03lu,TYPE=%s,AMP=%lu.%02lu,FREQ_VOTES=%lu,TYPE_VOTES=%lu\n",
            (unsigned long)(index + 1U),
            (unsigned long)(frequency_hz / 1000U),
            (unsigned long)(frequency_hz % 1000U),
            SignalHarmonic_WaveTypeString(component->type),
            (unsigned long)(amplitude_q100 / 100U),
            (unsigned long)(amplitude_q100 % 100U),
            (unsigned long)component->frequency_votes,
            (unsigned long)component->type_votes);
#else
        (void)AppOutput_Logf(
            "E7_COMPONENT,RANK=%lu,FREQ=%lu,TYPE=%s,AMP=%lu.%02lu,FREQ_VOTES=%lu,TYPE_VOTES=%lu\n",
            (unsigned long)(index + 1U),
            (unsigned long)frequency_hz,
            SignalHarmonic_WaveTypeString(component->type),
            (unsigned long)(amplitude_q100 / 100U),
            (unsigned long)(amplitude_q100 % 100U),
            (unsigned long)component->frequency_votes,
            (unsigned long)component->type_votes);
#endif
    }

    if (result->valid)
    {
        uint32_t first_hz = (uint32_t)(
            result->components[0].frequency_hz + 0.5f);
        uint32_t second_hz = (uint32_t)(
            result->components[1].frequency_hz + 0.5f);
#if (APP_ANALYSIS_DEFAULT_FREQUENCY_UNIT == 1U)
        (void)AppOutput_Logf(
            "E7_FINAL,FREQ1=%lu.%03lu,TYPE1=%s,FREQ2=%lu.%03lu,TYPE2=%s\n",
            (unsigned long)(first_hz / 1000U),
            (unsigned long)(first_hz % 1000U),
            SignalHarmonic_WaveTypeString(result->components[0].type),
            (unsigned long)(second_hz / 1000U),
            (unsigned long)(second_hz % 1000U),
            SignalHarmonic_WaveTypeString(result->components[1].type));
#else
        (void)AppOutput_Logf(
            "E7_FINAL,FREQ1=%lu,TYPE1=%s,FREQ2=%lu,TYPE2=%s\n",
            (unsigned long)first_hz,
            SignalHarmonic_WaveTypeString(result->components[0].type),
            (unsigned long)second_hz,
            SignalHarmonic_WaveTypeString(result->components[1].type));
#endif
    }
    else
    {
        (void)AppOutput_Logf("E7_FINAL,VALID=0,ERROR=%s\n",
                            SignalVote_ErrorString(result->error));
    }

    if (result->valid && g_app_dac_output_status.running)
    {
        (void)AppOutput_Logf(
            "DAC_RESULT,VALID=1,RUN=1,F1=%lu,TYPE1=%s,F2=%lu,TYPE2=%s,VPP_MV=%lu,ERR=%u\n",
            (unsigned long)(
                g_app_dac_output_status.frequency_hz[0] / 1000U),
            SignalHarmonic_WaveTypeString(
                g_app_dac_output_status.wave_type[0]),
            (unsigned long)(
                g_app_dac_output_status.frequency_hz[1] / 1000U),
            SignalHarmonic_WaveTypeString(
                g_app_dac_output_status.wave_type[1]),
            (unsigned long)APP_DAC_OUTPUT_VPP_MV,
            (unsigned int)g_app_dac_output_status.last_error);
    }
    else
    {
        (void)AppOutput_Logf(
            "DAC_RESULT,VALID=0,RUN=0,E7_VALID=%u,ERR=%u\n",
            result->valid ? 1U : 0U,
            (unsigned int)g_app_dac_output_status.last_error);
    }

    (void)AppOutput_Logf("E7_END\n");
    g_app_output_context.vote_result_pending = false;
    __DMB();
}

bool AppOutput_PublishVoteResult(const SignalVoteResult *result)
{
    if ((result == NULL) || (g_app_output_context.active_mode != APP_OUTPUT_MODE_LOG)
        || g_app_output_context.vote_result_pending)
    {
        return false;
    }

    g_app_output_context.pending_vote_result = *result;
    __DMB();
    g_app_output_context.vote_result_pending = true;
    return true;
}

static void app_output_emit_pending_phase_lock_status(void)
{
    AppPhaseLockStatus status;
    uint32_t primask = AppOutputInternal_IrqSave();
    status = g_app_output_context.pending_phase_lock_status;
    g_app_output_context.phase_lock_status_pending = false;
    AppOutputInternal_IrqRestore(primask);

    uint32_t residual_q100000 = (uint32_t)(
        status.residual * 100000.0f + 0.5f);
    (void)AppOutput_Logf(
        "PLL_STATE,RUN=%u,LOCK=%u,VALID=%lu,INVALID=%lu,RES=%lu.%05lu,US=%lu,ERR=%u\n",
        status.running ? 1U : 0U,
        status.all_locked ? 1U : 0U,
        (unsigned long)status.valid_frames,
        (unsigned long)status.invalid_frames,
        (unsigned long)(residual_q100000 / 100000U),
        (unsigned long)(residual_q100000 % 100000U),
        (unsigned long)status.processing_time_last_us,
        (unsigned int)status.last_error);

    for (uint32_t channel = 0U;
         channel < SIGNAL_JOINT_COMPONENT_COUNT;
         ++channel)
    {
        const AppPhaseLockChannelStatus *component =
            &status.channels[channel];
        uint32_t frequency_hz = (uint32_t)(
            component->tracked_frequency_hz + 0.5f);
        float phase_error_mdeg_f = component->phase_error_rad
            * 57295.7795131f;
        int32_t phase_error_mdeg = (int32_t)(
            phase_error_mdeg_f
            + ((phase_error_mdeg_f >= 0.0f) ? 0.5f : -0.5f));
        (void)AppOutput_Logf(
            "PLL_COMPONENT,RANK=%lu,FREQ=%lu.%03lu,TYPE=%s,ERR_MDEG=%ld,LOCK=%u\n",
            (unsigned long)(channel + 1U),
            (unsigned long)(frequency_hz / 1000U),
            (unsigned long)(frequency_hz % 1000U),
            SignalHarmonic_WaveTypeString(component->wave_type),
            (long)phase_error_mdeg,
            component->locked ? 1U : 0U);
    }

    (void)AppOutput_Logf(
        "PLL_DAC,RUN=%u,PHASE_CTRL=%u,PHASE_DEG=%u,RATIO=%u,HALF=%lu,FULL=%lu,OVERRUN=%lu,UNDERRUN=%lu,LAST_US=%lu,MAX_US=%lu,ERR=%u\n",
        g_app_dac_output_status.running ? 1U : 0U,
        g_app_dac_output_status.phase_control_active ? 1U : 0U,
        (unsigned int)g_app_dac_output_status.phase_difference_deg,
        (unsigned int)g_app_dac_output_status.harmonic_ratio,
        (unsigned long)g_app_dac_output_status.half_refill_count,
        (unsigned long)g_app_dac_output_status.full_refill_count,
        (unsigned long)g_app_dac_output_status.refill_overrun_count,
        (unsigned long)g_app_dac_output_status.underrun_count,
        (unsigned long)g_app_dac_output_status.refill_time_last_us,
        (unsigned long)g_app_dac_output_status.refill_time_max_us,
        (unsigned int)g_app_dac_output_status.last_error);

    if (g_app_dac_output_status.underrun_count != 0U)
    {
        (void)AppOutput_Logf(
            "PLL_DAC_FAULT,TICK_MS=%lu,NDTR=%lu,CR=0x%08lX,FCR=0x%08lX,LISR=0x%08lX,DMA_ERR=0x%08lX,DMA_STATE=%lu\n",
            (unsigned long)g_app_dac_output_status.underrun_tick_ms,
            (unsigned long)g_app_dac_output_status.underrun_dma_remaining,
            (unsigned long)g_app_dac_output_status.underrun_dma_cr,
            (unsigned long)g_app_dac_output_status.underrun_dma_fcr,
            (unsigned long)g_app_dac_output_status.underrun_dma_lisr,
            (unsigned long)g_app_dac_output_status.underrun_dma_error,
            (unsigned long)g_app_dac_output_status.underrun_dma_state);
    }
}

bool AppOutput_PublishPhaseLockStatus(const AppPhaseLockStatus *status)
{
    if ((status == NULL) || (g_app_output_context.active_mode != APP_OUTPUT_MODE_LOG))
    {
        return false;
    }

    uint32_t primask = AppOutputInternal_IrqSave();
    g_app_output_context.pending_phase_lock_status = *status;
    g_app_output_context.phase_lock_status_pending = true;
    AppOutputInternal_IrqRestore(primask);
    return true;
}

void AppOutputLogFormatter_EmitPending(void)
{
    if (g_app_output_context.signal_result_pending)
    {
        app_output_emit_pending_signal_result();
        return;
    }

    if (osMessageQueueGetCount(PrintQueueHandle) != 0U)
    {
        return;
    }

    if (g_app_output_context.dual_sine_result_pending)
    {
        app_output_emit_pending_dual_sine_result();
    }
    else if (g_app_output_context.harmonic_result_pending)
    {
        app_output_emit_pending_harmonic_result();
    }
    else if (g_app_output_context.joint_result_pending)
    {
        app_output_emit_pending_joint_result();
    }
    else if (g_app_output_context.vote_result_pending)
    {
        app_output_emit_pending_vote_result();
    }
    else if (g_app_output_context.phase_lock_status_pending)
    {
        app_output_emit_pending_phase_lock_status();
    }
}
