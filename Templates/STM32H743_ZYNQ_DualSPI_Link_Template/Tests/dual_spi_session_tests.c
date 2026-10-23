#include "dual_spi_session.h"

#include <stdio.h>
#include <string.h>

static unsigned int g_failures;

#define CHECK(condition)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            ++g_failures;                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);         \
        }                                                                       \
    } while (0)

typedef struct
{
    unsigned int count;
    uint16_t last_type;
    uint16_t last_tag;
    uint16_t last_length;
    uint8_t last_body[32];
} message_capture_t;

static void capture_queued_page(dual_spi_session_t *session,
                                message_capture_t *capture)
{
    dual_spi_page_view_t page;
    dual_spi_tlv_iterator_t iterator;
    dual_spi_tlv_view_t message;
    dual_spi_result_t result;
    bool has_value;

    CHECK(dual_spi_session_peek_rx_page(session, &page));
    dual_spi_tlv_iterator_init(&iterator,
                               page.payload,
                               page.payload_bytes,
                               page.message_count);
    do
    {
        result = dual_spi_tlv_iterator_next(&iterator,
                                            &message,
                                            &has_value);
        CHECK(result == DUAL_SPI_RESULT_OK);
        if (has_value)
        {
            ++capture->count;
            capture->last_type = message.type;
            capture->last_tag = message.tag;
            capture->last_length = message.length;
            if (message.length <= sizeof(capture->last_body))
            {
                memcpy(capture->last_body,
                       message.body,
                       message.length);
            }
        }
    } while ((result == DUAL_SPI_RESULT_OK) && has_value);

    dual_spi_session_release_rx_page(session);
}

static void refresh_header_crc(uint8_t page[DUAL_SPI_PAGE_BYTES])
{
    dual_spi_put_u32_le(&page[28], dual_spi_crc32c(page, 28U));
}

static void refresh_page_crc(uint8_t page[DUAL_SPI_PAGE_BYTES])
{
    dual_spi_put_u32_le(&page[DUAL_SPI_PAGE_CRC_OFFSET],
                        dual_spi_crc32c(page, DUAL_SPI_PAGE_CRC_OFFSET));
}

static void encode_control_page(uint8_t page[DUAL_SPI_PAGE_BYTES],
                                uint8_t flags,
                                uint32_t tx_sequence,
                                bool result_valid,
                                uint32_t result_sequence,
                                uint16_t status,
                                uint16_t credit)
{
    dual_spi_page_meta_t meta;

    memset(&meta, 0, sizeof(meta));
    meta.link_id = DUAL_SPI_LINK_A;
    meta.page_flags = flags;
    meta.tx_sequence = tx_sequence;
    meta.rx_credit_pages = credit;
    if (result_valid)
    {
        meta.page_flags |= DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID;
        meta.rx_result_sequence = result_sequence;
        meta.status_code = status;
        if (status != DUAL_SPI_STATUS_OK)
        {
            meta.page_flags |= DUAL_SPI_PAGE_FLAG_ERROR;
        }
    }
    CHECK(dual_spi_page_encode(page,
                               DUAL_SPI_PAGE_BYTES,
                               &meta,
                               NULL,
                               0U,
                               0U) == DUAL_SPI_RESULT_OK);
}

static void encode_sequenced_page(uint8_t page[DUAL_SPI_PAGE_BYTES],
                                  uint8_t link_id,
                                  uint32_t tx_sequence)
{
    dual_spi_page_meta_t meta;

    memset(&meta, 0, sizeof(meta));
    meta.link_id = link_id;
    meta.page_flags = DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID;
    meta.tx_sequence = tx_sequence;
    meta.rx_credit_pages = 1U;
    CHECK(dual_spi_page_encode(page,
                               DUAL_SPI_PAGE_BYTES,
                               &meta,
                               NULL,
                               0U,
                               0U) == DUAL_SPI_RESULT_OK);
}

static void check_pending_protocol_nack(
    dual_spi_session_t *session,
    const uint8_t page[DUAL_SPI_PAGE_BYTES],
    dual_spi_result_t expected_result,
    uint32_t expected_sequence,
    uint16_t expected_status)
{
    dual_spi_rx_disposition_t disposition;
    dual_spi_tx_observation_t observation;

    CHECK(dual_spi_session_receive_page(session,
                                        page,
                                        DUAL_SPI_PAGE_BYTES,
                                        &disposition,
                                        &observation) == expected_result);
    CHECK(session->pending_result_valid);
    CHECK(session->pending_result_sequence == expected_sequence);
    CHECK(session->pending_result_status == expected_status);
}

