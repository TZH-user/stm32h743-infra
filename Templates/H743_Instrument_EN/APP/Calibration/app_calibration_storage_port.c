/**
  ******************************************************************************
  * @file    app_calibration_storage_port.c
  * @brief   校准会话日志与三槽 Profile 持久化实现（UTF-8）。
  ******************************************************************************
  */

#include "app_calibration_storage_port.h"

#include "app_board_flash.h"
#include "app_board_flash_memory_map.h"
#include "calibration_core.h"

#include <stddef.h>
#include <string.h>

#define APP_CAL_STORAGE_META_MAGIC          0x43414D31UL
#define APP_CAL_STORAGE_PROFILE_MAGIC       0x43415031UL
#define APP_CAL_STORAGE_COMMIT_MARKER       0xA55AC33CUL
#define APP_CAL_STORAGE_SCHEMA_VERSION      1U
#define APP_CAL_STORAGE_SLOT_NONE           0xFFU
#define APP_CAL_STORAGE_META_RECORD_SIZE    64U
#define APP_CAL_STORAGE_LOG_SLOT_SIZE       64U
#define APP_CAL_STORAGE_LOG_PAYLOAD_SIZE    36U

typedef struct
{
    uint32_t magic;
    uint16_t schema_version;
    uint16_t record_size;
    uint32_t generation;
    uint32_t log_write_offset;
    uint32_t max_sequence;
    uint8_t active_slot;
    uint8_t pending_slot;
    uint8_t previous_slot;
    uint8_t reserved0;
    uint32_t active_sequence;
    uint32_t pending_sequence;
    uint32_t previous_sequence;
    uint8_t reserved1[20];
    uint32_t record_crc32;
    uint32_t commit_marker;
} AppCalibrationStorageMetadata;

typedef struct
{
    uint32_t magic;
    uint16_t schema_version;
    uint16_t record_size;
    uint32_t sequence;
    uint32_t payload_size;
    uint32_t payload_crc32;
    CalibrationProfile profile;
    uint32_t record_crc32;
    uint32_t commit_marker;
} AppCalibrationStoredProfile;

typedef struct
{
    CalibrationRecordHeader header;
    uint8_t payload[APP_CAL_STORAGE_LOG_PAYLOAD_SIZE];
    uint32_t record_crc32;
    uint32_t commit_marker;
} AppCalibrationLogSlot;

typedef struct
{
    uint32_t stored_point_count;
    uint32_t flags;
} AppCalibrationSessionFinish;

_Static_assert(sizeof(AppCalibrationStorageMetadata)
               == APP_CAL_STORAGE_META_RECORD_SIZE,
               "Calibration metadata record must be 64 bytes");
_Static_assert(sizeof(AppCalibrationLogSlot) == APP_CAL_STORAGE_LOG_SLOT_SIZE,
               "Calibration log slot must be 64 bytes");
_Static_assert(sizeof(AppCalibrationStoredProfile)
               <= APP_BOARD_FLASH_MAP_SECTOR_SIZE,
               "Calibration profile must fit in one W25Q64 sector");
_Static_assert(sizeof(CalibrationSessionInfo)
               <= APP_CAL_STORAGE_LOG_PAYLOAD_SIZE,
               "Session header exceeds log payload");
_Static_assert(sizeof(CalibrationMeasurementPoint)
               <= APP_CAL_STORAGE_LOG_PAYLOAD_SIZE,
               "Measurement point exceeds log payload");

static bool s_storage_ready = false;
static AppCalibrationStorageMetadata s_metadata;
static uint32_t s_metadata_sector = APP_BOARD_FLASH_MAP_CAL_META_A_START;
static uint32_t s_metadata_next_address = APP_BOARD_FLASH_MAP_CAL_META_A_START;
static uint32_t s_log_write_offset = 0U;
static uint32_t s_active_session_sequence = 0U;
static uint32_t s_active_session_point_count = 0U;

static const uint32_t s_profile_addresses[
    APP_BOARD_FLASH_MAP_CAL_PROFILE_SLOT_COUNT] = {
        APP_BOARD_FLASH_MAP_CAL_PROFILE_A_START,
        APP_BOARD_FLASH_MAP_CAL_PROFILE_B_START,
        APP_BOARD_FLASH_MAP_CAL_PROFILE_C_START,
    };

