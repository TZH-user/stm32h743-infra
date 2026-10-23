#ifndef APP_RTOS_APP_OUTPUT_TASK_H_
#define APP_RTOS_APP_OUTPUT_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "app_phase_lock.h"
#include "signal_dual_sine.h"
#include "signal_fft.h"
#include "signal_harmonic.h"
#include "signal_joint_model.h"
#include "signal_vote.h"

/**
 * The VCP carries exactly one format at a time.
 *
 * LOG:      human-readable text only.
 * ADC_WAVE: complete frames use "sample_index,adc_code" text rows.
 * FFT_WAVE: VOFA+ FireWater single-channel integer FFT values only.
 */
typedef enum
{
    APP_OUTPUT_MODE_OFF = 0,
    APP_OUTPUT_MODE_LOG = 1,
    APP_OUTPUT_MODE_ADC_WAVE = 2,
    APP_OUTPUT_MODE_FFT_WAVE = 3
} AppOutputMode;

typedef enum
{
    APP_OUTPUT_FREQUENCY_UNIT_HZ = 0,
    APP_OUTPUT_FREQUENCY_UNIT_KHZ = 1
} AppOutputFrequencyUnit;

typedef struct
{
    AppOutputMode requested_mode;
    AppOutputMode active_mode;
    uint32_t mode_switches;
    uint32_t dropped_log_messages;
    uint32_t log_messages_peak;
    uint32_t dropped_stream_packets;
    uint32_t transport_retries;
    uint32_t transport_errors;
    uint32_t stream_packets_used;
    uint32_t stream_packets_peak;
    uint32_t stream_packets_capacity;
    uint32_t stream_packets_sent;
    uint32_t stream_points_sent;
} AppOutputStats;

/** Existing CubeMX PrintfTask entry; it now owns all runtime VCP output. */
void StartPrintfTask(void *argument);

/** Request a VCP format switch. The output task applies it after draining DMA. */
bool AppOutput_RequestMode(AppOutputMode mode);

/** Return the currently active mode (not merely the requested mode). */
AppOutputMode AppOutput_GetMode(void);

/** Return the latest requested mode. */
AppOutputMode AppOutput_GetRequestedMode(void);

/** Human-readable mode name for UI/status pages. */
const char *AppOutput_ModeString(AppOutputMode mode);

/**
 * Queue one log line from task context. It is accepted only while LOG mode
 * is active. The queue is non-blocking; a full queue increments
 * dropped_log_messages. Do not call this formatting API from an ISR.
 */
#if defined(__GNUC__)
int AppOutput_Logf(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
#else
int AppOutput_Logf(const char *format, ...);
#endif

/**
 * 提交一次正式 FFT 候选结果。MainTask 只复制快照，文本格式化与串口发送
 * 均由 PrintfTask 完成，避免占用 MainTask 的有限栈空间。
 */
bool AppOutput_PublishSignalResult(const SignalFftResult *result);

/** 提交一次 E4 双正弦模型结果，由 PrintfTask 格式化并发送。 */
bool AppOutput_PublishDualSineResult(const SignalDualSineResult *result);

/** 提交一次 E5 波形分类结果，由 PrintfTask 格式化并发送。 */
bool AppOutput_PublishHarmonicResult(const SignalHarmonicResult *result);

/** 提交一次 E6 联合模型结果，由 PrintfTask 格式化并发送。 */
bool AppOutput_PublishJointResult(const SignalJointResult *result);

/** 提交一次 E7 多帧投票结果，由 PrintfTask 格式化并发送。 */
bool AppOutput_PublishVoteResult(const SignalVoteResult *result);

/** 提交一次锁相状态快照；重复提交时保留最新一份。 */
bool AppOutput_PublishPhaseLockStatus(const AppPhaseLockStatus *status);

/**
 * 完整发布一帧 ADC 原始码值，不做抽取。串口每行输出
 * “样本序号,ADC码值”，样本序号从 0 开始。
 *
 * 只有整帧能够放入固定环形队列时才接受，返回 true 表示整帧已入队。
 * 串口实际发送仍由 OutputTask 独占完成。
 */
bool AppOutput_PublishAdcFrame(const uint16_t *samples,
                               uint32_t sample_count);

/**
 * Publish a continuous ADC stream to the output task.
 *
 * This function performs phase-continuous integer decimation to the configured
 * target point rate. It does no UART access and is intended for AdcTask only.
 */
void AppOutput_PublishAdcSamples(const uint16_t *samples,
                                 uint32_t sample_count,
                                 uint32_t sample_rate_hz);

/**
 * Publish one FFT or other analysis series in application-defined integer
 * units. A practical FFT convention is dB x 100, e.g. -4237 means
 * -42.37 dB. It is accepted only in FFT_WAVE mode and is all-or-nothing.
 * One analysis task must be the sole producer of this API.
 */
bool AppOutput_PublishFftValues(const int32_t *values,
                                uint32_t value_count);

/**
 * 发布带频率列的频谱。每行格式为“频率数值,幅度值”。
 * kHz 模式固定保留三位小数，例如 15.000,-3421，不附加单位字符串。
 */
bool AppOutput_PublishFftBins(const int32_t *values,
                              uint32_t value_count,
                              uint16_t start_bin,
                              uint32_t sample_rate_hz,
                              uint16_t fft_size,
                              AppOutputFrequencyUnit frequency_unit);

/** Read output-mode and drop counters. */
void AppOutput_GetStats(AppOutputStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* APP_RTOS_APP_OUTPUT_TASK_H_ */
