#ifndef APP_SIGNAL_DETECT_SIGNAL_DETECT_H_
#define APP_SIGNAL_DETECT_SIGNAL_DETECT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "signal_detect_cfg.h"

typedef struct
{
    uint32_t sample_count;
    uint16_t raw_min;
    uint16_t raw_max;
    float raw_mean;
    float raw_rms;
    float ac_rms;
    uint32_t clip_count;
    uint32_t zero_count;
    uint32_t repeated_sample_count;
    uint32_t longest_repeated_run;
    bool valid;
} SignalFrameStats;

typedef struct
{
    uint32_t requested_sample_rate_hz;
    uint32_t actual_sample_rate_hz;
    uint32_t target_frames;
    uint32_t attempted_frames;
    uint32_t valid_frames;

    uint32_t sample_rate_errors;
    uint32_t start_errors;
    uint32_t wait_timeouts;
    uint32_t acquire_errors;
    uint32_t stop_errors;
    uint32_t frame_errors;

    uint32_t dma_completed_frames;
    uint32_t overwritten_frames;
    uint32_t notification_overflows;
    uint32_t dma_errors;

    uint16_t observed_min;
    uint16_t observed_max;
    float mean_of_frame_means;
    float mean_raw_rms;
    float mean_ac_rms;
    uint32_t total_clip_samples;
    uint32_t total_repeated_samples;
    uint32_t longest_repeated_run;
    uint32_t passed;
} SignalRateGateResult;

typedef struct
{
    uint32_t version;
    uint32_t complete;
    uint32_t rate_count;
    uint32_t passed_rate_count;
    uint32_t total_attempted_frames;
    uint32_t total_valid_frames;
    uint32_t elapsed_ms;
    SignalFrameStats last_frame;
    SignalRateGateResult rates[SIGNAL_DETECT_GATE_RATE_COUNT];
} SignalAcquisitionGateReport;

/* GDB 读取此变量即可取得三档采样率的完整门禁结果。 */
extern volatile SignalAcquisitionGateReport
    g_signal_acquisition_gate_report;

/** @brief 清空报告并写入三档门禁的固定目标参数。 */
void SignalDetect_ResetGateReport(void);

/** @brief 分析一帧只读 ADC 数据，不改变 DMA 缓冲区。 */
bool SignalDetect_AnalyzeFrame(const uint16_t *samples,
                               uint32_t count,
                               SignalFrameStats *stats);

/** @brief 把一帧统计量并入指定采样率的汇总结果。 */
void SignalDetect_AccumulateFrame(uint32_t rate_index,
                                  const SignalFrameStats *stats);

/** @brief 根据采集错误和有效帧数完成单档门禁判定。 */
void SignalDetect_FinalizeRate(uint32_t rate_index);

/** @brief 完成整份报告并记录总耗时。 */
void SignalDetect_FinalizeGate(uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_SIGNAL_DETECT_SIGNAL_DETECT_H_ */
