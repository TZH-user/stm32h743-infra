#ifndef CALIBRATION_CORE_H_
#define CALIBRATION_CORE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CALIBRATION_CORE_PROFILE_MAGIC          0x43414C31UL
#define CALIBRATION_CORE_SESSION_MAGIC          0x43414C53UL
#define CALIBRATION_CORE_RECORD_MAGIC           0x43414C52UL
#define CALIBRATION_CORE_SCHEMA_VERSION         1U
#define CALIBRATION_CORE_DAC_CHANNEL_COUNT      2U
#define CALIBRATION_CORE_MAX_FREQUENCY_POINTS  32U

#define CALIBRATION_PROFILE_SECTION_ADC_MODEL       (1UL << 0)
#define CALIBRATION_PROFILE_SECTION_DAC1_MODEL      (1UL << 1)
#define CALIBRATION_PROFILE_SECTION_DAC2_MODEL      (1UL << 2)
#define CALIBRATION_PROFILE_SECTION_ADC_LIMITS      (1UL << 3)
#define CALIBRATION_PROFILE_SECTION_DAC1_LIMITS     (1UL << 4)
#define CALIBRATION_PROFILE_SECTION_DAC2_LIMITS     (1UL << 5)
#define CALIBRATION_PROFILE_SECTION_DAC1_FREQUENCY  (1UL << 6)
#define CALIBRATION_PROFILE_SECTION_DAC2_FREQUENCY  (1UL << 7)
#define CALIBRATION_PROFILE_SECTION_CHANNEL_MATCH   (1UL << 8)

typedef enum
{
    CALIBRATION_CORE_OK = 0,
    CALIBRATION_CORE_ERROR_ARGUMENT,
    CALIBRATION_CORE_ERROR_NOT_ENOUGH_POINTS,
    CALIBRATION_CORE_ERROR_DEGENERATE_INPUT,
    CALIBRATION_CORE_ERROR_NON_FINITE,
    CALIBRATION_CORE_ERROR_OUT_OF_RANGE,
    CALIBRATION_CORE_ERROR_SCHEMA,
    CALIBRATION_CORE_ERROR_CHECKSUM
} CalibrationCoreStatus;

typedef enum
{
    CALIBRATION_RECORD_SESSION_HEADER = 1,
    CALIBRATION_RECORD_MEASUREMENT_POINT,
    CALIBRATION_RECORD_PROFILE,
    CALIBRATION_RECORD_SESSION_FINISH
} CalibrationRecordType;

/* 一个拟合点表示“已知参考值 x”和“装置测量值 y”。 */
typedef struct
{
    float reference_value;
    float measured_value;
    uint32_t frequency_hz;
    float phase_deg;
    uint32_t flags;
} CalibrationMeasurementPoint;

/* 线性模型约定 measured = gain * reference + offset。 */
typedef struct
{
    float gain;
    float offset;
    float r_squared;
    float max_abs_residual;
    uint32_t point_count;
    bool valid;
} CalibrationLinearModel;

typedef struct
{
    float lower_limit;
    float upper_limit;
    float max_step;
    uint32_t dwell_time_ms;
} CalibrationRampLimits;

typedef struct
{
    uint32_t frequency_hz;
    float amplitude_ratio;
    float phase_offset_deg;
} CalibrationFrequencyPoint;

/*
 * 会话头只描述数据，不依赖 ADC/DAC/继电器来源。具体采集者把原始点追加到
 * 同一 sequence 下，导出端即可按 schema 还原一次完整校准会话。
 */
typedef struct
{
    uint32_t magic;
    uint16_t schema_version;
    uint16_t payload_size;
    uint32_t sequence;
    uint32_t mode_id;
    uint32_t source_id;
    uint32_t item_id;
    uint32_t expected_point_count;
    uint32_t stored_point_count;
    uint32_t flags;
} CalibrationSessionInfo;

/* Flash/导出记录公共头；payload 的序列化由 Storage Port 负责。 */
typedef struct
{
    uint32_t magic;
    uint16_t schema_version;
    uint16_t record_type;
    uint32_t sequence;
    uint32_t payload_size;
    uint32_t payload_crc32;
} CalibrationRecordHeader;

typedef struct
{
    uint32_t magic;
    uint16_t schema_version;
    uint16_t payload_size;
    uint32_t sequence;
    uint32_t source_id;
    CalibrationLinearModel adc_model;
    CalibrationLinearModel dac_model[CALIBRATION_CORE_DAC_CHANNEL_COUNT];
    CalibrationRampLimits adc_limits;
    CalibrationRampLimits dac_limits[CALIBRATION_CORE_DAC_CHANNEL_COUNT];
    CalibrationFrequencyPoint
        frequency_response[CALIBRATION_CORE_DAC_CHANNEL_COUNT]
                          [CALIBRATION_CORE_MAX_FREQUENCY_POINTS];
    uint16_t frequency_point_count[CALIBRATION_CORE_DAC_CHANNEL_COUNT];
    float channel_amplitude_ratio;
    float channel_phase_offset_deg;
    uint32_t valid_sections;
    bool valid;
} CalibrationProfile;

void CalibrationCore_ProfileInit(CalibrationProfile *profile,
                                 uint32_t source_id,
                                 uint32_t sequence);

void CalibrationCore_SessionInit(CalibrationSessionInfo *session,
                                 uint32_t sequence,
                                 uint32_t mode_id,
                                 uint32_t source_id,
                                 uint32_t item_id,
                                 uint32_t expected_point_count);

void CalibrationCore_RecordHeaderInit(CalibrationRecordHeader *header,
                                      CalibrationRecordType record_type,
                                      uint32_t sequence,
                                      const void *payload,
                                      size_t payload_size);

uint32_t CalibrationCore_Crc32(const void *data, size_t size);

CalibrationCoreStatus CalibrationCore_ValidateRampLimits(
    const CalibrationRampLimits *limits);

CalibrationCoreStatus CalibrationCore_ValidateProfile(
    const CalibrationProfile *profile);

CalibrationCoreStatus CalibrationCore_FitLinear(
    const CalibrationMeasurementPoint *points,
    size_t point_count,
    CalibrationLinearModel *model);

CalibrationCoreStatus CalibrationCore_ApplyForward(
    const CalibrationLinearModel *model,
    float reference_value,
    float *measured_value);

CalibrationCoreStatus CalibrationCore_ApplyInverse(
    const CalibrationLinearModel *model,
    float measured_value,
    float *reference_value);

CalibrationCoreStatus CalibrationCore_NextRampValue(
    const CalibrationRampLimits *limits,
    float current_value,
    float target_value,
    float *next_value,
    bool *target_reached);

#ifdef __cplusplus
}
#endif

#endif /* CALIBRATION_CORE_H_ */