static bool app_cal_storage_bytes_erased(const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t index = 0U; index < size; ++index)
    {
        if (bytes[index] != 0xFFU)
        {
            return false;
        }
    }
    return true;
}

static bool app_cal_storage_slot_valid(uint8_t slot)
{
    return slot < APP_BOARD_FLASH_MAP_CAL_PROFILE_SLOT_COUNT;
}

static AppCalibrationPortStatus app_cal_storage_map_result(
    AppBoardFlashResult result)
{
    switch (result)
    {
        case APP_BOARD_FLASH_OK:
            return APP_CALIBRATION_PORT_OK;
        case APP_BOARD_FLASH_ERROR_BUSY:
            return APP_CALIBRATION_PORT_BUSY;
        case APP_BOARD_FLASH_ERROR_RANGE:
        case APP_BOARD_FLASH_ERROR_ALIGN:
            return APP_CALIBRATION_PORT_OUT_OF_RANGE;
        case APP_BOARD_FLASH_ERROR_ARG:
        case APP_BOARD_FLASH_ERROR_INIT:
        case APP_BOARD_FLASH_ERROR_ID:
        case APP_BOARD_FLASH_ERROR_QSPI:
        case APP_BOARD_FLASH_ERROR_VERIFY:
        default:
            return APP_CALIBRATION_PORT_IO_ERROR;
    }
}

static bool app_cal_storage_metadata_valid(
    const AppCalibrationStorageMetadata *metadata)
{
    if ((metadata->magic != APP_CAL_STORAGE_META_MAGIC)
        || (metadata->schema_version != APP_CAL_STORAGE_SCHEMA_VERSION)
        || (metadata->record_size != sizeof(*metadata))
        || (metadata->commit_marker != APP_CAL_STORAGE_COMMIT_MARKER)
        || (metadata->log_write_offset
            > APP_BOARD_FLASH_MAP_CAL_SESSION_SIZE))
    {
        return false;
    }

    uint32_t crc = CalibrationCore_Crc32(
        metadata, offsetof(AppCalibrationStorageMetadata, record_crc32));
    return crc == metadata->record_crc32;
}

static bool app_cal_storage_log_slot_valid(const AppCalibrationLogSlot *slot)
{
    if ((slot->header.magic != CALIBRATION_CORE_RECORD_MAGIC)
        || (slot->header.schema_version != CALIBRATION_CORE_SCHEMA_VERSION)
        || (slot->header.payload_size > APP_CAL_STORAGE_LOG_PAYLOAD_SIZE)
        || (slot->commit_marker != APP_CAL_STORAGE_COMMIT_MARKER))
    {
        return false;
    }

    uint32_t payload_crc = CalibrationCore_Crc32(slot->payload,
                                                 slot->header.payload_size);
    uint32_t record_crc = CalibrationCore_Crc32(
        slot, offsetof(AppCalibrationLogSlot, record_crc32));
    return (payload_crc == slot->header.payload_crc32)
        && (record_crc == slot->record_crc32);
}

static bool app_cal_storage_profile_valid(
    const AppCalibrationStoredProfile *stored)
{
    if ((stored->magic != APP_CAL_STORAGE_PROFILE_MAGIC)
        || (stored->schema_version != APP_CAL_STORAGE_SCHEMA_VERSION)
        || (stored->record_size != sizeof(*stored))
        || (stored->payload_size != sizeof(stored->profile))
        || (stored->sequence != stored->profile.sequence)
        || (stored->commit_marker != APP_CAL_STORAGE_COMMIT_MARKER))
    {
        return false;
    }

    uint32_t payload_crc = CalibrationCore_Crc32(&stored->profile,
                                                 sizeof(stored->profile));
    uint32_t record_crc = CalibrationCore_Crc32(
        stored, offsetof(AppCalibrationStoredProfile, record_crc32));
    return (payload_crc == stored->payload_crc32)
        && (record_crc == stored->record_crc32)
        && (CalibrationCore_ValidateProfile(&stored->profile)
            == CALIBRATION_CORE_OK);
}