static void acknowledge_local_reset(dual_spi_session_t *session,
                                    uint8_t tx_page[DUAL_SPI_PAGE_BYTES],
                                    uint8_t peer_page[DUAL_SPI_PAGE_BYTES])
{
    dual_spi_rx_disposition_t disposition;
    dual_spi_tx_observation_t observation;

    CHECK(dual_spi_session_build_tx_page(session, tx_page, 1U, 1U) ==
          DUAL_SPI_RESULT_OK);
    dual_spi_session_mark_tx_attempted(session);
    encode_control_page(peer_page,
                        DUAL_SPI_PAGE_FLAG_ACK_ONLY,
                        0U,
                        true,
                        0U,
                        DUAL_SPI_STATUS_OK,
                        1U);
    CHECK(dual_spi_session_receive_page(session,
                                        peer_page,
                                        DUAL_SPI_PAGE_BYTES,
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_ACKED);
    CHECK(session->tx_state.synchronized);
}

static void test_reset_retransmits_until_matching_result(void)
{
    dual_spi_session_t session;
    dual_spi_page_view_t view;
    uint8_t tx_page[DUAL_SPI_PAGE_BYTES];
    uint8_t peer_page[DUAL_SPI_PAGE_BYTES];

    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 10U, 1U) ==
          DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_page_decode(tx_page, sizeof(tx_page), &view) ==
          DUAL_SPI_RESULT_OK);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_RESET_EPOCH) != 0U);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_RETRANSMIT) == 0U);
    CHECK(view.meta.tx_sequence == 0U);

    dual_spi_session_mark_tx_attempted(&session);
    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 20U, 1U) ==
          DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_page_decode(tx_page, sizeof(tx_page), &view) ==
          DUAL_SPI_RESULT_OK);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_RETRANSMIT) != 0U);

    acknowledge_local_reset(&session, tx_page, peer_page);
    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 30U, 1U) ==
          DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_page_decode(tx_page, sizeof(tx_page), &view) ==
          DUAL_SPI_RESULT_OK);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_ACK_ONLY) != 0U);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID) == 0U);
}

static void test_arbitrary_tlv_bytes_are_queued_and_encoded(void)
{
    static const uint8_t body_a[] = {0x00U, 0x7FU, 0x80U, 0xFFU};
    static const uint8_t body_b[] = {1U, 2U, 3U};
    dual_spi_session_t session;
    dual_spi_page_view_t view;
    dual_spi_tlv_iterator_t iterator;
    dual_spi_tlv_view_t message;
    bool has_value = false;
    uint8_t tx_page[DUAL_SPI_PAGE_BYTES];
    uint8_t peer_page[DUAL_SPI_PAGE_BYTES];

    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    acknowledge_local_reset(&session, tx_page, peer_page);

    CHECK(dual_spi_session_queue_tlv(&session,
                                     UINT16_C(0x0102),
                                     UINT16_C(0x1001),
                                     body_a,
                                     sizeof(body_a)) == DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_session_queue_tlv(&session,
                                     UINT16_C(0x0304),
                                     UINT16_C(0x1002),
                                     body_b,
                                     sizeof(body_b)) == DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 40U, 1U) ==
          DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_page_decode(tx_page, sizeof(tx_page), &view) ==
          DUAL_SPI_RESULT_OK);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID) != 0U);
    CHECK(view.message_count == 2U);

    dual_spi_tlv_iterator_init(&iterator,
                               view.payload,
                               view.payload_bytes,
                               view.message_count);
    CHECK(dual_spi_tlv_iterator_next(&iterator, &message, &has_value) ==
          DUAL_SPI_RESULT_OK);
    CHECK(has_value);
    CHECK(message.type == UINT16_C(0x0102));
    CHECK(message.tag == UINT16_C(0x1001));
    CHECK(message.length == sizeof(body_a));
    CHECK(memcmp(message.body, body_a, sizeof(body_a)) == 0);

    CHECK(dual_spi_tlv_iterator_next(&iterator, &message, &has_value) ==
          DUAL_SPI_RESULT_OK);
    CHECK(has_value);
    CHECK(message.type == UINT16_C(0x0304));
    CHECK(memcmp(message.body, body_b, sizeof(body_b)) == 0);
}

