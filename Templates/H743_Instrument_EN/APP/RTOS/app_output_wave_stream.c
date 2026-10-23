/**
  ******************************************************************************
  * @file    app_output_wave_stream.c
  * @brief   ADC/FFT packet ring, FireWater formatting and stream publication.
  ******************************************************************************
  */

#include "app_output_task_internal.h"

#include <stddef.h>

#include "stm32h7xx_hal.h"
#include "app_output_config.h"

#if (APP_OUTPUT_STREAM_CHUNK_POINTS == 0U)
#error "APP_OUTPUT_STREAM_CHUNK_POINTS must be greater than zero"
#endif
#if (APP_OUTPUT_STREAM_QUEUE_DEPTH < 2U)
#error "APP_OUTPUT_STREAM_QUEUE_DEPTH must be at least two"
#endif
#if (APP_OUTPUT_FFT_MAX_POINTS == 0U)
#error "APP_OUTPUT_FFT_MAX_POINTS must be greater than zero"
#endif
#if (((APP_OUTPUT_STREAM_QUEUE_DEPTH - 1U) * APP_OUTPUT_STREAM_CHUNK_POINTS) \
     < APP_OUTPUT_FFT_MAX_POINTS)
#error "Stream queue is too small for APP_OUTPUT_FFT_MAX_POINTS"
#endif

static uint8_t s_firewater_packet[APP_OUTPUT_PACKET_BYTES];

static uint8_t app_output_next_stream_index(uint8_t index)
{
    ++index;
    if (index >= APP_OUTPUT_STREAM_QUEUE_DEPTH)
    {
        index = 0U;
    }
    return index;
}

static void app_output_reset_stream_locked(void)
{
    g_app_output_context.stream_head = 0U;
    g_app_output_context.stream_tail = 0U;
    g_app_output_context.stream_build_count = 0U;
    g_app_output_context.stream_build_mode = APP_OUTPUT_MODE_OFF;
    g_app_output_context.stream_build_frequency_format =
        APP_OUTPUT_FREQUENCY_FORMAT_NONE;
    g_app_output_context.adc_decimation = 0U;
    g_app_output_context.adc_countdown = 0U;
    __DMB();
}

void AppOutputWave_Reset(void)
{
    uint32_t primask = AppOutputInternal_IrqSave();
    app_output_reset_stream_locked();
    AppOutputInternal_IrqRestore(primask);
}

static bool app_output_commit_stream_locked(AppOutputMode mode)
{
    if (g_app_output_context.stream_build_count == 0U)
    {
        return true;
    }

    uint8_t head = g_app_output_context.stream_head;
    uint8_t next = app_output_next_stream_index(head);

    if (next == g_app_output_context.stream_tail)
    {
        ++g_app_output_context.dropped_stream_packets;
        g_app_output_context.stream_build_count = 0U;
        g_app_output_context.stream_build_mode = APP_OUTPUT_MODE_OFF;
        return false;
    }

    g_app_output_context.stream_queue[head].source_mode = mode;
    g_app_output_context.stream_queue[head].count = g_app_output_context.stream_build_count;
    g_app_output_context.stream_queue[head].frequency_format =
        g_app_output_context.stream_build_frequency_format;
    __DMB();
    g_app_output_context.stream_head = next;

    uint32_t used_packets;
    if (next >= g_app_output_context.stream_tail)
    {
        used_packets = (uint32_t)next
            - (uint32_t)g_app_output_context.stream_tail;
    }
    else
    {
        used_packets = (uint32_t)APP_OUTPUT_STREAM_QUEUE_DEPTH
            - (uint32_t)g_app_output_context.stream_tail
            + (uint32_t)next;
    }
    if (used_packets > g_app_output_context.stream_packets_peak)
    {
        g_app_output_context.stream_packets_peak = used_packets;
    }

    g_app_output_context.stream_build_count = 0U;
    g_app_output_context.stream_build_mode = APP_OUTPUT_MODE_OFF;
    g_app_output_context.stream_build_frequency_format =
        APP_OUTPUT_FREQUENCY_FORMAT_NONE;
    return true;
}