static AppCalibrationPortStatus app_cal_storage_write_committed(
    uint32_t address,
    const void *record,
    size_t commit_offset)
{
    const uint8_t *bytes = (const uint8_t *)record;
    AppBoardFlashResult result = AppBoardFlash_Write(address,
                                                     bytes,
                                                     (uint32_t)commit_offset);
    if (result != APP_BOARD_FLASH_OK)
    {
        return app_cal_storage_map_result(result);
    }

    result = AppBoardFlash_Write(address + (uint32_t)commit_offset,
                                 bytes + commit_offset,
                                 sizeof(uint32_t));
    return app_cal_storage_map_result(result);
}

static void app_cal_storage_metadata_defaults(void)
{
    memset(&s_metadata, 0, sizeof(s_metadata));
    s_metadata.magic = APP_CAL_STORAGE_META_MAGIC;
    s_metadata.schema_version = APP_CAL_STORAGE_SCHEMA_VERSION;
    s_metadata.record_size = sizeof(s_metadata);
    s_metadata.active_slot = APP_CAL_STORAGE_SLOT_NONE;
    s_metadata.pending_slot = APP_CAL_STORAGE_SLOT_NONE;
    s_metadata.previous_slot = APP_CAL_STORAGE_SLOT_NONE;
}

static AppCalibrationPortStatus app_cal_storage_scan_metadata_sector(
    uint32_t sector_address,
    AppCalibrationStorageMetadata *latest,
    uint32_t *next_address,
    bool *found)
{
    AppCalibrationStorageMetadata record;
    *found = false;
    *next_address = sector_address;

    for (uint32_t offset = 0U;
         offset < APP_BOARD_FLASH_MAP_SECTOR_SIZE;
         offset += sizeof(record))
    {
        AppBoardFlashResult result = AppBoardFlash_Read(sector_address + offset,
                                                        (uint8_t *)&record,
                                                        sizeof(record));
        if (result != APP_BOARD_FLASH_OK)
        {
            return app_cal_storage_map_result(result);
        }

        if (app_cal_storage_bytes_erased(&record, sizeof(record)))
        {
            *next_address = sector_address + offset;
            return APP_CALIBRATION_PORT_OK;
        }
        if (app_cal_storage_metadata_valid(&record)
            && (!*found || (record.generation > latest->generation)))
        {
            *latest = record;
            *found = true;
        }
        *next_address = sector_address + offset + sizeof(record);
    }
    return APP_CALIBRATION_PORT_OK;
}

static AppCalibrationPortStatus app_cal_storage_load_metadata(void)
{
    AppCalibrationStorageMetadata latest_a;
    AppCalibrationStorageMetadata latest_b;
    uint32_t next_a;
    uint32_t next_b;
    bool found_a;
    bool found_b;

    AppCalibrationPortStatus status = app_cal_storage_scan_metadata_sector(
        APP_BOARD_FLASH_MAP_CAL_META_A_START,
        &latest_a,
        &next_a,
        &found_a);
    if (status != APP_CALIBRATION_PORT_OK)
    {
        return status;
    }
    status = app_cal_storage_scan_metadata_sector(
        APP_BOARD_FLASH_MAP_CAL_META_B_START,
        &latest_b,
        &next_b,
        &found_b);
    if (status != APP_CALIBRATION_PORT_OK)
    {
        return status;
    }

    if (!found_a && !found_b)
    {
        app_cal_storage_metadata_defaults();
        s_metadata_sector = APP_BOARD_FLASH_MAP_CAL_META_A_START;
        s_metadata_next_address = APP_BOARD_FLASH_MAP_CAL_META_A_START;
    }
    else if (found_a && (!found_b
                         || (latest_a.generation >= latest_b.generation)))
    {
        s_metadata = latest_a;
        s_metadata_sector = APP_BOARD_FLASH_MAP_CAL_META_A_START;
        s_metadata_next_address = next_a;
    }
    else
    {
        s_metadata = latest_b;
        s_metadata_sector = APP_BOARD_FLASH_MAP_CAL_META_B_START;
        s_metadata_next_address = next_b;
    }
    return APP_CALIBRATION_PORT_OK;
}