static void test_receive_dispatches_new_page_once_and_nacks_gap(void)
{
    static const uint8_t body[] = {9U, 8U, 7U, 6U};
    dual_spi_session_t session;
    dual_spi_page_meta_t meta;
    dual_spi_tlv_builder_t builder;
    dual_spi_rx_disposition_t disposition;
    dual_spi_tx_observation_t observation;
    dual_spi_page_view_t view;
    message_capture_t capture;
    uint8_t payload[32];
    uint8_t peer_page[DUAL_SPI_PAGE_BYTES];
    uint8_t tx_page[DUAL_SPI_PAGE_BYTES];

    memset(&capture, 0, sizeof(capture));
    dual_spi_session_init(&session, DUAL_SPI_LINK_A);

    encode_control_page(peer_page,
                        DUAL_SPI_PAGE_FLAG_ACK_ONLY |
                            DUAL_SPI_PAGE_FLAG_RESET_EPOCH |
                            DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID,
                        0U,
                        false,
                        0U,
                        DUAL_SPI_STATUS_OK,
                        1U);
    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_RESET_ACCEPTED);

    dual_spi_tlv_builder_init(&builder, payload, sizeof(payload));
    CHECK(dual_spi_tlv_builder_add(&builder,
                                   UINT16_C(0x2222),
                                   0U,
                                   UINT16_C(0xABCD),
                                   body,
                                   sizeof(body)) == DUAL_SPI_RESULT_OK);
    memset(&meta, 0, sizeof(meta));
    meta.link_id = DUAL_SPI_LINK_A;
    meta.page_flags = DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID;
    meta.tx_sequence = 1U;
    meta.rx_credit_pages = 1U;
    CHECK(dual_spi_page_encode(peer_page,
                               sizeof(peer_page),
                               &meta,
                               payload,
                               builder.used,
                               builder.message_count) == DUAL_SPI_RESULT_OK);

    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_ACCEPT_NEW);
    capture_queued_page(&session, &capture);
    CHECK(capture.count == 1U);
    CHECK(capture.last_type == UINT16_C(0x2222));
    CHECK(capture.last_tag == UINT16_C(0xABCD));
    CHECK(capture.last_length == sizeof(body));
    CHECK(memcmp(capture.last_body, body, sizeof(body)) == 0);

    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_DUPLICATE);
    CHECK(capture.count == 1U);

    meta.tx_sequence = 3U;
    CHECK(dual_spi_page_encode(peer_page,
                               sizeof(peer_page),
                               &meta,
                               payload,
                               builder.used,
                               builder.message_count) == DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_SEQUENCE_GAP);
    CHECK(capture.count == 1U);

    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 50U, 1U) ==
          DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_page_decode(tx_page, sizeof(tx_page), &view) ==
          DUAL_SPI_RESULT_OK);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID) != 0U);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_ERROR) != 0U);
    CHECK(view.meta.rx_result_sequence == 3U);
    CHECK(view.meta.status_code == DUAL_SPI_STATUS_SEQUENCE_GAP);
}

static void test_sequence_gap_resets_epoch_without_losing_outstanding_tlv(void)
{
    static const uint8_t body[] = {0xDEU, 0xADU, 0xBEU, 0xEFU};
    dual_spi_session_t session;
    dual_spi_page_view_t view;
    dual_spi_tlv_iterator_t iterator;
    dual_spi_tlv_view_t message;
    dual_spi_rx_disposition_t disposition;
    dual_spi_tx_observation_t observation;
    bool has_value = false;
    uint8_t tx_page[DUAL_SPI_PAGE_BYTES];
    uint8_t peer_page[DUAL_SPI_PAGE_BYTES];

    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    acknowledge_local_reset(&session, tx_page, peer_page);
    CHECK(dual_spi_session_queue_tlv(&session,
                                     UINT16_C(0x4040),
                                     UINT16_C(0x1234),
                                     body,
                                     sizeof(body)) == DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 60U, 1U) ==
          DUAL_SPI_RESULT_OK);
    dual_spi_session_mark_tx_attempted(&session);

    encode_control_page(peer_page,
                        DUAL_SPI_PAGE_FLAG_ACK_ONLY,
                        0U,
                        true,
                        1U,
                        DUAL_SPI_STATUS_SEQUENCE_GAP,
                        1U);
    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_RESET_REQUIRED);

    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 70U, 1U) ==
          DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_page_decode(tx_page, sizeof(tx_page), &view) ==
          DUAL_SPI_RESULT_OK);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_RESET_EPOCH) != 0U);
    dual_spi_session_mark_tx_attempted(&session);

    encode_control_page(peer_page,
                        DUAL_SPI_PAGE_FLAG_ACK_ONLY,
                        0U,
                        true,
                        0U,
                        DUAL_SPI_STATUS_OK,
                        1U);
    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_ACKED);

    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 80U, 1U) ==
          DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_page_decode(tx_page, sizeof(tx_page), &view) ==
          DUAL_SPI_RESULT_OK);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID) != 0U);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_RESET_EPOCH) == 0U);
    CHECK(view.meta.tx_sequence == 1U);
    CHECK(view.message_count == 1U);

    dual_spi_tlv_iterator_init(&iterator,
                               view.payload,
                               view.payload_bytes,
                               view.message_count);
    CHECK(dual_spi_tlv_iterator_next(&iterator, &message, &has_value) ==
          DUAL_SPI_RESULT_OK);
    CHECK(has_value);
    if (has_value)
    {
        CHECK(message.type == UINT16_C(0x4040));
        CHECK(message.tag == UINT16_C(0x1234));
        CHECK(memcmp(message.body, body, sizeof(body)) == 0);
    }
}