static bool app_output_append_stream_point_locked(
    AppOutputMode mode,
    uint8_t frequency_format,
    uint32_t frequency_hz,
    int32_t value)
{
    if (g_app_output_context.stream_build_count == 0U)
    {
        g_app_output_context.stream_build_mode = mode;
        g_app_output_context.stream_build_frequency_format = frequency_format;
    }
    else if ((g_app_output_context.stream_build_mode != mode)
             || (g_app_output_context.stream_build_frequency_format
                 != frequency_format))
    {
        (void)app_output_commit_stream_locked(g_app_output_context.stream_build_mode);
        g_app_output_context.stream_build_mode = mode;
        g_app_output_context.stream_build_frequency_format = frequency_format;
    }

    uint8_t head = g_app_output_context.stream_head;
    g_app_output_context.stream_queue[head].frequencies_hz[
        g_app_output_context.stream_build_count] = frequency_hz;
    g_app_output_context.stream_queue[head].values[g_app_output_context.stream_build_count] = value;
    ++g_app_output_context.stream_build_count;

    if (g_app_output_context.stream_build_count >= APP_OUTPUT_STREAM_CHUNK_POINTS)
    {
        return app_output_commit_stream_locked(mode);
    }

    return true;
}

static bool app_output_append_stream_value(AppOutputMode mode, int32_t value)
{
    if (g_app_output_context.active_mode != mode)
    {
        return false;
    }

    uint32_t primask = AppOutputInternal_IrqSave();
    bool accepted = false;

    if (g_app_output_context.active_mode == mode)
    {
        accepted = app_output_append_stream_point_locked(
            mode,
            APP_OUTPUT_FREQUENCY_FORMAT_NONE,
            0U,
            value
        );
    }

    AppOutputInternal_IrqRestore(primask);
    return accepted;
}

static bool app_output_append_indexed_stream_value(
    AppOutputMode mode,
    uint32_t sample_index,
    int32_t value)
{
    if (g_app_output_context.active_mode != mode)
    {
        return false;
    }

    uint32_t primask = AppOutputInternal_IrqSave();
    bool accepted = false;

    if (g_app_output_context.active_mode == mode)
    {
        /*
         * 复用现有横轴存储区保存样本序号。发送端会编码成
         * “sample_index,adc_code”，供上位机波形分析直接读取。
         */
        accepted = app_output_append_stream_point_locked(
            mode,
            APP_OUTPUT_FREQUENCY_FORMAT_SAMPLE_INDEX,
            sample_index,
            value
        );
    }

    AppOutputInternal_IrqRestore(primask);
    return accepted;
}
static uint32_t app_output_append_unsigned_integer(uint8_t *destination,
                                                   uint32_t value)
{
    uint32_t length = 0U;
    uint8_t reversed_digits[10];
    uint32_t digit_count = 0U;

    do
    {
        reversed_digits[digit_count++] =
            (uint8_t)('0' + (value % 10U));
        value /= 10U;
    } while (value > 0U);

    while (digit_count > 0U)
    {
        destination[length++] = reversed_digits[--digit_count];
    }

    return length;
}

static uint32_t app_output_append_signed_integer(uint8_t *destination,
                                                  int32_t value)
{
    uint32_t length = 0U;
    uint32_t magnitude;

    if (value < 0)
    {
        destination[length++] = '-';
        magnitude = (uint32_t)(-(int64_t)value);
    }
    else
    {
        magnitude = (uint32_t)value;
    }

    length += app_output_append_unsigned_integer(
        &destination[length],
        magnitude
    );
    return length;
}