static AppCalibrationPortStatus app_cal_storage_append_metadata(void)
{
    AppCalibrationStorageMetadata next = s_metadata;
    next.generation = s_metadata.generation + 1U;
    memset(next.reserved1, 0, sizeof(next.reserved1));
    next.record_crc32 = CalibrationCore_Crc32(
        &next, offsetof(AppCalibrationStorageMetadata, record_crc32));
    next.commit_marker = APP_CAL_STORAGE_COMMIT_MARKER;

    uint32_t sector_end = s_metadata_sector
        + APP_BOARD_FLASH_MAP_SECTOR_SIZE;
    if ((s_metadata_next_address + sizeof(next)) > sector_end)
    {
        uint32_t alternate = (s_metadata_sector
                              == APP_BOARD_FLASH_MAP_CAL_META_A_START)
            ? APP_BOARD_FLASH_MAP_CAL_META_B_START
            : APP_BOARD_FLASH_MAP_CAL_META_A_START;
        AppBoardFlashResult erase_result = AppBoardFlash_Erase4k(alternate);
        if (erase_result != APP_BOARD_FLASH_OK)
        {
            return app_cal_storage_map_result(erase_result);
        }
        s_metadata_sector = alternate;
        s_metadata_next_address = alternate;
    }
    else
    {
        AppCalibrationStorageMetadata current;
        AppBoardFlashResult read_result = AppBoardFlash_Read(
            s_metadata_next_address, (uint8_t *)&current, sizeof(current));
        if (read_result != APP_BOARD_FLASH_OK)
        {
            return app_cal_storage_map_result(read_result);
        }
        if (!app_cal_storage_bytes_erased(&current, sizeof(current)))
        {
            AppBoardFlashResult erase_result = AppBoardFlash_Erase4k(
                s_metadata_sector);
            if (erase_result != APP_BOARD_FLASH_OK)
            {
                return app_cal_storage_map_result(erase_result);
            }
            s_metadata_next_address = s_metadata_sector;
        }
    }

    AppCalibrationPortStatus status = app_cal_storage_write_committed(
        s_metadata_next_address,
        &next,
        offsetof(AppCalibrationStorageMetadata, commit_marker));
    if (status == APP_CALIBRATION_PORT_OK)
    {
        s_metadata = next;
        s_metadata_next_address += sizeof(next);
    }
    return status;
}

static AppCalibrationPortStatus app_cal_storage_read_profile(
    uint8_t slot,
    CalibrationProfile *profile)
{
    if (!app_cal_storage_slot_valid(slot) || (profile == NULL))
    {
        return APP_CALIBRATION_PORT_NOT_CONFIGURED;
    }

    AppCalibrationStoredProfile stored;
    AppBoardFlashResult result = AppBoardFlash_Read(s_profile_addresses[slot],
                                                    (uint8_t *)&stored,
                                                    sizeof(stored));
    if (result != APP_BOARD_FLASH_OK)
    {
        return app_cal_storage_map_result(result);
    }
    if (!app_cal_storage_profile_valid(&stored))
    {
        return APP_CALIBRATION_PORT_NOT_CONFIGURED;
    }
    *profile = stored.profile;
    return APP_CALIBRATION_PORT_OK;
}

static AppCalibrationPortStatus app_cal_storage_write_profile(
    uint8_t slot,
    const CalibrationProfile *profile)
{
    if (!app_cal_storage_slot_valid(slot) || (profile == NULL))
    {
        return APP_CALIBRATION_PORT_OUT_OF_RANGE;
    }

    AppCalibrationStoredProfile stored;
    memset(&stored, 0, sizeof(stored));
    stored.magic = APP_CAL_STORAGE_PROFILE_MAGIC;
    stored.schema_version = APP_CAL_STORAGE_SCHEMA_VERSION;
    stored.record_size = sizeof(stored);
    stored.sequence = profile->sequence;
    stored.payload_size = sizeof(stored.profile);
    stored.profile = *profile;
    stored.payload_crc32 = CalibrationCore_Crc32(&stored.profile,
                                                 sizeof(stored.profile));
    stored.record_crc32 = CalibrationCore_Crc32(
        &stored, offsetof(AppCalibrationStoredProfile, record_crc32));
    stored.commit_marker = APP_CAL_STORAGE_COMMIT_MARKER;

    AppBoardFlashResult result = AppBoardFlash_Erase4k(
        s_profile_addresses[slot]);
    if (result != APP_BOARD_FLASH_OK)
    {
        return app_cal_storage_map_result(result);
    }

    AppCalibrationPortStatus status = app_cal_storage_write_committed(
        s_profile_addresses[slot],
        &stored,
        offsetof(AppCalibrationStoredProfile, commit_marker));
    if (status != APP_CALIBRATION_PORT_OK)
    {
        return status;
    }

    AppCalibrationStoredProfile verify;
    result = AppBoardFlash_Read(s_profile_addresses[slot],
                                (uint8_t *)&verify,
                                sizeof(verify));
    return ((result == APP_BOARD_FLASH_OK)
            && app_cal_storage_profile_valid(&verify))
        ? APP_CALIBRATION_PORT_OK : APP_CALIBRATION_PORT_IO_ERROR;
}

