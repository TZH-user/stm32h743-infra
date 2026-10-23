#ifndef APP_CONFIG_AD9226_CONFIG_H_
#define APP_CONFIG_AD9226_CONFIG_H_

#include <stdint.h>

/* AD9226 驱动与硬件采样约束（UTF-8）；运行参数由单次分析接口传入。 */

/* 一个 DMA 目标缓冲区保存一帧完整采样，必须覆盖最大 FFT 点数。 */
#ifndef AD9226_FRAME_SAMPLES
#define AD9226_FRAME_SAMPLES                   4096U
#endif

/* 当前工程 APB1 = 120 MHz，TIM2 内核时钟 = 240 MHz。 */
#ifndef AD9226_TIM2_KERNEL_CLOCK_HZ
#define AD9226_TIM2_KERNEL_CLOCK_HZ            240000000UL
#endif

/* AD9226 输出流水线存在 7 个时钟周期延迟，启动时额外预送 8 个采样时钟。 */
#ifndef AD9226_PRIME_CLOCK_CYCLES
#define AD9226_PRIME_CLOCK_CYCLES              8U
#endif

/* TIM2_CH2 在每个采样周期的 3/4 相位读取 GPIOB。 */
#ifndef AD9226_DMA_SAMPLE_PHASE_NUM
#define AD9226_DMA_SAMPLE_PHASE_NUM            3U
#endif

#ifndef AD9226_DMA_SAMPLE_PHASE_DEN
#define AD9226_DMA_SAMPLE_PHASE_DEN            4U
#endif

/* 软件允许的最大采样率，用于限制运行时参数和分阶段压力测试。 */
#ifndef AD9226_MAX_SAMPLE_RATE_HZ
#define AD9226_MAX_SAMPLE_RATE_HZ              10000000UL
#endif

#endif /* APP_CONFIG_AD9226_CONFIG_H_ */