static void test_protocol_fault_nack_forces_reset_instead_of_endless_retransmit(void)
{
    static const uint8_t body[] = {0x11U, 0x22U};
    dual_spi_session_t session;
    dual_spi_page_view_t view;
    dual_spi_rx_disposition_t disposition;
    dual_spi_tx_observation_t observation;
    uint8_t tx_page[DUAL_SPI_PAGE_BYTES];
    uint8_t peer_page[DUAL_SPI_PAGE_BYTES];

    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    acknowledge_local_reset(&session, tx_page, peer_page);
    CHECK(dual_spi_session_queue_tlv(&session,
                                     UINT16_C(0x5050),
                                     1U,
                                     body,
                                     sizeof(body)) == DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 90U, 1U) ==
          DUAL_SPI_RESULT_OK);
    dual_spi_session_mark_tx_attempted(&session);

    encode_control_page(peer_page,
                        DUAL_SPI_PAGE_FLAG_ACK_ONLY,
                        0U,
                        true,
                        1U,
                        DUAL_SPI_STATUS_BAD_TLV,
                        1U);
    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_NACK);

    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 100U, 1U) ==
          DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_page_decode(tx_page, sizeof(tx_page), &view) ==
          DUAL_SPI_RESULT_OK);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_RESET_EPOCH) != 0U);
    CHECK(view.meta.tx_sequence == 0U);
}

static void test_bad_payload_length_nack_forces_reset(void)
{
    static const uint8_t body[] = {0x33U, 0x44U};
    dual_spi_session_t session;
    dual_spi_page_view_t view;
    dual_spi_rx_disposition_t disposition;
    dual_spi_tx_observation_t observation;
    uint8_t tx_page[DUAL_SPI_PAGE_BYTES];
    uint8_t peer_page[DUAL_SPI_PAGE_BYTES];

    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    acknowledge_local_reset(&session, tx_page, peer_page);
    CHECK(dual_spi_session_queue_tlv(&session,
                                     UINT16_C(0x5151),
                                     2U,
                                     body,
                                     sizeof(body)) == DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 105U, 1U) ==
          DUAL_SPI_RESULT_OK);
    dual_spi_session_mark_tx_attempted(&session);

    encode_control_page(peer_page,
                        DUAL_SPI_PAGE_FLAG_ACK_ONLY,
                        0U,
                        true,
                        1U,
                        DUAL_SPI_STATUS_BAD_PAYLOAD_LENGTH,
                        1U);
    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_NACK);
    CHECK(!session.tx_state.synchronized);

    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 106U, 1U) ==
          DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_page_decode(tx_page, sizeof(tx_page), &view) ==
          DUAL_SPI_RESULT_OK);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_RESET_EPOCH) != 0U);
    CHECK(view.meta.tx_sequence == 0U);
}