static uint32_t app_output_append_frequency_value(
    uint8_t *destination,
    uint32_t frequency_hz,
    uint8_t frequency_format,
    int32_t value)
{
    uint32_t length = 0U;

    if (frequency_format == APP_OUTPUT_FREQUENCY_FORMAT_KHZ)
    {
        uint32_t whole_khz = frequency_hz / 1000U;
        uint32_t fractional_khz = frequency_hz % 1000U;
        length += app_output_append_unsigned_integer(
            &destination[length],
            whole_khz
        );
        destination[length++] = '.';
        destination[length++] =
            (uint8_t)('0' + ((fractional_khz / 100U) % 10U));
        destination[length++] =
            (uint8_t)('0' + ((fractional_khz / 10U) % 10U));
        destination[length++] =
            (uint8_t)('0' + (fractional_khz % 10U));
    }
    else
    {
        length += app_output_append_unsigned_integer(
            &destination[length],
            frequency_hz
        );
    }

    destination[length++] = ',';
    length += app_output_append_signed_integer(&destination[length], value);
    destination[length++] = '\n';
    return length;
}

uint32_t AppOutputWave_UsedPacketsLocked(void)
{
    uint8_t head = g_app_output_context.stream_head;
    uint8_t tail = g_app_output_context.stream_tail;

    if (head >= tail)
    {
        return (uint32_t)(head - tail);
    }

    return (uint32_t)APP_OUTPUT_STREAM_QUEUE_DEPTH
        - (uint32_t)tail
        + (uint32_t)head;
}

static uint32_t app_output_stream_free_packets_locked(void)
{
    return ((uint32_t)APP_OUTPUT_STREAM_QUEUE_DEPTH - 1U)
        - AppOutputWave_UsedPacketsLocked();
}

bool AppOutputWave_PacketAvailable(void)
{
    return g_app_output_context.stream_tail != g_app_output_context.stream_head;
}

void AppOutputWave_SendOne(AppOutputMode active_mode)
{
    uint8_t tail = g_app_output_context.stream_tail;
    if (tail == g_app_output_context.stream_head)
    {
        return;
    }

    AppOutputStreamPacket *packet = &g_app_output_context.stream_queue[tail];

    if (packet->source_mode != active_mode)
    {
        g_app_output_context.stream_tail = app_output_next_stream_index(tail);
        ++g_app_output_context.dropped_stream_packets;
        return;
    }

    uint32_t packet_length = 0U;
    for (uint32_t i = 0U; i < packet->count; ++i)
    {
        if (packet->frequency_format == APP_OUTPUT_FREQUENCY_FORMAT_NONE)
        {
            packet_length += app_output_append_signed_integer(
                &s_firewater_packet[packet_length],
                packet->values[i]
            );
            s_firewater_packet[packet_length++] = '\n';
        }
        else
        {
            packet_length += app_output_append_frequency_value(
                &s_firewater_packet[packet_length],
                packet->frequencies_hz[i],
                packet->frequency_format,
                packet->values[i]
            );
        }
    }

    if (AppOutputInternal_WriteWait(s_firewater_packet,
                              (uint16_t)packet_length) >= 0)
    {
        ++g_app_output_context.stream_packets_sent;
        g_app_output_context.stream_points_sent += packet->count;
        __DMB();
        g_app_output_context.stream_tail = app_output_next_stream_index(tail);
    }
}
bool AppOutput_PublishAdcFrame(const uint16_t *samples,
                               uint32_t sample_count)
{
    const uint32_t frame_capacity =
        ((uint32_t)APP_OUTPUT_STREAM_QUEUE_DEPTH - 1U)
        * (uint32_t)APP_OUTPUT_STREAM_CHUNK_POINTS;

    if ((samples == NULL) || (sample_count == 0U)
        || (sample_count > frame_capacity)
        || (g_app_output_context.active_mode != APP_OUTPUT_MODE_ADC_WAVE))
    {
        return false;
    }

    const uint32_t required_packets =
        (sample_count + APP_OUTPUT_STREAM_CHUNK_POINTS - 1U)
        / APP_OUTPUT_STREAM_CHUNK_POINTS;

    /* 单帧采用全有或全无策略，避免 VOFA+ 收到被队列截断的波形。 */
    uint32_t primask = AppOutputInternal_IrqSave();
    bool has_capacity =
        (g_app_output_context.active_mode == APP_OUTPUT_MODE_ADC_WAVE)
        && (g_app_output_context.stream_build_count == 0U)
        && (app_output_stream_free_packets_locked() >= required_packets);

    if (!has_capacity)
    {
        g_app_output_context.dropped_stream_packets += required_packets;
    }
    AppOutputInternal_IrqRestore(primask);

    if (!has_capacity)
    {
        return false;
    }

    for (uint32_t i = 0U; i < sample_count; ++i)
    {
        if (!app_output_append_indexed_stream_value(
                APP_OUTPUT_MODE_ADC_WAVE,
                i,
                (int32_t)(samples[i] & 0x0FFFU)))
        {
            return false;
        }
    }

    primask = AppOutputInternal_IrqSave();
    bool accepted = false;
    if (g_app_output_context.active_mode == APP_OUTPUT_MODE_ADC_WAVE)
    {
        accepted = app_output_commit_stream_locked(
            APP_OUTPUT_MODE_ADC_WAVE
        );
    }
    AppOutputInternal_IrqRestore(primask);
    return accepted;
}

