#ifndef DUAL_SPI_SESSION_H
#define DUAL_SPI_SESSION_H

#include "dual_spi_protocol.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t valid_pages;
    uint32_t protocol_errors;
    uint32_t accepted_pages;
    uint32_t duplicate_pages;
    uint32_t sequence_gaps;
    uint32_t backpressured_pages;
    uint32_t dispatched_messages;
    uint32_t acked_tx_pages;
    uint32_t retransmit_pages_built;
} dual_spi_session_stats_t;

typedef struct
{
    dual_spi_link_id_t link_id;
    dual_spi_rx_sequence_state_t rx_state;
    dual_spi_tx_state_t tx_state;

    bool pending_result_valid;
    uint32_t pending_result_sequence;
    uint16_t pending_result_status;

    uint8_t pending_payload[DUAL_SPI_PAYLOAD_BYTES_MAX];
    uint16_t pending_payload_bytes;
    uint16_t pending_message_count;

    uint8_t outstanding_payload[DUAL_SPI_PAYLOAD_BYTES_MAX];
    uint16_t outstanding_payload_bytes;
    uint16_t outstanding_message_count;
    bool outstanding_transmitted;
    bool replay_after_reset;

    bool rx_page_valid;
    dual_spi_page_meta_t rx_page_meta;
    uint8_t rx_page_payload[DUAL_SPI_PAYLOAD_BYTES_MAX];
    uint16_t rx_page_payload_bytes;
    uint16_t rx_page_message_count;

    dual_spi_session_stats_t stats;
} dual_spi_session_t;

void dual_spi_session_init(dual_spi_session_t *session,
                           dual_spi_link_id_t link_id);

dual_spi_result_t dual_spi_session_queue_tlv(dual_spi_session_t *session,
                                             uint16_t type,
                                             uint16_t tag,
                                             const void *body,
                                             uint16_t body_bytes);

dual_spi_result_t dual_spi_session_build_tx_page(
    dual_spi_session_t *session,
    uint8_t page[DUAL_SPI_PAGE_BYTES],
    uint32_t sender_time_us,
    uint16_t rx_credit_pages);

void dual_spi_session_mark_tx_attempted(dual_spi_session_t *session);

bool dual_spi_session_peek_rx_page(const dual_spi_session_t *session,
                                   dual_spi_page_view_t *view);

void dual_spi_session_release_rx_page(dual_spi_session_t *session);

uint16_t dual_spi_session_rx_credit_pages(
    const dual_spi_session_t *session);

dual_spi_result_t dual_spi_session_receive_page(
    dual_spi_session_t *session,
    const uint8_t *page,
    size_t page_bytes,
    dual_spi_rx_disposition_t *disposition,
    dual_spi_tx_observation_t *observation);

#ifdef __cplusplus
}
#endif

#endif