static void test_crc_trusted_protocol_errors_create_pending_nack(void)
{
    const uint32_t sequence = UINT32_C(0x12345678);
    dual_spi_session_t session;
    dual_spi_page_meta_t meta;
    dual_spi_page_view_t view;
    uint8_t payload[DUAL_SPI_TLV_HEADER_BYTES] = {0U};
    uint8_t peer_page[DUAL_SPI_PAGE_BYTES];
    uint8_t tx_page[DUAL_SPI_PAGE_BYTES];

    dual_spi_put_u16_le(&payload[0], UINT16_C(0x0101));
    memset(&meta, 0, sizeof(meta));
    meta.link_id = DUAL_SPI_LINK_A;
    meta.page_flags = DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID;
    meta.tx_sequence = sequence;
    meta.rx_credit_pages = 1U;
    CHECK(dual_spi_page_encode(peer_page,
                               sizeof(peer_page),
                               &meta,
                               payload,
                               sizeof(payload),
                               1U) == DUAL_SPI_RESULT_OK);
    peer_page[DUAL_SPI_HEADER_BYTES] = UINT8_C(0xFF);
    peer_page[DUAL_SPI_HEADER_BYTES + 1U] = UINT8_C(0xFF);
    refresh_page_crc(peer_page);
    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    check_pending_protocol_nack(&session,
                                peer_page,
                                DUAL_SPI_RESULT_BAD_TLV,
                                sequence,
                                DUAL_SPI_STATUS_BAD_TLV);

    encode_sequenced_page(peer_page, DUAL_SPI_LINK_A, sequence);
    peer_page[7] |= DUAL_SPI_PAGE_FLAG_MORE_FRAGMENTS;
    refresh_header_crc(peer_page);
    refresh_page_crc(peer_page);
    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    check_pending_protocol_nack(&session,
                                peer_page,
                                DUAL_SPI_RESULT_BAD_FLAGS,
                                sequence,
                                DUAL_SPI_STATUS_BAD_FLAGS);

    encode_sequenced_page(peer_page, DUAL_SPI_LINK_B, sequence);
    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    check_pending_protocol_nack(&session,
                                peer_page,
                                DUAL_SPI_RESULT_BAD_LINK_ID,
                                sequence,
                                DUAL_SPI_STATUS_BAD_LINK_ID);

    encode_sequenced_page(peer_page, DUAL_SPI_LINK_A, sequence);
    dual_spi_put_u16_le(&peer_page[26], 2U);
    refresh_header_crc(peer_page);
    refresh_page_crc(peer_page);
    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    check_pending_protocol_nack(&session,
                                peer_page,
                                DUAL_SPI_RESULT_BAD_CREDIT,
                                sequence,
                                DUAL_SPI_STATUS_BAD_CREDIT);

    encode_sequenced_page(peer_page, DUAL_SPI_LINK_A, sequence);
    dual_spi_put_u16_le(&peer_page[16],
                        DUAL_SPI_PAYLOAD_BYTES_MAX + 1U);
    refresh_header_crc(peer_page);
    refresh_page_crc(peer_page);
    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    check_pending_protocol_nack(&session,
                                peer_page,
                                DUAL_SPI_RESULT_BAD_PAYLOAD_LENGTH,
                                sequence,
                                DUAL_SPI_STATUS_BAD_PAYLOAD_LENGTH);

    CHECK(dual_spi_session_build_tx_page(&session, tx_page, 110U, 1U) ==
          DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_page_decode(tx_page, sizeof(tx_page), &view) ==
          DUAL_SPI_RESULT_OK);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID) != 0U);
    CHECK((view.meta.page_flags & DUAL_SPI_PAGE_FLAG_ERROR) != 0U);
    CHECK(view.meta.rx_result_sequence == sequence);
    CHECK(view.meta.status_code == DUAL_SPI_STATUS_BAD_PAYLOAD_LENGTH);
}

static void test_crc_errors_do_not_create_pending_result(void)
{
    const uint32_t sequence = UINT32_C(0x89ABCDEF);
    dual_spi_session_t session;
    dual_spi_rx_disposition_t disposition;
    dual_spi_tx_observation_t observation;
    uint8_t peer_page[DUAL_SPI_PAGE_BYTES];

    encode_sequenced_page(peer_page, DUAL_SPI_LINK_A, sequence);
    peer_page[20] ^= UINT8_C(0x01);
    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) ==
          DUAL_SPI_RESULT_BAD_HEADER_CRC);
    CHECK(!session.pending_result_valid);

    encode_sequenced_page(peer_page, DUAL_SPI_LINK_A, sequence);
    peer_page[100] ^= UINT8_C(0x01);
    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) ==
          DUAL_SPI_RESULT_BAD_PAGE_CRC);
    CHECK(!session.pending_result_valid);
}