static AppCalibrationPortStatus app_cal_storage_recover_log_tail(void)
{
    uint32_t offset = s_metadata.log_write_offset;
    AppCalibrationLogSlot slot;

    while ((offset + sizeof(slot))
           <= APP_BOARD_FLASH_MAP_CAL_SESSION_SIZE)
    {
        AppBoardFlashResult result = AppBoardFlash_Read(
            APP_BOARD_FLASH_MAP_CAL_SESSION_START + offset,
            (uint8_t *)&slot,
            sizeof(slot));
        if (result != APP_BOARD_FLASH_OK)
        {
            return app_cal_storage_map_result(result);
        }
        if (app_cal_storage_bytes_erased(&slot, sizeof(slot)))
        {
            break;
        }
        if (!app_cal_storage_log_slot_valid(&slot))
        {
            offset = ((offset / APP_BOARD_FLASH_SECTOR_SIZE) + 1U)
                * APP_BOARD_FLASH_SECTOR_SIZE;
            break;
        }
        if (slot.header.sequence > s_metadata.max_sequence)
        {
            s_metadata.max_sequence = slot.header.sequence;
        }
        offset += sizeof(slot);
    }

    s_log_write_offset = offset;
    return (offset <= APP_BOARD_FLASH_MAP_CAL_SESSION_SIZE)
        ? APP_CALIBRATION_PORT_OK : APP_CALIBRATION_PORT_OUT_OF_RANGE;
}

static AppCalibrationPortStatus app_cal_storage_prepare_log_address(
    uint32_t *address)
{
    if (address == NULL)
    {
        return APP_CALIBRATION_PORT_IO_ERROR;
    }

    while ((s_log_write_offset + APP_CAL_STORAGE_LOG_SLOT_SIZE)
           <= APP_BOARD_FLASH_MAP_CAL_SESSION_SIZE)
    {
        AppCalibrationLogSlot current;
        uint32_t candidate = APP_BOARD_FLASH_MAP_CAL_SESSION_START
            + s_log_write_offset;
        AppBoardFlashResult result = AppBoardFlash_Read(candidate,
                                                        (uint8_t *)&current,
                                                        sizeof(current));
        if (result != APP_BOARD_FLASH_OK)
        {
            return app_cal_storage_map_result(result);
        }
        if (app_cal_storage_bytes_erased(&current, sizeof(current)))
        {
            *address = candidate;
            return APP_CALIBRATION_PORT_OK;
        }

        s_log_write_offset = ((s_log_write_offset
                               / APP_BOARD_FLASH_SECTOR_SIZE) + 1U)
            * APP_BOARD_FLASH_SECTOR_SIZE;
        if ((s_log_write_offset + APP_CAL_STORAGE_LOG_SLOT_SIZE)
            > APP_BOARD_FLASH_MAP_CAL_SESSION_SIZE)
        {
            break;
        }

        uint32_t next_sector = APP_BOARD_FLASH_MAP_CAL_SESSION_START
            + s_log_write_offset;
        result = AppBoardFlash_Erase4k(next_sector);
        if (result != APP_BOARD_FLASH_OK)
        {
            return app_cal_storage_map_result(result);
        }
    }
    return APP_CALIBRATION_PORT_OUT_OF_RANGE;
}

