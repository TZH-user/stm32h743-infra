#include "app_trig_lookup.h"

#include <math.h>
#include <stddef.h>

#define SIGNALHOST_TWO_PI  6.28318530717958647692

void AppTrigLookup_SinCosRadians(float phase_rad,
                                 float *sine,
                                 float *cosine)
{
    if ((sine == NULL) || (cosine == NULL))
    {
        return;
    }
    *sine = sinf(phase_rad);
    *cosine = cosf(phase_rad);
}

int32_t AppTrigLookup_SineQ15(uint32_t phase_q32)
{
    double phase = SIGNALHOST_TWO_PI * (double)phase_q32 / 4294967296.0;
    long value = lround(sin(phase) * 32767.0);
    if (value > 32767L)
    {
        value = 32767L;
    }
    else if (value < -32768L)
    {
        value = -32768L;
    }
    return (int32_t)value;
}