static void test_rx_mailbox_backpressure_is_atomic(void)
{
    static const uint8_t body[] = {0x10U, 0x20U, 0x30U, 0x40U};
    dual_spi_session_t session;
    dual_spi_page_meta_t meta;
    dual_spi_tlv_builder_t builder;
    dual_spi_page_view_t queued_page;
    dual_spi_rx_disposition_t disposition;
    dual_spi_tx_observation_t observation;
    uint8_t payload[32];
    uint8_t peer_page[DUAL_SPI_PAGE_BYTES];

    dual_spi_session_init(&session, DUAL_SPI_LINK_A);
    encode_control_page(peer_page,
                        DUAL_SPI_PAGE_FLAG_ACK_ONLY |
                            DUAL_SPI_PAGE_FLAG_RESET_EPOCH |
                            DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID,
                        0U,
                        false,
                        0U,
                        DUAL_SPI_STATUS_OK,
                        1U);
    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_RESET_ACCEPTED);

    dual_spi_tlv_builder_init(&builder, payload, sizeof(payload));
    CHECK(dual_spi_tlv_builder_add(&builder,
                                   UINT16_C(0x6060),
                                   0U,
                                   UINT16_C(0x0101),
                                   body,
                                   sizeof(body)) == DUAL_SPI_RESULT_OK);
    memset(&meta, 0, sizeof(meta));
    meta.link_id = DUAL_SPI_LINK_A;
    meta.page_flags = DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID;
    meta.tx_sequence = 1U;
    meta.rx_credit_pages = 1U;
    CHECK(dual_spi_page_encode(peer_page,
                               sizeof(peer_page),
                               &meta,
                               payload,
                               builder.used,
                               builder.message_count) == DUAL_SPI_RESULT_OK);

    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_ACCEPT_NEW);
    CHECK(session.rx_state.expected_sequence == 2U);
    CHECK(dual_spi_session_rx_credit_pages(&session) == 0U);
    CHECK(dual_spi_session_peek_rx_page(&session, &queued_page));
    CHECK(queued_page.meta.tx_sequence == 1U);

    meta.tx_sequence = 2U;
    CHECK(dual_spi_page_encode(peer_page,
                               sizeof(peer_page),
                               &meta,
                               payload,
                               builder.used,
                               builder.message_count) == DUAL_SPI_RESULT_OK);
    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) ==
          DUAL_SPI_RESULT_NO_RX_CREDIT);
    CHECK(disposition == DUAL_SPI_RX_BACKPRESSURE);
    CHECK(session.rx_state.expected_sequence == 2U);
    CHECK(session.pending_result_sequence == 1U);
    CHECK(session.stats.backpressured_pages == 1U);
    CHECK(dual_spi_session_peek_rx_page(&session, &queued_page));
    CHECK(queued_page.meta.tx_sequence == 1U);

    dual_spi_session_release_rx_page(&session);
    CHECK(dual_spi_session_rx_credit_pages(&session) == 1U);
    CHECK(dual_spi_session_receive_page(&session,
                                        peer_page,
                                        sizeof(peer_page),
                                        &disposition,
                                        &observation) == DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_ACCEPT_NEW);
    CHECK(session.rx_state.expected_sequence == 3U);
    CHECK(session.pending_result_sequence == 2U);
    CHECK(dual_spi_session_peek_rx_page(&session, &queued_page));
    CHECK(queued_page.meta.tx_sequence == 2U);
    dual_spi_session_release_rx_page(&session);
}

int main(void)
{
    test_reset_retransmits_until_matching_result();
    test_arbitrary_tlv_bytes_are_queued_and_encoded();
    test_receive_dispatches_new_page_once_and_nacks_gap();
    test_sequence_gap_resets_epoch_without_losing_outstanding_tlv();
    test_protocol_fault_nack_forces_reset_instead_of_endless_retransmit();
    test_bad_payload_length_nack_forces_reset();
    test_crc_trusted_protocol_errors_create_pending_nack();
    test_crc_errors_do_not_create_pending_result();
    test_rx_mailbox_backpressure_is_atomic();

    if (g_failures != 0U)
    {
        printf("%u session test assertion(s) failed\n", g_failures);
        return 1;
    }

    puts("dual_spi_session_tests: PASS");
    return 0;
}


