#ifndef SIGNALHOST_ARM_MATH_H_
#define SIGNALHOST_ARM_MATH_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    ARM_MATH_SUCCESS = 0,
    ARM_MATH_ARGUMENT_ERROR = -1
} arm_status;

typedef struct
{
    uint16_t fft_len;
} arm_rfft_fast_instance_f32;

arm_status arm_rfft_fast_init_f32(arm_rfft_fast_instance_f32 *instance,
                                  uint16_t fft_len);
void arm_rfft_fast_f32(const arm_rfft_fast_instance_f32 *instance,
                       float *input,
                       float *output,
                       uint8_t inverse_flag);
void arm_sin_cos_f32(float theta_degrees, float *sine, float *cosine);

#ifdef __cplusplus
}
#endif

#endif /* SIGNALHOST_ARM_MATH_H_ */

