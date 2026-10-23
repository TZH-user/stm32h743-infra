#ifndef APP_DAC_OUTPUT_CONFIG_H_
#define APP_DAC_OUTPUT_CONFIG_H_

#include <stdint.h>

/*
 * 片内双 DAC 输出参数（UTF-8）。
 * TIM6 = 240 MHz / 150 = 1.6 MS/s。DMA 长缓冲给相位跟踪计算保留时间，
 * 每半缓冲约 10.24 ms；运行期由 Q32 相位累加器生成任意细分频率。
 */
#define APP_DAC_OUTPUT_SAMPLE_RATE_HZ          1600000UL
#define APP_DAC_OUTPUT_BUFFER_POINTS           32768U
#define APP_DAC_OUTPUT_HALF_POINTS             \
    (APP_DAC_OUTPUT_BUFFER_POINTS / 2U)
#define APP_DAC_OUTPUT_MIN_FREQUENCY_HZ        20000UL
#define APP_DAC_OUTPUT_MAX_FREQUENCY_HZ        100000UL
#define APP_DAC_OUTPUT_FREQUENCY_STEP_HZ       5000UL
#define APP_DAC_OUTPUT_VOTE_TOLERANCE_HZ       1000UL

/* 发挥项：B' 相对 A' 的初相位差，只接受 0..180 度的 5 度网格。 */
#define APP_DAC_OUTPUT_PHASE_MIN_DEG            0U
#define APP_DAC_OUTPUT_PHASE_MAX_DEG            180U
#define APP_DAC_OUTPUT_PHASE_STEP_DEG           5U

/* 默认按 VREF+=3.3 V 计算 0.8 Vpp；外部实际幅度以后续测量为准。 */
#define APP_DAC_OUTPUT_VREF_MV                 3300UL
#define APP_DAC_OUTPUT_VPP_MV                  800UL
#define APP_DAC_OUTPUT_CENTER_CODE             2048U
#define APP_DAC_OUTPUT_PEAK_CODE               \
    ((4095UL * APP_DAC_OUTPUT_VPP_MV) / (2UL * APP_DAC_OUTPUT_VREF_MV))

#endif /* APP_DAC_OUTPUT_CONFIG_H_ */
