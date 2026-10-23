#include "calibration_core.h"

#include <float.h>
#include <math.h>
#include <string.h>

#define CALIBRATION_CORE_CRC32_POLYNOMIAL  0xEDB88320UL

static bool calibration_core_model_valid(
    const CalibrationLinearModel *model)
{
    return (model != NULL) && model->valid
        && isfinite(model->gain)
        && isfinite(model->offset)
        && isfinite(model->r_squared)
        && isfinite(model->max_abs_residual)
        && (fabsf(model->gain) > FLT_EPSILON)
        && (model->point_count >= 2U);
}

void CalibrationCore_ProfileInit(CalibrationProfile *profile,
                                 uint32_t source_id,
                                 uint32_t sequence)
{
    if (profile == NULL)
    {
        return;
    }

    memset(profile, 0, sizeof(*profile));
    profile->magic = CALIBRATION_CORE_PROFILE_MAGIC;
    profile->schema_version = CALIBRATION_CORE_SCHEMA_VERSION;
    profile->payload_size = (uint16_t)sizeof(*profile);
    profile->sequence = sequence;
    profile->source_id = source_id;
}

void CalibrationCore_SessionInit(CalibrationSessionInfo *session,
                                 uint32_t sequence,
                                 uint32_t mode_id,
                                 uint32_t source_id,
                                 uint32_t item_id,
                                 uint32_t expected_point_count)
{
    if (session == NULL)
    {
        return;
    }

    memset(session, 0, sizeof(*session));
    session->magic = CALIBRATION_CORE_SESSION_MAGIC;
    session->schema_version = CALIBRATION_CORE_SCHEMA_VERSION;
    session->payload_size = (uint16_t)sizeof(*session);
    session->sequence = sequence;
    session->mode_id = mode_id;
    session->source_id = source_id;
    session->item_id = item_id;
    session->expected_point_count = expected_point_count;
}

uint32_t CalibrationCore_Crc32(const void *data, size_t size)
{
    if ((data == NULL) && (size != 0U))
    {
        return 0U;
    }

    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t index = 0U; index < size; ++index)
    {
        crc ^= bytes[index];
        for (uint32_t bit = 0U; bit < 8U; ++bit)
        {
            uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U)
                ^ (CALIBRATION_CORE_CRC32_POLYNOMIAL & mask);
        }
    }
    return ~crc;
}

void CalibrationCore_RecordHeaderInit(CalibrationRecordHeader *header,
                                      CalibrationRecordType record_type,
                                      uint32_t sequence,
                                      const void *payload,
                                      size_t payload_size)
{
    if ((header == NULL) || (payload_size > UINT32_MAX)
        || ((payload == NULL) && (payload_size != 0U)))
    {
        return;
    }

    memset(header, 0, sizeof(*header));
    header->magic = CALIBRATION_CORE_RECORD_MAGIC;
    header->schema_version = CALIBRATION_CORE_SCHEMA_VERSION;
    header->record_type = (uint16_t)record_type;
    header->sequence = sequence;
    header->payload_size = (uint32_t)payload_size;
    header->payload_crc32 = CalibrationCore_Crc32(payload, payload_size);
}

CalibrationCoreStatus CalibrationCore_ValidateRampLimits(
    const CalibrationRampLimits *limits)
{
    if ((limits == NULL)
        || !isfinite(limits->lower_limit)
        || !isfinite(limits->upper_limit)
        || !isfinite(limits->max_step)
        || (limits->lower_limit >= limits->upper_limit)
        || (limits->max_step <= 0.0f)
        || (limits->max_step
            > (limits->upper_limit - limits->lower_limit)))
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }
    return CALIBRATION_CORE_OK;
}

