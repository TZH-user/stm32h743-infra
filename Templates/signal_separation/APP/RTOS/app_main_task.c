/**
  ******************************************************************************
  * @file    app_main_task.c
  * @brief   MainTask 采样门禁与 E7 八帧识别主流程（UTF-8）。
  ******************************************************************************
  */

#include "app_main_task.h"

#include "Types/RtosTypes.h"
#include "ad9226.h"
#include "app_analysis_config.h"
#include "app_calibration_service.h"
#include "app_config_contracts.h"
#include "app_output_config.h"
#include "app_output_task.h"
#include "app_phase_lock_config.h"
#include "app_phase_lock_task.h"
#include "app_ui_adapter.h"
#include "cmsis_os2.h"
#include "signal_detect.h"
#include "signal_dual_sine.h"
#include "signal_fft.h"
#include "signal_harmonic.h"
#include "signal_joint_model.h"
#include "signal_preprocess.h"
#include "signal_vote.h"
#include "signal_core_stm32_adapter.h"
#include "tim.h"

#include <string.h>



extern DMA_HandleTypeDef hdma_tim2_ch2;

static volatile bool s_start_requested = false;
static volatile bool s_stop_requested = false;
static volatile bool s_session_running = false;
static volatile bool s_initialization_busy = true;
static volatile uint32_t s_busy_start_requests = 0U;
static volatile uint32_t s_stop_requests = 0U;

typedef struct
{
    SignalFftResult fft;
    SignalDualSineResult dual;
    SignalHarmonicResult harmonic;
    SignalJointResult joint;
} AppMainFrameAnalysis;

static uint32_t app_main_elapsed_ms(uint32_t start_tick,
                                    uint32_t end_tick)
{
    uint32_t tick_frequency_hz = osKernelGetTickFreq();
    if (tick_frequency_hz == 0U)
    {
        return 0U;
    }

    uint32_t elapsed_ticks = end_tick - start_tick;
    return (uint32_t)(((uint64_t)elapsed_ticks * 1000ULL)
                      / tick_frequency_hz);
}

static uint32_t app_main_elapsed_us(uint32_t start_count,
                                    uint32_t end_count)
{
    uint32_t timer_frequency_hz = osKernelGetSysTimerFreq();
    if (timer_frequency_hz == 0U)
    {
        return 0U;
    }

    uint32_t elapsed_counts = end_count - start_count;
    return (uint32_t)(((uint64_t)elapsed_counts * 1000000ULL)
                      / timer_frequency_hz);
}

