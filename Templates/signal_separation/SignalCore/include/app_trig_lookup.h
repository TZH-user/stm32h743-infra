#ifndef APP_MATH_APP_TRIG_LOOKUP_H_
#define APP_MATH_APP_TRIG_LOOKUP_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*@brief:用CMSIS-DSP正弦表计算弧度角的正弦和余弦。
 *@param:phase_rad  输入相位，单位rad。
 *@param:sine       正弦输出。
 *@param:cosine     余弦输出。
 */
void AppTrigLookup_SinCosRadians(float phase_rad,
                                 float *sine,
                                 float *cosine);

/*@brief:按Q32整周相位查询Q15正弦，使用两位线性细分。
 *@param:phase_q32  0到UINT32_MAX映射一个完整周期。
 *@return:Q15正弦值，范围[-32768,32767]。
 */
int32_t AppTrigLookup_SineQ15(uint32_t phase_q32);

#ifdef __cplusplus
}
#endif

#endif /* APP_MATH_APP_TRIG_LOOKUP_H_ */