void AppOutput_PublishAdcSamples(const uint16_t *samples,
                                 uint32_t sample_count,
                                 uint32_t sample_rate_hz)
{
    if ((samples == NULL) || (sample_count == 0U)
        || (sample_rate_hz == 0U)
        || (g_app_output_context.active_mode != APP_OUTPUT_MODE_ADC_WAVE))
    {
        return;
    }

    uint32_t target_rate = APP_OUTPUT_ADC_TARGET_POINTS_PER_SECOND;
    if (target_rate == 0U)
    {
        return;
    }

    uint32_t decimation =
        (sample_rate_hz + target_rate - 1U) / target_rate;
    if (decimation == 0U)
    {
        decimation = 1U;
    }

    /*
     * AdcTask has higher priority than OutputTask, so one frame is published as
     * a burst. Ensure that burst can fit in the fixed packet ring even at low
     * sampling rates where target-rate decimation alone would select too many
     * points from one 4096-sample frame.
     */
    const uint32_t burst_capacity =
        ((uint32_t)APP_OUTPUT_STREAM_QUEUE_DEPTH - 1U)
        * (uint32_t)APP_OUTPUT_STREAM_CHUNK_POINTS;
    uint32_t burst_decimation =
        (sample_count + burst_capacity - 1U) / burst_capacity;

    if (decimation < burst_decimation)
    {
        decimation = burst_decimation;
    }

    if (g_app_output_context.adc_decimation != decimation)
    {
        uint32_t primask = AppOutputInternal_IrqSave();
        g_app_output_context.adc_decimation = decimation;
        g_app_output_context.adc_countdown = 0U;
        AppOutputInternal_IrqRestore(primask);
    }

    uint32_t index = g_app_output_context.adc_countdown;
    while (index < sample_count)
    {
        (void)app_output_append_stream_value(
            APP_OUTPUT_MODE_ADC_WAVE,
            (int32_t)(samples[index] & 0x0FFFU)
        );
        index += decimation;
    }

    g_app_output_context.adc_countdown = index - sample_count;
}