/*@brief:处理一帧已停止采集的数据，并返回 E7 可直接记录的错误码。 */
static SignalDetectError app_main_analyze_frame(
    const AD9226_Frame *frame,
    uint32_t sample_rate_hz,
    bool fft_initialized,
    AppMainFrameAnalysis *analysis,
    uint32_t *analysis_time_us)
{
    if ((frame == NULL) || (analysis == NULL)
        || (analysis_time_us == NULL) || (frame->samples == NULL)
        || (frame->sample_count != SIGNAL_PREPROCESS_SAMPLE_COUNT))
    {
        return SIGNAL_DETECT_ERR_FRAME_SIZE;
    }

    uint32_t total_start = osKernelGetSysTimerCount();
    SignalPreprocessInfo preprocess_info = {0};
    uint32_t preprocess_start = osKernelGetSysTimerCount();
    bool preprocess_valid = SignalPreprocess_Run(
        frame->samples,
        frame->sample_count,
        g_signal_preprocess_windowed,
        g_signal_preprocess_unwindowed,
        &preprocess_info);
    uint32_t preprocess_time_us = app_main_elapsed_us(
        preprocess_start,
        osKernelGetSysTimerCount());
    SignalPreprocess_RecordDiagnostics(&preprocess_info,
                                       preprocess_time_us);
    if (!preprocess_valid)
    {
        *analysis_time_us = app_main_elapsed_us(
            total_start, osKernelGetSysTimerCount());
        if (preprocess_info.rms < SIGNAL_PREPROCESS_MIN_RMS_CODE)
        {
            return SIGNAL_DETECT_ERR_LOW_AMPLITUDE;
        }
        if (preprocess_info.clip_ratio > SIGNAL_PREPROCESS_MAX_CLIP_RATIO)
        {
            return SIGNAL_DETECT_ERR_CLIPPING;
        }
        return SIGNAL_DETECT_ERR_INTERNAL;
    }

    SignalFftConfig fft_config = {0};
    SignalFft_LoadDefaultConfig(&fft_config, sample_rate_hz);
    uint32_t fft_start = osKernelGetSysTimerCount();
    bool fft_valid = fft_initialized && SignalFft_Run(
        g_signal_preprocess_windowed,
        &fft_config,
        &analysis->fft);
    uint32_t fft_time_us = app_main_elapsed_us(
        fft_start,
        osKernelGetSysTimerCount());
    SignalFft_RecordDiagnostics(fft_valid, fft_time_us);
    analysis->fft.processing_time_us = fft_time_us;
    if (!fft_valid)
    {
        *analysis_time_us = app_main_elapsed_us(
            total_start, osKernelGetSysTimerCount());
        return SIGNAL_DETECT_ERR_MODEL_AMBIGUOUS;
    }

    uint32_t dual_start = osKernelGetSysTimerCount();
    (void)SignalDualSine_Analyze(g_signal_preprocess_unwindowed,
                                frame->sample_count,
                                &analysis->fft,
                                &analysis->dual);
    uint32_t dual_time_us = app_main_elapsed_us(
        dual_start,
        osKernelGetSysTimerCount());
    SignalDualSine_PublishResult(&analysis->dual, dual_time_us);
    analysis->dual.processing_time_us = dual_time_us;

    uint32_t harmonic_start = osKernelGetSysTimerCount();
    (void)SignalHarmonic_Analyze(g_signal_preprocess_unwindowed,
                                frame->sample_count,
                                &analysis->fft,
                                &analysis->dual,
                                &analysis->harmonic);
    uint32_t harmonic_time_us = app_main_elapsed_us(
        harmonic_start,
        osKernelGetSysTimerCount());
    SignalHarmonic_PublishResult(&analysis->harmonic, harmonic_time_us);
    analysis->harmonic.processing_time_us = harmonic_time_us;

    uint32_t joint_start = osKernelGetSysTimerCount();
    (void)SignalJoint_Analyze(g_signal_preprocess_unwindowed,
                             frame->sample_count,
                             &analysis->fft,
                             &analysis->joint);
    uint32_t joint_time_us = app_main_elapsed_us(
        joint_start,
        osKernelGetSysTimerCount());
    SignalJoint_PublishResult(&analysis->joint, joint_time_us);
    analysis->joint.processing_time_us = joint_time_us;

    *analysis_time_us = app_main_elapsed_us(
        total_start, osKernelGetSysTimerCount());
    return analysis->joint.valid
        ? SIGNAL_DETECT_ERR_NONE
        : SIGNAL_DETECT_ERR_MODEL_AMBIGUOUS;
}

bool AppMain_RequestStart(void)
{
    if (AppCalibrationService_IsExclusive() || s_initialization_busy
        || s_session_running || s_start_requested)
    {
        ++s_busy_start_requests;
        return false;
    }

    s_start_requested = true;
    __DMB();
    return true;
}

bool AppMain_RequestStop(void)
{
    if (!s_session_running && !AppPhaseLockTask_IsRunning())
    {
        return false;
    }

    s_stop_requested = true;
    ++s_stop_requests;
    __DMB();
    return true;
}

bool AppMain_IsRunning(void)
{
    return s_initialization_busy || s_session_running || s_start_requested;
}

/*@brief:CubeMX 创建的 MainTask 入口，完成一次门禁并响应 RUN/STOP。
 *@param:argument  CubeMX 任务参数，当前未使用。
 *@return:无返回值；空闲时等待下一次 RUN 请求。
 */