CalibrationCoreStatus CalibrationCore_ValidateProfile(
    const CalibrationProfile *profile)
{
    if (profile == NULL)
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }
    if ((profile->magic != CALIBRATION_CORE_PROFILE_MAGIC)
        || (profile->schema_version != CALIBRATION_CORE_SCHEMA_VERSION)
        || (profile->payload_size != sizeof(*profile)))
    {
        return CALIBRATION_CORE_ERROR_SCHEMA;
    }
    if (!profile->valid || (profile->valid_sections == 0U))
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }

    if (((profile->valid_sections & CALIBRATION_PROFILE_SECTION_ADC_MODEL)
         != 0U)
        && !calibration_core_model_valid(&profile->adc_model))
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }
    for (uint32_t channel = 0U;
         channel < CALIBRATION_CORE_DAC_CHANNEL_COUNT;
         ++channel)
    {
        uint32_t model_section = (channel == 0U)
            ? CALIBRATION_PROFILE_SECTION_DAC1_MODEL
            : CALIBRATION_PROFILE_SECTION_DAC2_MODEL;
        uint32_t limits_section = (channel == 0U)
            ? CALIBRATION_PROFILE_SECTION_DAC1_LIMITS
            : CALIBRATION_PROFILE_SECTION_DAC2_LIMITS;
        uint32_t frequency_section = (channel == 0U)
            ? CALIBRATION_PROFILE_SECTION_DAC1_FREQUENCY
            : CALIBRATION_PROFILE_SECTION_DAC2_FREQUENCY;

        if (((profile->valid_sections & model_section) != 0U)
            && !calibration_core_model_valid(&profile->dac_model[channel]))
        {
            return CALIBRATION_CORE_ERROR_ARGUMENT;
        }
        if (((profile->valid_sections & limits_section) != 0U)
            && (CalibrationCore_ValidateRampLimits(
                    &profile->dac_limits[channel]) != CALIBRATION_CORE_OK))
        {
            return CALIBRATION_CORE_ERROR_ARGUMENT;
        }
        if ((profile->valid_sections & frequency_section) != 0U)
        {
            uint16_t count = profile->frequency_point_count[channel];
            if ((count == 0U)
                || (count > CALIBRATION_CORE_MAX_FREQUENCY_POINTS))
            {
                return CALIBRATION_CORE_ERROR_OUT_OF_RANGE;
            }
            for (uint16_t index = 0U; index < count; ++index)
            {
                const CalibrationFrequencyPoint *point =
                    &profile->frequency_response[channel][index];
                if ((point->frequency_hz == 0U)
                    || !isfinite(point->amplitude_ratio)
                    || !isfinite(point->phase_offset_deg)
                    || (point->amplitude_ratio <= 0.0f))
                {
                    return CALIBRATION_CORE_ERROR_ARGUMENT;
                }
            }
        }
    }

    if (((profile->valid_sections & CALIBRATION_PROFILE_SECTION_ADC_LIMITS)
         != 0U)
        && (CalibrationCore_ValidateRampLimits(&profile->adc_limits)
            != CALIBRATION_CORE_OK))
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }
    if ((profile->valid_sections & CALIBRATION_PROFILE_SECTION_CHANNEL_MATCH)
        != 0U)
    {
        if (!isfinite(profile->channel_amplitude_ratio)
            || !isfinite(profile->channel_phase_offset_deg)
            || (profile->channel_amplitude_ratio <= 0.0f))
        {
            return CALIBRATION_CORE_ERROR_ARGUMENT;
        }
    }
    return CALIBRATION_CORE_OK;
}