static AppCalibrationPortStatus app_cal_storage_append_log_record(
    CalibrationRecordType type,
    uint32_t sequence,
    const void *payload,
    size_t payload_size)
{
    if ((payload == NULL) || (payload_size > APP_CAL_STORAGE_LOG_PAYLOAD_SIZE))
    {
        return APP_CALIBRATION_PORT_OUT_OF_RANGE;
    }

    uint32_t address;
    AppCalibrationPortStatus status = app_cal_storage_prepare_log_address(
        &address);
    if (status != APP_CALIBRATION_PORT_OK)
    {
        return status;
    }

    AppCalibrationLogSlot slot;
    memset(&slot, 0, sizeof(slot));
    CalibrationCore_RecordHeaderInit(&slot.header,
                                     type,
                                     sequence,
                                     payload,
                                     payload_size);
    memcpy(slot.payload, payload, payload_size);
    slot.record_crc32 = CalibrationCore_Crc32(
        &slot, offsetof(AppCalibrationLogSlot, record_crc32));
    slot.commit_marker = APP_CAL_STORAGE_COMMIT_MARKER;

    status = app_cal_storage_write_committed(
        address, &slot, offsetof(AppCalibrationLogSlot, commit_marker));
    if (status == APP_CALIBRATION_PORT_OK)
    {
        s_log_write_offset += sizeof(slot);
    }
    return status;
}

bool AppCalibrationStoragePort_Init(void)
{
    s_storage_ready = false;
    s_active_session_sequence = 0U;
    s_active_session_point_count = 0U;

    if (!AppBoardFlash_RtosInit()
        || (AppBoardFlash_Init() != APP_BOARD_FLASH_OK))
    {
        return false;
    }

    if ((app_cal_storage_load_metadata() != APP_CALIBRATION_PORT_OK)
        || (app_cal_storage_recover_log_tail() != APP_CALIBRATION_PORT_OK))
    {
        return false;
    }

    s_storage_ready = true;
    return true;
}

static bool app_cal_storage_is_ready(void *context)
{
    (void)context;
    return s_storage_ready && AppBoardFlash_IsReady();
}

static AppCalibrationPortStatus app_cal_storage_get_next_sequence(
    void *context,
    uint32_t *sequence)
{
    (void)context;
    if (!s_storage_ready || (sequence == NULL))
    {
        return APP_CALIBRATION_PORT_NOT_CONFIGURED;
    }
    *sequence = s_metadata.max_sequence + 1U;
    if (*sequence == 0U)
    {
        *sequence = 1U;
    }
    return APP_CALIBRATION_PORT_OK;
}

static AppCalibrationPortStatus app_cal_storage_begin_session(
    void *context,
    const CalibrationSessionInfo *session)
{
    (void)context;
    if (!s_storage_ready || (session == NULL)
        || (s_active_session_sequence != 0U))
    {
        return APP_CALIBRATION_PORT_BUSY;
    }

    AppCalibrationPortStatus status = app_cal_storage_append_log_record(
        CALIBRATION_RECORD_SESSION_HEADER,
        session->sequence,
        session,
        sizeof(*session));
    if (status == APP_CALIBRATION_PORT_OK)
    {
        s_active_session_sequence = session->sequence;
        s_active_session_point_count = 0U;
        if (session->sequence > s_metadata.max_sequence)
        {
            s_metadata.max_sequence = session->sequence;
        }
    }
    return status;
}

static AppCalibrationPortStatus app_cal_storage_append_point(
    void *context,
    uint32_t session_sequence,
    const CalibrationMeasurementPoint *point)
{
    (void)context;
    if (!s_storage_ready || (point == NULL)
        || (session_sequence == 0U)
        || (session_sequence != s_active_session_sequence))
    {
        return APP_CALIBRATION_PORT_BUSY;
    }

    AppCalibrationPortStatus status = app_cal_storage_append_log_record(
        CALIBRATION_RECORD_MEASUREMENT_POINT,
        session_sequence,
        point,
        sizeof(*point));
    if (status == APP_CALIBRATION_PORT_OK)
    {
        ++s_active_session_point_count;
    }
    return status;
}

