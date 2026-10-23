#ifndef APP_SIGNAL_DETECT_SIGNAL_DETECT_CFG_H_
#define APP_SIGNAL_DETECT_SIGNAL_DETECT_CFG_H_

#include <stdint.h>

/* 信号识别 E1 采样质量门禁参数（UTF-8）。 */

#define SIGNAL_DETECT_GATE_RATE_COUNT              3U
#define SIGNAL_DETECT_GATE_FRAMES_PER_RATE         100U
#define SIGNAL_DETECT_EXPECTED_FRAME_SAMPLES       4096U
#define SIGNAL_DETECT_CAPTURE_TIMEOUT_MS           100U

/* E7 一次启动固定分析 8 帧，至少 5 帧有效才允许输出稳定结论。 */
#define SIGNAL_ANALYSIS_FRAME_COUNT                8U
#define SIGNAL_MIN_VALID_FRAME_COUNT               5U

#define SIGNAL_DETECT_GATE_RATE_0_HZ               512000UL
#define SIGNAL_DETECT_GATE_RATE_1_HZ               1024000UL
#define SIGNAL_DETECT_GATE_RATE_2_HZ               2048000UL

/* 接近 12 位满量程两端的样本记为削顶样本。 */
#define SIGNAL_DETECT_CLIP_LOW_CODE                8U
#define SIGNAL_DETECT_CLIP_HIGH_CODE               4087U

/* E2 预处理门限。所有幅值均使用去直流后的 ADC 码值。 */
#define SIGNAL_PREPROCESS_SAMPLE_COUNT             SIGNAL_DETECT_EXPECTED_FRAME_SAMPLES
#define SIGNAL_PREPROCESS_MIN_RMS_CODE             4.0f
#define SIGNAL_PREPROCESS_MAX_CLIP_RATIO           0.01f
#define SIGNAL_PREPROCESS_PI_F                     3.14159265358979323846f

#endif /* APP_SIGNAL_DETECT_SIGNAL_DETECT_CFG_H_ */
