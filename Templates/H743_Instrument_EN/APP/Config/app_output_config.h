#ifndef APP_CONFIG_APP_OUTPUT_CONFIG_H_
#define APP_CONFIG_APP_OUTPUT_CONFIG_H_

#include <stdint.h>

/* 信号识别日志级别；正式固件默认只输出 E7 最终结果（UTF-8）。 */
typedef enum
{
    SIGNAL_LOG_NONE = 0,
    SIGNAL_LOG_RESULT,
    SIGNAL_LOG_CANDIDATE,
    SIGNAL_LOG_MODEL,
    SIGNAL_LOG_SPECTRUM
} SignalLogLevel;

/*
 * USART1/VOFA+ 编译期输出策略（UTF-8）。
 *
 * 本文件负责默认模式、缓存容量、分包和超时，不负责运行时强制锁定模式。
 * 运行时模式由 AppOutput_RequestMode() 切换；任一时刻 VCP 只允许一种
 * 数据格式，避免日志、ADC 波形和 FFT 频谱互相混入。
 *
 * 数值与 app_output_task.h 中的 AppOutputMode 保持一致：
 * 0 = 关闭，1 = 日志，2 = ADC 波形，3 = FFT 频谱。
 * E8 正式固件默认使用日志模式输出 E7 结果；需要一帧 ADC 波形时改为 2U。
 */
#ifndef APP_OUTPUT_DEFAULT_MODE
#define APP_OUTPUT_DEFAULT_MODE                    1U
#endif

/*
 * 0 = 不输出识别日志；1 = E7 最终结果；2 = 追加 E3 候选；
 * 3 = 再追加 E4～E6 模型诊断；4 预留给独占 FFT_WAVE 频谱模式。
 */
#ifndef APP_OUTPUT_SIGNAL_LOG_LEVEL
#define APP_OUTPUT_SIGNAL_LOG_LEVEL                1U
#endif

#if (APP_OUTPUT_SIGNAL_LOG_LEVEL > 4U)
#error "APP_OUTPUT_SIGNAL_LOG_LEVEL is invalid"
#endif

/* 连续 ADC 发布接口的目标输出点率；单帧接口始终完整发布，不使用此抽取参数。 */
#ifndef APP_OUTPUT_ADC_TARGET_POINTS_PER_SECOND
#define APP_OUTPUT_ADC_TARGET_POINTS_PER_SECOND    150000U
#endif

/* 一个 FireWater 队列包最多包含的整数点数。 */
#ifndef APP_OUTPUT_STREAM_CHUNK_POINTS
#define APP_OUTPUT_STREAM_CHUNK_POINTS             128U
#endif

/*
 * SPSC 数据包环形队列深度，其中一个槽位用于区分满和空。
 * 34 个槽位提供 33 × 128 = 4224 点容量，可完整容纳 4096 点 ADC 帧；
 * 同时兼容最多 2048 点的 FFT 频谱。
 */
#ifndef APP_OUTPUT_STREAM_QUEUE_DEPTH
#define APP_OUTPUT_STREAM_QUEUE_DEPTH              34U
#endif

/* 切换格式前等待 VCP DMA 旧数据发送完成，防止新旧格式混合。 */
#ifndef APP_OUTPUT_MODE_SWITCH_TIMEOUT_MS
#define APP_OUTPUT_MODE_SWITCH_TIMEOUT_MS          500U
#endif

/* 当前工程串口只允许单帧 ADC 波形，拒绝切换到 FFT 串口格式。 */
#ifndef APP_OUTPUT_ENABLE_FFT_WAVE
#define APP_OUTPUT_ENABLE_FFT_WAVE                 0U
#endif

#if (APP_OUTPUT_ENABLE_FFT_WAVE == 0U) \
    && (APP_OUTPUT_DEFAULT_MODE == 3U)
#error "FFT serial output is disabled but selected as the default mode"
#endif

/* 单次发布允许的最大 FFT/分析点数。 */
#ifndef APP_OUTPUT_FFT_MAX_POINTS
#define APP_OUTPUT_FFT_MAX_POINTS                  2048U
#endif

/* 必须与 CubeMX 中 PrintQueue 的单项大小 128 字节保持一致。 */
#ifndef APP_OUTPUT_LOG_MESSAGE_BYTES
#define APP_OUTPUT_LOG_MESSAGE_BYTES               128U
#endif

#endif /* APP_CONFIG_APP_OUTPUT_CONFIG_H_ */