static AppCalibrationPortStatus app_cal_storage_finish_session(
    void *context,
    uint32_t session_sequence)
{
    (void)context;
    if (!s_storage_ready || (session_sequence == 0U)
        || (session_sequence != s_active_session_sequence))
    {
        return APP_CALIBRATION_PORT_BUSY;
    }

    AppCalibrationSessionFinish finish = {
        .stored_point_count = s_active_session_point_count,
        .flags = 0U,
    };
    AppCalibrationPortStatus status = app_cal_storage_append_log_record(
        CALIBRATION_RECORD_SESSION_FINISH,
        session_sequence,
        &finish,
        sizeof(finish));
    if (status != APP_CALIBRATION_PORT_OK)
    {
        return status;
    }

    s_metadata.log_write_offset = s_log_write_offset;
    status = app_cal_storage_append_metadata();
    if (status == APP_CALIBRATION_PORT_OK)
    {
        s_active_session_sequence = 0U;
        s_active_session_point_count = 0U;
    }
    return status;
}

static uint8_t app_cal_storage_choose_pending_slot(void)
{
    if (app_cal_storage_slot_valid(s_metadata.pending_slot))
    {
        return s_metadata.pending_slot;
    }
    for (uint8_t slot = 0U;
         slot < APP_BOARD_FLASH_MAP_CAL_PROFILE_SLOT_COUNT;
         ++slot)
    {
        if ((slot != s_metadata.active_slot)
            && (slot != s_metadata.previous_slot))
        {
            return slot;
        }
    }
    return APP_CAL_STORAGE_SLOT_NONE;
}

static AppCalibrationPortStatus app_cal_storage_store_pending_profile(
    void *context,
    const CalibrationProfile *profile)
{
    (void)context;
    if (!s_storage_ready || (profile == NULL)
        || (CalibrationCore_ValidateProfile(profile) != CALIBRATION_CORE_OK))
    {
        return APP_CALIBRATION_PORT_NOT_CONFIGURED;
    }

    uint8_t slot = app_cal_storage_choose_pending_slot();
    if (!app_cal_storage_slot_valid(slot))
    {
        return APP_CALIBRATION_PORT_OUT_OF_RANGE;
    }

    AppCalibrationPortStatus status = app_cal_storage_write_profile(slot,
                                                                    profile);
    if (status != APP_CALIBRATION_PORT_OK)
    {
        return status;
    }

    s_metadata.pending_slot = slot;
    s_metadata.pending_sequence = profile->sequence;
    return app_cal_storage_append_metadata();
}

static AppCalibrationPortStatus app_cal_storage_load_boot_profile(
    void *context,
    CalibrationProfile *profile)
{
    (void)context;
    if (!s_storage_ready || (profile == NULL))
    {
        return APP_CALIBRATION_PORT_NOT_CONFIGURED;
    }

    if (app_cal_storage_slot_valid(s_metadata.pending_slot))
    {
        AppCalibrationPortStatus status = app_cal_storage_read_profile(
            s_metadata.pending_slot, profile);
        if (status != APP_CALIBRATION_PORT_OK)
        {
            return status;
        }

        s_metadata.previous_slot = s_metadata.active_slot;
        s_metadata.previous_sequence = s_metadata.active_sequence;
        s_metadata.active_slot = s_metadata.pending_slot;
        s_metadata.active_sequence = s_metadata.pending_sequence;
        s_metadata.pending_slot = APP_CAL_STORAGE_SLOT_NONE;
        s_metadata.pending_sequence = 0U;
        status = app_cal_storage_append_metadata();
        return status;
    }

    return app_cal_storage_read_profile(s_metadata.active_slot, profile);
}

void AppCalibrationStoragePort_Fill(AppCalibrationStoragePort *storage)
{
    if (storage == NULL)
    {
        return;
    }

    memset(storage, 0, sizeof(*storage));
    storage->is_ready = app_cal_storage_is_ready;
    storage->get_next_sequence = app_cal_storage_get_next_sequence;
    storage->begin_session = app_cal_storage_begin_session;
    storage->append_point = app_cal_storage_append_point;
    storage->finish_session = app_cal_storage_finish_session;
    storage->store_pending_profile = app_cal_storage_store_pending_profile;
    storage->load_boot_profile = app_cal_storage_load_boot_profile;
}
