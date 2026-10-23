#include "arm_math.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#define SIGNALHOST_PI  3.14159265358979323846

static void signalhost_complex_fft(float *real,
                                   float *imaginary,
                                   uint32_t count,
                                   bool inverse)
{
    for (uint32_t i = 1U, j = 0U; i < count; ++i)
    {
        uint32_t bit = count >> 1U;
        for (; (j & bit) != 0U; bit >>= 1U)
        {
            j ^= bit;
        }
        j ^= bit;
        if (i < j)
        {
            float real_tmp = real[i];
            float imag_tmp = imaginary[i];
            real[i] = real[j];
            imaginary[i] = imaginary[j];
            real[j] = real_tmp;
            imaginary[j] = imag_tmp;
        }
    }

    for (uint32_t length = 2U; length <= count; length <<= 1U)
    {
        double angle = (inverse ? 2.0 : -2.0) * SIGNALHOST_PI
                       / (double)length;
        float step_real = (float)cos(angle);
        float step_imag = (float)sin(angle);
        uint32_t half = length >> 1U;

        for (uint32_t base = 0U; base < count; base += length)
        {
            float twiddle_real = 1.0f;
            float twiddle_imag = 0.0f;
            for (uint32_t offset = 0U; offset < half; ++offset)
            {
                uint32_t even = base + offset;
                uint32_t odd = even + half;
                float odd_real = (real[odd] * twiddle_real)
                                 - (imaginary[odd] * twiddle_imag);
                float odd_imag = (real[odd] * twiddle_imag)
                                 + (imaginary[odd] * twiddle_real);
                float even_real = real[even];
                float even_imag = imaginary[even];

                real[even] = even_real + odd_real;
                imaginary[even] = even_imag + odd_imag;
                real[odd] = even_real - odd_real;
                imaginary[odd] = even_imag - odd_imag;

                float next_real = (twiddle_real * step_real)
                                  - (twiddle_imag * step_imag);
                twiddle_imag = (twiddle_real * step_imag)
                               + (twiddle_imag * step_real);
                twiddle_real = next_real;
            }
        }
    }

    if (inverse)
    {
        float scale = 1.0f / (float)count;
        for (uint32_t index = 0U; index < count; ++index)
        {
            real[index] *= scale;
            imaginary[index] *= scale;
        }
    }
}

arm_status arm_rfft_fast_init_f32(arm_rfft_fast_instance_f32 *instance,
                                  uint16_t fft_len)
{
    if ((instance == NULL) || (fft_len == 0U)
        || ((fft_len & (uint16_t)(fft_len - 1U)) != 0U))
    {
        return ARM_MATH_ARGUMENT_ERROR;
    }
    instance->fft_len = fft_len;
    return ARM_MATH_SUCCESS;
}

void arm_rfft_fast_f32(const arm_rfft_fast_instance_f32 *instance,
                       float *input,
                       float *output,
                       uint8_t inverse_flag)
{
    if ((instance == NULL) || (input == NULL) || (output == NULL))
    {
        return;
    }

    uint32_t count = instance->fft_len;
    float *real = (float *)malloc(count * sizeof(float));
    float *imaginary = (float *)calloc(count, sizeof(float));
    if ((real == NULL) || (imaginary == NULL))
    {
        free(real);
        free(imaginary);
        return;
    }

    for (uint32_t index = 0U; index < count; ++index)
    {
        real[index] = input[index];
    }
    signalhost_complex_fft(real, imaginary, count, inverse_flag != 0U);

    if (inverse_flag == 0U)
    {
        output[0U] = real[0U];
        output[1U] = real[count / 2U];
        for (uint32_t bin = 1U; bin < (count / 2U); ++bin)
        {
            output[2U * bin] = real[bin];
            output[(2U * bin) + 1U] = imaginary[bin];
        }
    }
    else
    {
        for (uint32_t index = 0U; index < count; ++index)
        {
            output[index] = real[index];
        }
    }

    free(real);
    free(imaginary);
}

void arm_sin_cos_f32(float theta_degrees, float *sine, float *cosine)
{
    if ((sine == NULL) || (cosine == NULL))
    {
        return;
    }
    double radians = (double)theta_degrees * SIGNALHOST_PI / 180.0;
    *sine = (float)sin(radians);
    *cosine = (float)cos(radians);
}