void StartMainTask(void *argument)
{
    (void)argument;

    static const uint32_t gate_rates_hz[SIGNAL_DETECT_GATE_RATE_COUNT] = {
        SIGNAL_DETECT_GATE_RATE_0_HZ,
        SIGNAL_DETECT_GATE_RATE_1_HZ,
        SIGNAL_DETECT_GATE_RATE_2_HZ,
    };

    /* MainTask 独占复用这些工作区，避免长流程局部结构体叠加占用任务栈。 */
    static AD9226_Config adc_config;
    adc_config.timer = &htim2;
    adc_config.dma = &hdma_tim2_ch2;
    adc_config.frame_ready_semaphore = AdcFrameReadySemHandle;
    adc_config.timer_kernel_clock_hz = AD9226_TIM2_KERNEL_CLOCK_HZ;
    adc_config.initial_sample_rate_hz = APP_ANALYSIS_DEFAULT_SAMPLE_RATE_HZ;

    SignalDetect_ResetGateReport();
    bool signal_core_ready = SignalCoreStm32Adapter_Init();
    SignalPreprocess_Init();
    SignalPreprocess_ResetDiagnostics();
    SignalFft_ResetDiagnostics();
    SignalDualSine_ResetResult();
    SignalHarmonic_ResetResult();
    SignalJoint_ResetResult();
    bool fft_initialized = SignalFft_Init();
    uint32_t gate_start_tick = osKernelGetTickCount();
    uint32_t capture_timeout_ticks = (uint32_t)(
        (((uint64_t)SIGNAL_DETECT_CAPTURE_TIMEOUT_MS
          * osKernelGetTickFreq()) + 999ULL) / 1000ULL);
    bool initialized = signal_core_ready
        && (AD9226_Init(&adc_config) == AD9226_STATUS_OK);

    for (uint32_t rate_index = 0U;
         rate_index < SIGNAL_DETECT_GATE_RATE_COUNT;
         ++rate_index)
    {
        volatile SignalRateGateResult *rate =
            &g_signal_acquisition_gate_report.rates[rate_index];
        uint32_t actual_rate_hz = 0U;

        if (!initialized
            || (AD9226_SetSampleRate(gate_rates_hz[rate_index],
                                     &actual_rate_hz)
                != AD9226_STATUS_OK))
        {
            ++rate->sample_rate_errors;
            SignalDetect_FinalizeRate(rate_index);
            continue;
        }
        rate->actual_sample_rate_hz = actual_rate_hz;

        for (uint32_t frame_index = 0U;
             frame_index < SIGNAL_DETECT_GATE_FRAMES_PER_RATE;
             ++frame_index)
        {
            static AD9226_Frame frame;
            static AD9226_Stats adc_stats;
            static SignalFrameStats frame_stats;
            frame = (AD9226_Frame){0};
            adc_stats = (AD9226_Stats){0};
            frame_stats = (SignalFrameStats){0};
            ++rate->attempted_frames;

            /* 第1步：启动AD9226并等待一帧采集完成。 */
            bool started = (AD9226_Start() == AD9226_STATUS_OK);
            if (!started)
            {
                ++rate->start_errors;
                continue;
            }

            /*
             * 门禁只采一帧。等待期间用帧完成信号量做有界阻塞：
             * DMA 完成回调会立即释放信号量，第一帧到达后仍能及时停止；
             * 帧未到达时 MainTask 让出 CPU，避免 AboveNormal 优先级
             * 忙等饿死 Normal 优先级的 DisplayTask（LVGL/触摸）。
             */
            uint32_t capture_start_tick = osKernelGetTickCount();
            bool frame_acquired = false;
            while (!frame_acquired
                   && ((osKernelGetTickCount() - capture_start_tick)
                       < capture_timeout_ticks))
            {
                frame_acquired = AD9226_AcquireLatestFrame(&frame);
                if (!frame_acquired)
                {
                    (void)osSemaphoreAcquire(AdcFrameReadySemHandle, 1U);
                }
            }
            if (!frame_acquired)
            {
                ++rate->wait_timeouts;
            }

            /* 第2步：先停止AD9226，再读取稳定的DMA完成缓冲区。 */
            if (AD9226_Stop() != AD9226_STATUS_OK)
            {
                ++rate->stop_errors;
            }

            AD9226_GetStats(&adc_stats);
            rate->dma_completed_frames += adc_stats.completed_frames;
            rate->overwritten_frames += adc_stats.overwritten_frames;
            rate->notification_overflows +=
                adc_stats.notification_overflows;
            rate->dma_errors += adc_stats.dma_errors;

            if (!frame_acquired)
            {
                continue;
            }

            if (!SignalDetect_AnalyzeFrame(frame.samples,
                                           frame.sample_count,
                                           &frame_stats))
            {
                ++rate->frame_errors;
                continue;
            }
            SignalDetect_AccumulateFrame(rate_index, &frame_stats);

            /* E2 只处理最高采样率的完整帧，采集已停止，不与 DMA 并发。 */
            if (rate_index == (SIGNAL_DETECT_GATE_RATE_COUNT - 1U))
            {
                static SignalPreprocessInfo preprocess_info;
                preprocess_info = (SignalPreprocessInfo){0};
                uint32_t preprocess_start = osKernelGetSysTimerCount();
                (void)SignalPreprocess_Run(
                    frame.samples,
                    frame.sample_count,
                    g_signal_preprocess_windowed,
                    g_signal_preprocess_unwindowed,
                    &preprocess_info);
                uint32_t preprocess_time_us = app_main_elapsed_us(
                    preprocess_start,
                    osKernelGetSysTimerCount());
                SignalPreprocess_RecordDiagnostics(&preprocess_info,
                                                   preprocess_time_us);
            }

            /* 每帧停止后让出一个 tick，避免门禁期间长期饿死 UI 任务。 */
            osDelay(1U);
        }

        SignalDetect_FinalizeRate(rate_index);
    }

    SignalDetect_FinalizeGate(app_main_elapsed_ms(
        gate_start_tick,
        osKernelGetTickCount()));
    s_initialization_busy = false;
    __DMB();

    uint32_t session_rate_hz =
        g_signal_acquisition_gate_report
            .rates[SIGNAL_DETECT_GATE_RATE_COUNT - 1U]
            .actual_sample_rate_hz;

#if (APP_ANALYSIS_RUN_ON_BOOT != 0U)
    uint32_t boot_sessions_remaining = APP_ANALYSIS_BOOT_SESSION_COUNT;
    s_start_requested = true;
#else
    uint32_t boot_sessions_remaining = 0U;
#endif

    for (;;)
    {
        if (AppCalibrationService_IsExclusive())
        {
            s_start_requested = false;
            if (s_session_running)
            {
                s_stop_requested = true;
            }
            __DMB();
        }

        if (s_stop_requested && !s_session_running)
        {
            (void)AppPhaseLockTask_Stop(
                APP_PHASE_LOCK_STOP_TIMEOUT_TICKS);
            s_stop_requested = false;
            __DMB();
        }

        if (!s_start_requested)
        {
            osDelay(20U);
            continue;
        }

        /* 清空上一轮，只执行一次“停止锁相 -> 采集分析 -> 请求跟踪输出”。 */
        (void)AppPhaseLockTask_Stop(
            APP_PHASE_LOCK_STOP_TIMEOUT_TICKS);
        s_start_requested = false;
        s_stop_requested = false;
        s_busy_start_requests = 0U;
        s_stop_requests = 0U;
        s_session_running = true;
        __DMB();

        SignalPreprocess_ResetDiagnostics();
        SignalFft_ResetDiagnostics();
        SignalDualSine_ResetResult();
        SignalHarmonic_ResetResult();
        SignalJoint_ResetResult();
        SignalVote_Begin();

        static AD9226_Frame output_frame;
        output_frame = (AD9226_Frame){0};
        bool output_frame_valid = false;
        bool output_analysis_present = false;
        static AppMainFrameAnalysis output_analysis;
        static SignalVoteResult vote_result;
        memset(&output_analysis, 0, sizeof(output_analysis));
        memset(&vote_result, 0, sizeof(vote_result));
        uint32_t session_start_tick = osKernelGetTickCount();
        uint32_t actual_rate_hz = 0U;
        bool rate_ready = initialized
            && (AD9226_SetSampleRate(SIGNAL_DETECT_GATE_RATE_2_HZ,
                                     &actual_rate_hz)
                == AD9226_STATUS_OK);
        if (rate_ready)
        {
            session_rate_hz = actual_rate_hz;
        }

        for (uint32_t frame_index = 0U;
             frame_index < SIGNAL_ANALYSIS_FRAME_COUNT;
             ++frame_index)
        {
            if (s_stop_requested)
            {
                break;
            }
            if (!rate_ready)
            {
                SignalVote_AddInvalidFrame(SIGNAL_DETECT_ERR_INTERNAL);
                continue;
            }

            static AD9226_Frame frame;
            frame = (AD9226_Frame){0};
            SignalVote_SetState(SIGNAL_DETECT_WAIT_FRAME);

            /* 第1步：启动 AD9226，只等待一帧。 */
            if (AD9226_Start() != AD9226_STATUS_OK)
            {
                SignalVote_AddInvalidFrame(SIGNAL_DETECT_ERR_INTERNAL);
                continue;
            }

            uint32_t capture_start_tick = osKernelGetTickCount();
            bool frame_acquired = false;
            while (!frame_acquired && !s_stop_requested
                   && ((osKernelGetTickCount() - capture_start_tick)
                       < capture_timeout_ticks))
            {
                frame_acquired = AD9226_AcquireLatestFrame(&frame);
                if (!frame_acquired)
                {
                    /* 有界等待帧信号量，正式分析同样不允许高优先级忙等。 */
                    (void)osSemaphoreAcquire(AdcFrameReadySemHandle, 1U);
                }
            }

            /* 第2步：无论本帧是否成功，都先停止 AD9226。 */
            bool stopped = (AD9226_Stop() == AD9226_STATUS_OK);
            if (s_stop_requested)
            {
                break;
            }
            if (!frame_acquired)
            {
                SignalVote_AddInvalidFrame(
                    SIGNAL_DETECT_ERR_CAPTURE_TIMEOUT);
                continue;
            }
            if (!stopped)
            {
                SignalVote_AddInvalidFrame(SIGNAL_DETECT_ERR_INTERNAL);
                continue;
            }

            /* 即使分析无效也保留最后一帧，使 UI 能显示 E7 错误状态。 */
            output_frame = frame;
            output_frame_valid = true;

            SignalVote_SetState(SIGNAL_DETECT_PROCESS_FRAME);
            static AppMainFrameAnalysis frame_analysis;
            memset(&frame_analysis, 0, sizeof(frame_analysis));
            uint32_t analysis_time_us = 0U;
            SignalDetectError frame_error = app_main_analyze_frame(
                &frame,
                session_rate_hz,
                fft_initialized,
                &frame_analysis,
                &analysis_time_us);

            /*
             * 无论联合模型是否通过，都保留末帧诊断结果。这样串口和 UI
             * 能区分“FFT 候选不足”与“模型残差/置信度不通过”。
             */
            output_analysis = frame_analysis;
            output_analysis_present = true;

            SignalVote_SetState(SIGNAL_DETECT_COLLECT_RESULT);
            if (frame_error == SIGNAL_DETECT_ERR_NONE)
            {
                SignalVote_AddFrame(&frame_analysis.joint,
                                    analysis_time_us);
            }
            else
            {
                SignalVote_AddInvalidFrame(frame_error);
            }

            osDelay(1U);
        }

        uint32_t session_time_ms = app_main_elapsed_ms(
            session_start_tick, osKernelGetTickCount());
        (void)SignalVote_Finalize(session_time_ms,
                                  s_busy_start_requests,
                                  s_stop_requests,
                                  &vote_result);

        if (vote_result.valid)
        {
            (void)AppPhaseLockTask_RequestStart(&vote_result);
        }

        /* 第3步：只发布代表帧和一次 E7 稳定结果。 */
        if (output_frame_valid)
        {
            (void)AppUiAdapter_SubmitAdcFrame(
                output_frame.samples,
                output_frame.sample_count,
                session_rate_hz,
                &output_analysis.fft,
                &output_analysis.dual,
                &output_analysis.harmonic,
                &output_analysis.joint,
                &vote_result);

            SignalLogLevel log_level =
                (SignalLogLevel)APP_OUTPUT_SIGNAL_LOG_LEVEL;
            if ((AppOutput_GetMode() == APP_OUTPUT_MODE_LOG)
                && output_analysis_present
                && (log_level >= SIGNAL_LOG_CANDIDATE))
            {
                (void)AppOutput_PublishSignalResult(&output_analysis.fft);
                if (log_level >= SIGNAL_LOG_MODEL)
                {
                    (void)AppOutput_PublishDualSineResult(
                        &output_analysis.dual);
                    (void)AppOutput_PublishHarmonicResult(
                        &output_analysis.harmonic);
                    (void)AppOutput_PublishJointResult(
                        &output_analysis.joint);
                }
            }
            else if (AppOutput_GetMode() == APP_OUTPUT_MODE_ADC_WAVE)
            {
                (void)AppOutput_PublishAdcFrame(output_frame.samples,
                                                output_frame.sample_count);
            }
        }
        if ((AppOutput_GetMode() == APP_OUTPUT_MODE_LOG)
            && (APP_OUTPUT_SIGNAL_LOG_LEVEL >= SIGNAL_LOG_RESULT))
        {
            (void)AppOutput_PublishVoteResult(&vote_result);
        }

        if (boot_sessions_remaining > 0U)
        {
            --boot_sessions_remaining;
            if (boot_sessions_remaining > 0U)
            {
                s_start_requested = true;
            }
        }

        s_session_running = false;
        s_stop_requested = false;
        __DMB();
    }
}