bool AppOutput_PublishFftValues(const int32_t *values,
                                uint32_t value_count)
{
    if ((values == NULL) || (value_count == 0U)
        || (value_count > APP_OUTPUT_FFT_MAX_POINTS)
        || (g_app_output_context.active_mode != APP_OUTPUT_MODE_FFT_WAVE))
    {
        return false;
    }

    const uint32_t required_packets =
        (value_count + APP_OUTPUT_STREAM_CHUNK_POINTS - 1U)
        / APP_OUTPUT_STREAM_CHUNK_POINTS;

    /*
     * FFT publication is frame-oriented: either the complete spectrum fits or
     * none of it is queued. The consumer can only advance tail while we copy,
     * so this capacity snapshot cannot become less favorable.
     */
    uint32_t primask = AppOutputInternal_IrqSave();
    bool has_capacity =
        (g_app_output_context.active_mode == APP_OUTPUT_MODE_FFT_WAVE)
        && (g_app_output_context.stream_build_count == 0U)
        && (app_output_stream_free_packets_locked() >= required_packets);

    if (!has_capacity)
    {
        g_app_output_context.dropped_stream_packets += required_packets;
    }
    AppOutputInternal_IrqRestore(primask);

    if (!has_capacity)
    {
        return false;
    }

    for (uint32_t i = 0U; i < value_count; ++i)
    {
        if (!app_output_append_stream_value(
                APP_OUTPUT_MODE_FFT_WAVE,
                values[i]))
        {
            return false;
        }
    }

    primask = AppOutputInternal_IrqSave();
    bool accepted = false;

    if (g_app_output_context.active_mode == APP_OUTPUT_MODE_FFT_WAVE)
    {
        accepted = app_output_commit_stream_locked(
            APP_OUTPUT_MODE_FFT_WAVE
        );
    }

    AppOutputInternal_IrqRestore(primask);
    return accepted;
}

bool AppOutput_PublishFftBins(const int32_t *values,
                              uint32_t value_count,
                              uint16_t start_bin,
                              uint32_t sample_rate_hz,
                              uint16_t fft_size,
                              AppOutputFrequencyUnit frequency_unit)
{
    if ((values == NULL) || (value_count == 0U)
        || (value_count > APP_OUTPUT_FFT_MAX_POINTS)
        || (sample_rate_hz == 0U) || (fft_size == 0U)
        || (frequency_unit > APP_OUTPUT_FREQUENCY_UNIT_KHZ)
        || (g_app_output_context.active_mode != APP_OUTPUT_MODE_FFT_WAVE))
    {
        return false;
    }

    const uint32_t required_packets =
        (value_count + APP_OUTPUT_STREAM_CHUNK_POINTS - 1U)
        / APP_OUTPUT_STREAM_CHUNK_POINTS;
    uint32_t primask = AppOutputInternal_IrqSave();
    bool has_capacity =
        (g_app_output_context.active_mode == APP_OUTPUT_MODE_FFT_WAVE)
        && (g_app_output_context.stream_build_count == 0U)
        && (app_output_stream_free_packets_locked() >= required_packets);

    if (!has_capacity)
    {
        g_app_output_context.dropped_stream_packets += required_packets;
    }
    AppOutputInternal_IrqRestore(primask);
    if (!has_capacity)
    {
        return false;
    }

    uint8_t frequency_format =
        (frequency_unit == APP_OUTPUT_FREQUENCY_UNIT_KHZ)
        ? APP_OUTPUT_FREQUENCY_FORMAT_KHZ
        : APP_OUTPUT_FREQUENCY_FORMAT_HZ;

    for (uint32_t i = 0U; i < value_count; ++i)
    {
        uint32_t bin = (uint32_t)start_bin + i;
        uint32_t frequency_hz = (uint32_t)(
            ((uint64_t)bin * sample_rate_hz + (fft_size / 2U))
            / fft_size
        );

        primask = AppOutputInternal_IrqSave();
        bool accepted = (g_app_output_context.active_mode
                         == APP_OUTPUT_MODE_FFT_WAVE)
            && app_output_append_stream_point_locked(
                APP_OUTPUT_MODE_FFT_WAVE,
                frequency_format,
                frequency_hz,
                values[i]
            );
        AppOutputInternal_IrqRestore(primask);
        if (!accepted)
        {
            return false;
        }
    }

    primask = AppOutputInternal_IrqSave();
    bool accepted = false;
    if (g_app_output_context.active_mode == APP_OUTPUT_MODE_FFT_WAVE)
    {
        accepted = app_output_commit_stream_locked(
            APP_OUTPUT_MODE_FFT_WAVE
        );
    }
    AppOutputInternal_IrqRestore(primask);
    return accepted;
}
