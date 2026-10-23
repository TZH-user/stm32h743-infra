/**
  ******************************************************************************
  * @file    app_trig_lookup.c
  * @brief   CMSIS-DSP正弦表统一查询接口（UTF-8）。
  ******************************************************************************
  */

#include "app_trig_lookup.h"

#include "arm_common_tables.h"
#include "arm_math.h"

#include <stddef.h>

#define APP_TRIG_LOOKUP_RAD_TO_DEG_F       57.2957795130823208768f
#define APP_TRIG_LOOKUP_TABLE_BITS         9U
#define APP_TRIG_LOOKUP_FRACTION_BITS      2U
#define APP_TRIG_LOOKUP_TABLE_SHIFT        \
    (32U - APP_TRIG_LOOKUP_TABLE_BITS)
#define APP_TRIG_LOOKUP_FRACTION_SHIFT     \
    (APP_TRIG_LOOKUP_TABLE_SHIFT - APP_TRIG_LOOKUP_FRACTION_BITS)
#define APP_TRIG_LOOKUP_FRACTION_MASK      \
    ((1U << APP_TRIG_LOOKUP_FRACTION_BITS) - 1U)

void AppTrigLookup_SinCosRadians(float phase_rad,
                                 float *sine,
                                 float *cosine)
{
    if ((sine == NULL) || (cosine == NULL))
    {
        return;
    }

    arm_sin_cos_f32(phase_rad * APP_TRIG_LOOKUP_RAD_TO_DEG_F,
                    sine,
                    cosine);
}

int32_t AppTrigLookup_SineQ15(uint32_t phase_q32)
{
    uint32_t index = phase_q32 >> APP_TRIG_LOOKUP_TABLE_SHIFT;
    uint32_t fraction = (phase_q32 >> APP_TRIG_LOOKUP_FRACTION_SHIFT)
        & APP_TRIG_LOOKUP_FRACTION_MASK;
    int32_t first = sinTable_q15[index];
    int32_t second = sinTable_q15[index + 1U];
    int32_t difference = second - first;
    return first + ((difference * (int32_t)fraction
                     + (1L << (APP_TRIG_LOOKUP_FRACTION_BITS - 1U)))
                    >> APP_TRIG_LOOKUP_FRACTION_BITS);
}