CalibrationCoreStatus CalibrationCore_FitLinear(
    const CalibrationMeasurementPoint *points,
    size_t point_count,
    CalibrationLinearModel *model)
{
    if ((points == NULL) || (model == NULL))
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }
    if (point_count < 2U)
    {
        return CALIBRATION_CORE_ERROR_NOT_ENOUGH_POINTS;
    }

    memset(model, 0, sizeof(*model));

    double mean_x = 0.0;
    double mean_y = 0.0;
    for (size_t index = 0U; index < point_count; ++index)
    {
        if (!isfinite(points[index].reference_value)
            || !isfinite(points[index].measured_value))
        {
            return CALIBRATION_CORE_ERROR_NON_FINITE;
        }
        mean_x += (double)points[index].reference_value;
        mean_y += (double)points[index].measured_value;
    }
    mean_x /= (double)point_count;
    mean_y /= (double)point_count;

    double sum_xx = 0.0;
    double sum_xy = 0.0;
    double sum_yy = 0.0;
    for (size_t index = 0U; index < point_count; ++index)
    {
        double delta_x = (double)points[index].reference_value - mean_x;
        double delta_y = (double)points[index].measured_value - mean_y;
        sum_xx += delta_x * delta_x;
        sum_xy += delta_x * delta_y;
        sum_yy += delta_y * delta_y;
    }
    if (sum_xx <= DBL_EPSILON)
    {
        return CALIBRATION_CORE_ERROR_DEGENERATE_INPUT;
    }

    double gain = sum_xy / sum_xx;
    double offset = mean_y - (gain * mean_x);
    double residual_sum = 0.0;
    double max_abs_residual = 0.0;
    for (size_t index = 0U; index < point_count; ++index)
    {
        double predicted = gain * (double)points[index].reference_value
            + offset;
        double residual = (double)points[index].measured_value - predicted;
        double absolute_residual = fabs(residual);
        residual_sum += residual * residual;
        if (absolute_residual > max_abs_residual)
        {
            max_abs_residual = absolute_residual;
        }
    }

    double r_squared = (sum_yy > DBL_EPSILON)
        ? (1.0 - (residual_sum / sum_yy)) : 1.0;
    if (!isfinite(gain) || !isfinite(offset) || !isfinite(r_squared)
        || !isfinite(max_abs_residual))
    {
        return CALIBRATION_CORE_ERROR_NON_FINITE;
    }

    model->gain = (float)gain;
    model->offset = (float)offset;
    model->r_squared = (float)r_squared;
    model->max_abs_residual = (float)max_abs_residual;
    model->point_count = (uint32_t)point_count;
    model->valid = true;
    return CALIBRATION_CORE_OK;
}

CalibrationCoreStatus CalibrationCore_ApplyForward(
    const CalibrationLinearModel *model,
    float reference_value,
    float *measured_value)
{
    if ((model == NULL) || (measured_value == NULL) || !model->valid)
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }

    float result = (model->gain * reference_value) + model->offset;
    if (!isfinite(result))
    {
        return CALIBRATION_CORE_ERROR_NON_FINITE;
    }
    *measured_value = result;
    return CALIBRATION_CORE_OK;
}

CalibrationCoreStatus CalibrationCore_ApplyInverse(
    const CalibrationLinearModel *model,
    float measured_value,
    float *reference_value)
{
    if ((model == NULL) || (reference_value == NULL) || !model->valid)
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }
    if (fabsf(model->gain) <= FLT_EPSILON)
    {
        return CALIBRATION_CORE_ERROR_DEGENERATE_INPUT;
    }

    float result = (measured_value - model->offset) / model->gain;
    if (!isfinite(result))
    {
        return CALIBRATION_CORE_ERROR_NON_FINITE;
    }
    *reference_value = result;
    return CALIBRATION_CORE_OK;
}

CalibrationCoreStatus CalibrationCore_NextRampValue(
    const CalibrationRampLimits *limits,
    float current_value,
    float target_value,
    float *next_value,
    bool *target_reached)
{
    if ((next_value == NULL) || (target_reached == NULL)
        || !isfinite(current_value) || !isfinite(target_value)
        || (CalibrationCore_ValidateRampLimits(limits)
            != CALIBRATION_CORE_OK))
    {
        return CALIBRATION_CORE_ERROR_ARGUMENT;
    }
    if ((target_value < limits->lower_limit)
        || (target_value > limits->upper_limit)
        || (current_value < limits->lower_limit)
        || (current_value > limits->upper_limit))
    {
        return CALIBRATION_CORE_ERROR_OUT_OF_RANGE;
    }

    float difference = target_value - current_value;
    if (fabsf(difference) <= limits->max_step)
    {
        *next_value = target_value;
        *target_reached = true;
        return CALIBRATION_CORE_OK;
    }

    *next_value = current_value
        + ((difference > 0.0f) ? limits->max_step : -limits->max_step);
    *target_reached = false;
    return CALIBRATION_CORE_OK;
}
