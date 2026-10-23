#ifndef APP_RTOS_APP_OUTPUT_TASK_INTERNAL_H_
#define APP_RTOS_APP_OUTPUT_TASK_INTERNAL_H_

#include "app_output_task.h"
#include "app_output_config.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_OUTPUT_FIREWATER_MAX_ROW_BYTES    24U
#define APP_OUTPUT_PACKET_BYTES               \
    (APP_OUTPUT_STREAM_CHUNK_POINTS * APP_OUTPUT_FIREWATER_MAX_ROW_BYTES)
#define APP_OUTPUT_TASK_POLL_MS               2U

#define APP_OUTPUT_FREQUENCY_FORMAT_NONE          0U
#define APP_OUTPUT_FREQUENCY_FORMAT_HZ            1U
#define APP_OUTPUT_FREQUENCY_FORMAT_KHZ           2U
#define APP_OUTPUT_FREQUENCY_FORMAT_SAMPLE_INDEX  3U

typedef struct
{
    uint16_t length;
    uint8_t kind;
    uint8_t reserved;
    char data[APP_OUTPUT_LOG_MESSAGE_BYTES - 4U];
} AppOutputLogMessage;

typedef struct
{
    AppOutputMode source_mode;
    uint16_t count;
    uint8_t frequency_format;
    uint8_t reserved;
    uint32_t frequencies_hz[APP_OUTPUT_STREAM_CHUNK_POINTS];
    int32_t values[APP_OUTPUT_STREAM_CHUNK_POINTS];
} AppOutputStreamPacket;

typedef struct
{
    volatile AppOutputMode requested_mode;
    volatile AppOutputMode active_mode;
    volatile uint32_t mode_switches;
    volatile uint32_t dropped_log_messages;
    volatile uint32_t log_messages_peak;
    volatile uint32_t dropped_stream_packets;
    volatile uint32_t transport_retries;
    volatile uint32_t transport_errors;
    volatile uint32_t stream_packets_peak;
    volatile uint32_t stream_packets_sent;
    volatile uint32_t stream_points_sent;

    AppOutputStreamPacket stream_queue[APP_OUTPUT_STREAM_QUEUE_DEPTH];
    volatile uint8_t stream_head;
    volatile uint8_t stream_tail;
    uint16_t stream_build_count;
    AppOutputMode stream_build_mode;
    uint8_t stream_build_frequency_format;

    uint32_t adc_decimation;
    uint32_t adc_countdown;

    SignalFftResult pending_signal_result;
    volatile bool signal_result_pending;
    SignalDualSineResult pending_dual_sine_result;
    volatile bool dual_sine_result_pending;
    SignalHarmonicResult pending_harmonic_result;
    volatile bool harmonic_result_pending;
    SignalJointResult pending_joint_result;
    volatile bool joint_result_pending;
    SignalVoteResult pending_vote_result;
    volatile bool vote_result_pending;
    AppPhaseLockStatus pending_phase_lock_status;
    volatile bool phase_lock_status_pending;
} AppOutputContext;

extern AppOutputContext g_app_output_context;

uint32_t AppOutputInternal_IrqSave(void);
void AppOutputInternal_IrqRestore(uint32_t primask);
int AppOutputInternal_WriteWait(const uint8_t *data, uint16_t length);

void AppOutputLogFormatter_EmitPending(void);

void AppOutputWave_Reset(void);
bool AppOutputWave_PacketAvailable(void);
void AppOutputWave_SendOne(AppOutputMode active_mode);
uint32_t AppOutputWave_UsedPacketsLocked(void);

#endif /* APP_RTOS_APP_OUTPUT_TASK_INTERNAL_H_ */
