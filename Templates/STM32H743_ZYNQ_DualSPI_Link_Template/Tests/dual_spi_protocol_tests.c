#include "dual_spi_protocol.h"

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

static void refresh_header_crc(uint8_t page[DUAL_SPI_PAGE_BYTES])
{
    dual_spi_put_u32_le(&page[28], dual_spi_crc32c(page, 28U));
}

static void refresh_page_crc(uint8_t page[DUAL_SPI_PAGE_BYTES])
{
    dual_spi_put_u32_le(&page[DUAL_SPI_PAGE_CRC_OFFSET],
                        dual_spi_crc32c(page, DUAL_SPI_PAGE_CRC_OFFSET));
}

static dual_spi_page_meta_t make_meta(void)
{
    dual_spi_page_meta_t meta;

    meta.link_id = DUAL_SPI_LINK_A;
    meta.page_flags = DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID |
                      DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID;
    meta.tx_sequence = UINT32_C(0x01020304);
    meta.rx_result_sequence = UINT32_C(0xA0B0C0D0);
    meta.sender_time_us = UINT32_C(0x11223344);
    meta.status_code = DUAL_SPI_STATUS_OK;
    meta.rx_credit_pages = 1U;
    return meta;
}

static void test_crc32c_standard_vector(void)
{
    static const uint8_t vector[] = "123456789";

    CHECK(dual_spi_crc32c(vector, sizeof(vector) - 1U) == UINT32_C(0xE3069283));
    CHECK(dual_spi_crc32c(NULL, 0U) == 0U);
}

static void test_little_endian_helpers(void)
{
    uint8_t bytes[4] = {0U};

    dual_spi_put_u16_le(bytes, UINT16_C(0xA1B2));
    CHECK(bytes[0] == UINT8_C(0xB2));
    CHECK(bytes[1] == UINT8_C(0xA1));
    CHECK(dual_spi_get_u16_le(bytes) == UINT16_C(0xA1B2));

    dual_spi_put_u32_le(bytes, UINT32_C(0x12345678));
    CHECK(bytes[0] == UINT8_C(0x78));
    CHECK(bytes[1] == UINT8_C(0x56));
    CHECK(bytes[2] == UINT8_C(0x34));
    CHECK(bytes[3] == UINT8_C(0x12));
    CHECK(dual_spi_get_u32_le(bytes) == UINT32_C(0x12345678));
}

static void test_tlv_builder_and_iterator(void)
{
    uint8_t payload[64];
    static const uint8_t body_a[] = {0x10U, 0x20U, 0x30U};
    static const uint8_t body_b[] = {0xAAU, 0xBBU, 0xCCU, 0xDDU};
    dual_spi_tlv_builder_t builder;
    dual_spi_tlv_iterator_t iterator;
    dual_spi_tlv_view_t view;
    bool has_value = false;

    memset(payload, 0xA5, sizeof(payload));
    dual_spi_tlv_builder_init(&builder, payload, (uint16_t)sizeof(payload));

    CHECK(dual_spi_tlv_builder_add(&builder,
                                   UINT16_C(0x0101),
                                   0U,
                                   UINT16_C(0x1234),
                                   body_a,
                                   (uint16_t)sizeof(body_a)) == DUAL_SPI_RESULT_OK);
    CHECK(builder.used == 12U);
    CHECK(builder.message_count == 1U);
    CHECK(payload[11] == 0U);

    CHECK(dual_spi_tlv_builder_add(&builder,
                                   UINT16_C(0x9000),
                                   0U,
                                   UINT16_C(0x5678),
                                   body_b,
                                   (uint16_t)sizeof(body_b)) == DUAL_SPI_RESULT_OK);
    CHECK(builder.used == 24U);
    CHECK(builder.message_count == 2U);

    dual_spi_tlv_iterator_init(&iterator,
                               payload,
                               builder.used,
                               builder.message_count);

    CHECK(dual_spi_tlv_iterator_next(&iterator, &view, &has_value) == DUAL_SPI_RESULT_OK);
    CHECK(has_value);
    CHECK(view.type == UINT16_C(0x0101));
    CHECK(view.flags == 0U);
    CHECK(view.length == sizeof(body_a));
    CHECK(view.tag == UINT16_C(0x1234));
    CHECK(memcmp(view.body, body_a, sizeof(body_a)) == 0);

    CHECK(dual_spi_tlv_iterator_next(&iterator, &view, &has_value) == DUAL_SPI_RESULT_OK);
    CHECK(has_value);
    CHECK(view.type == UINT16_C(0x9000));
    CHECK(view.length == sizeof(body_b));
    CHECK(memcmp(view.body, body_b, sizeof(body_b)) == 0);

    CHECK(dual_spi_tlv_iterator_next(&iterator, &view, &has_value) == DUAL_SPI_RESULT_OK);
    CHECK(!has_value);
}

static void test_tlv_rejects_capacity_truncation_and_nonzero_padding(void)
{
    uint8_t payload[16] = {0U};
    static const uint8_t body[5] = {1U, 2U, 3U, 4U, 5U};
    dual_spi_tlv_builder_t builder;
    dual_spi_tlv_iterator_t iterator;
    dual_spi_tlv_view_t view;
    bool has_value = false;

    dual_spi_tlv_builder_init(&builder, payload, 12U);
    CHECK(dual_spi_tlv_builder_add(&builder,
                                   1U,
                                   0U,
                                   0U,
                                   body,
                                   (uint16_t)sizeof(body)) == DUAL_SPI_RESULT_NO_SPACE);

    dual_spi_put_u16_le(&payload[0], 1U);
    dual_spi_put_u16_le(&payload[2], 0U);
    dual_spi_put_u16_le(&payload[4], 1U);
    dual_spi_put_u16_le(&payload[6], 0U);
    payload[8] = 0x55U;
    payload[9] = 0x7EU;

    dual_spi_tlv_iterator_init(&iterator, payload, 12U, 1U);
    CHECK(dual_spi_tlv_iterator_next(&iterator, &view, &has_value) == DUAL_SPI_RESULT_BAD_TLV);

    dual_spi_tlv_builder_init(&builder, payload, (uint16_t)sizeof(payload));
    CHECK(dual_spi_tlv_builder_add(&builder,
                                   1U,
                                   1U,
                                   0U,
                                   NULL,
                                   0U) == DUAL_SPI_RESULT_BAD_TLV);

    dual_spi_tlv_iterator_init(&iterator, payload, 8U, 1U);
    CHECK(dual_spi_tlv_iterator_next(&iterator, &view, &has_value) == DUAL_SPI_RESULT_BAD_TLV);
}

static void test_tlv_rejects_16bit_alignment_overflow(void)
{
    static uint8_t payload[UINT16_MAX];
    static uint8_t body[UINT16_MAX - DUAL_SPI_TLV_HEADER_BYTES];
    uint8_t encoded_header[DUAL_SPI_TLV_HEADER_BYTES] = {0U};
    dual_spi_tlv_builder_t builder;
    dual_spi_tlv_iterator_t iterator;
    dual_spi_tlv_view_t view;
    bool has_value = false;

    dual_spi_tlv_builder_init(&builder,
                              payload,
                              UINT16_MAX);
    CHECK(dual_spi_tlv_builder_add(
              &builder,
              UINT16_C(0x0101),
              0U,
              0U,
              body,
              (uint16_t)sizeof(body)) == DUAL_SPI_RESULT_NO_SPACE);
    CHECK(builder.used == 0U);
    CHECK(builder.message_count == 0U);

    dual_spi_put_u16_le(&encoded_header[0], UINT16_C(0x0101));
    dual_spi_put_u16_le(&encoded_header[4],
                        (uint16_t)sizeof(body));
    dual_spi_tlv_iterator_init(&iterator,
                               encoded_header,
                               (uint16_t)sizeof(encoded_header),
                               1U);
    CHECK(dual_spi_tlv_iterator_next(&iterator,
                                     &view,
                                     &has_value) ==
          DUAL_SPI_RESULT_BAD_TLV);
    CHECK(!has_value);
}

static void test_tlv_iterator_rejects_reserved_type(void)
{
    uint8_t payload[DUAL_SPI_TLV_HEADER_BYTES] = {0U};
    dual_spi_tlv_iterator_t iterator;
    dual_spi_tlv_view_t view;
    bool has_value = false;

    dual_spi_put_u16_le(&payload[0], UINT16_C(0xFFFF));
    dual_spi_tlv_iterator_init(&iterator,
                               payload,
                               (uint16_t)sizeof(payload),
                               1U);

    CHECK(dual_spi_tlv_iterator_next(&iterator, &view, &has_value) ==
          DUAL_SPI_RESULT_BAD_TLV);
    CHECK(!has_value);
}

static void test_page_round_trip(void)
{
    uint8_t page[DUAL_SPI_PAGE_BYTES];
    uint8_t payload[64];
    static const uint8_t body[] = {9U, 8U, 7U, 6U, 5U};
    dual_spi_tlv_builder_t builder;
    dual_spi_page_meta_t meta = make_meta();
    dual_spi_page_view_t decoded;

    dual_spi_tlv_builder_init(&builder, payload, (uint16_t)sizeof(payload));
    CHECK(dual_spi_tlv_builder_add(&builder,
                                   UINT16_C(0x0201),
                                   0U,
                                   UINT16_C(0x0042),
                                   body,
                                   (uint16_t)sizeof(body)) == DUAL_SPI_RESULT_OK);

    CHECK(dual_spi_page_encode(page,
                               sizeof(page),
                               &meta,
                               payload,
                               builder.used,
                               builder.message_count) == DUAL_SPI_RESULT_OK);
    CHECK(memcmp(page, "DSP1", 4U) == 0);
    CHECK(page[4] == DUAL_SPI_VERSION_MAJOR);
    CHECK(page[5] == DUAL_SPI_HEADER_BYTES);
    CHECK(dual_spi_get_u32_le(&page[8]) == meta.tx_sequence);
    CHECK(dual_spi_get_u32_le(&page[28]) == dual_spi_crc32c(page, 28U));
    CHECK(dual_spi_get_u32_le(&page[DUAL_SPI_PAGE_CRC_OFFSET]) ==
          dual_spi_crc32c(page, DUAL_SPI_PAGE_CRC_OFFSET));

    CHECK(dual_spi_page_decode(page, sizeof(page), &decoded) == DUAL_SPI_RESULT_OK);
    CHECK(decoded.meta.link_id == meta.link_id);
    CHECK(decoded.meta.page_flags == meta.page_flags);
    CHECK(decoded.meta.tx_sequence == meta.tx_sequence);
    CHECK(decoded.meta.rx_result_sequence == meta.rx_result_sequence);
    CHECK(decoded.meta.sender_time_us == meta.sender_time_us);
    CHECK(decoded.meta.status_code == meta.status_code);
    CHECK(decoded.meta.rx_credit_pages == meta.rx_credit_pages);
    CHECK(decoded.payload_bytes == builder.used);
    CHECK(decoded.message_count == builder.message_count);
    CHECK(memcmp(decoded.payload, payload, builder.used) == 0);

    for (size_t index = DUAL_SPI_HEADER_BYTES + builder.used;
         index < DUAL_SPI_PAGE_CRC_OFFSET;
         ++index)
    {
        CHECK(page[index] == 0U);
    }
}

static void test_page_detects_crc_and_length_errors(void)
{
    uint8_t page[DUAL_SPI_PAGE_BYTES];
    dual_spi_page_meta_t meta = make_meta();
    dual_spi_page_view_t decoded;

    meta.page_flags = DUAL_SPI_PAGE_FLAG_ACK_ONLY |
                      DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID;
    meta.tx_sequence = 0U;
    CHECK(dual_spi_page_encode(page,
                               sizeof(page),
                               &meta,
                               NULL,
                               0U,
                               0U) == DUAL_SPI_RESULT_OK);

    page[20] ^= 0x01U;
    CHECK(dual_spi_page_decode(page, sizeof(page), &decoded) ==
          DUAL_SPI_RESULT_BAD_HEADER_CRC);
    page[20] ^= 0x01U;

    page[100] ^= 0x01U;
    CHECK(dual_spi_page_decode(page, sizeof(page), &decoded) ==
          DUAL_SPI_RESULT_BAD_PAGE_CRC);
    page[100] ^= 0x01U;

    page[100] = 0x01U;
    refresh_page_crc(page);
    CHECK(dual_spi_page_decode(page, sizeof(page), &decoded) ==
          DUAL_SPI_RESULT_BAD_TLV);
    page[100] = 0U;
    refresh_page_crc(page);

    dual_spi_put_u16_le(&page[16], DUAL_SPI_PAYLOAD_BYTES_MAX + 1U);
    refresh_header_crc(page);
    refresh_page_crc(page);
    CHECK(dual_spi_page_decode(page, sizeof(page), &decoded) ==
          DUAL_SPI_RESULT_BAD_PAYLOAD_LENGTH);

    CHECK(dual_spi_page_decode(page, DUAL_SPI_PAGE_BYTES - 1U, &decoded) ==
          DUAL_SPI_RESULT_BAD_PAYLOAD_LENGTH);
}

static void test_page_rejects_invalid_metadata(void)
{
    uint8_t page[DUAL_SPI_PAGE_BYTES];
    dual_spi_page_meta_t meta = make_meta();

    CHECK(dual_spi_page_encode(page,
                               sizeof(page),
                               &meta,
                               NULL,
                               DUAL_SPI_PAYLOAD_BYTES_MAX + 1U,
                               0U) == DUAL_SPI_RESULT_BAD_PAYLOAD_LENGTH);

    meta.link_id = 2U;
    CHECK(dual_spi_page_encode(page, sizeof(page), &meta, NULL, 0U, 0U) ==
          DUAL_SPI_RESULT_BAD_LINK_ID);

    meta = make_meta();
    meta.rx_credit_pages = 2U;
    CHECK(dual_spi_page_encode(page, sizeof(page), &meta, NULL, 0U, 0U) ==
          DUAL_SPI_RESULT_BAD_CREDIT);

    meta = make_meta();
    meta.status_code = DUAL_SPI_STATUS_BAD_TLV;
    CHECK(dual_spi_page_encode(page, sizeof(page), &meta, NULL, 0U, 0U) ==
          DUAL_SPI_RESULT_BAD_FLAGS);

    meta.page_flags |= DUAL_SPI_PAGE_FLAG_ERROR;
    CHECK(dual_spi_page_encode(page, sizeof(page), &meta, NULL, 0U, 0U) ==
          DUAL_SPI_RESULT_OK);

    meta = make_meta();
    meta.page_flags = DUAL_SPI_PAGE_FLAG_ACK_ONLY |
                      DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID;
    meta.tx_sequence = 0U;
    meta.rx_result_sequence = 0U;
    CHECK(dual_spi_page_encode(page, sizeof(page), &meta, NULL, 0U, 0U) ==
          DUAL_SPI_RESULT_BAD_FLAGS);

    meta.page_flags |= DUAL_SPI_PAGE_FLAG_RESET_EPOCH;
    CHECK(dual_spi_page_encode(page, sizeof(page), &meta, NULL, 0U, 0U) ==
          DUAL_SPI_RESULT_OK);

    meta.tx_sequence = 1U;
    CHECK(dual_spi_page_encode(page, sizeof(page), &meta, NULL, 0U, 0U) ==
          DUAL_SPI_RESULT_BAD_FLAGS);

    meta = make_meta();
    meta.page_flags |= DUAL_SPI_PAGE_FLAG_MORE_FRAGMENTS;
    CHECK(dual_spi_page_encode(page, sizeof(page), &meta, NULL, 0U, 0U) ==
          DUAL_SPI_RESULT_BAD_FLAGS);
}

static void test_page_decode_rejects_reserved_flag_bits(void)
{
    uint8_t page[DUAL_SPI_PAGE_BYTES];
    dual_spi_page_meta_t meta = make_meta();
    dual_spi_page_view_t decoded;

    CHECK(dual_spi_page_encode(page,
                               sizeof(page),
                               &meta,
                               NULL,
                               0U,
                               0U) == DUAL_SPI_RESULT_OK);
    page[7] |= UINT8_C(0x80);
    refresh_header_crc(page);
    refresh_page_crc(page);

    CHECK(dual_spi_page_decode(page, sizeof(page), &decoded) ==
          DUAL_SPI_RESULT_BAD_FLAGS);
}

static dual_spi_page_view_t make_sequence_page(uint8_t flags,
                                               uint32_t tx_sequence,
                                               uint16_t credit)
{
    dual_spi_page_view_t page;

    memset(&page, 0, sizeof(page));
    page.meta.link_id = DUAL_SPI_LINK_A;
    page.meta.page_flags = flags;
    page.meta.tx_sequence = tx_sequence;
    page.meta.rx_credit_pages = credit;
    return page;
}

static void test_rx_sequence_reset_duplicate_gap_and_wrap(void)
{
    dual_spi_rx_sequence_state_t state;
    dual_spi_rx_disposition_t disposition;
    dual_spi_page_view_t page;

    dual_spi_rx_sequence_init(&state);
    CHECK(!state.synchronized);
    CHECK(state.expected_sequence == 0U);

    page = make_sequence_page(DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID, 0U, 1U);
    CHECK(dual_spi_rx_sequence_accept(&state, &page, &disposition) ==
          DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_SEQUENCE_GAP);
    CHECK(!state.synchronized);

    page = make_sequence_page(DUAL_SPI_PAGE_FLAG_ACK_ONLY |
                                  DUAL_SPI_PAGE_FLAG_RESET_EPOCH |
                                  DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID,
                              0U,
                              1U);
    CHECK(dual_spi_rx_sequence_accept(&state, &page, &disposition) ==
          DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_RESET_ACCEPTED);
    CHECK(state.synchronized);
    CHECK(state.expected_sequence == 1U);

    CHECK(dual_spi_rx_sequence_accept(&state, &page, &disposition) ==
          DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_RESET_DUPLICATE);
    CHECK(state.expected_sequence == 1U);

    page = make_sequence_page(DUAL_SPI_PAGE_FLAG_ACK_ONLY, 0U, 1U);
    CHECK(dual_spi_rx_sequence_accept(&state, &page, &disposition) ==
          DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_UNSEQUENCED);
    CHECK(state.expected_sequence == 1U);

    page = make_sequence_page(DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID, 1U, 1U);
    CHECK(dual_spi_rx_sequence_accept(&state, &page, &disposition) ==
          DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_ACCEPT_NEW);
    CHECK(state.expected_sequence == 2U);

    CHECK(dual_spi_rx_sequence_accept(&state, &page, &disposition) ==
          DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_DUPLICATE);
    CHECK(state.expected_sequence == 2U);

    page.meta.tx_sequence = 3U;
    CHECK(dual_spi_rx_sequence_accept(&state, &page, &disposition) ==
          DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_SEQUENCE_GAP);
    CHECK(state.expected_sequence == 2U);

    state.synchronized = true;
    state.expected_sequence = UINT32_MAX;
    page.meta.tx_sequence = UINT32_MAX;
    CHECK(dual_spi_rx_sequence_accept(&state, &page, &disposition) ==
          DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_ACCEPT_NEW);
    CHECK(state.expected_sequence == 0U);

    page.meta.tx_sequence = 0U;
    CHECK(dual_spi_rx_sequence_accept(&state, &page, &disposition) ==
          DUAL_SPI_RESULT_OK);
    CHECK(disposition == DUAL_SPI_RX_ACCEPT_NEW);
    CHECK(state.expected_sequence == 1U);
}

static void test_tx_outstanding_result_and_credit_state(void)
{
    dual_spi_tx_state_t state;
    dual_spi_tx_observation_t observation;
    dual_spi_page_view_t peer;
    uint32_t sequence = UINT32_MAX;

    dual_spi_tx_state_init(&state);
    CHECK(!state.synchronized);
    CHECK(!state.outstanding);
    CHECK(state.peer_credit_pages == 0U);

    CHECK(dual_spi_tx_begin_data(&state, &sequence) ==
          DUAL_SPI_RESULT_NOT_SYNCHRONIZED);

    CHECK(dual_spi_tx_begin_reset(&state, &sequence) == DUAL_SPI_RESULT_OK);
    CHECK(sequence == 0U);
    CHECK(state.outstanding);
    CHECK(state.outstanding_is_reset);
    CHECK(state.outstanding_sequence == 0U);

    peer = make_sequence_page(DUAL_SPI_PAGE_FLAG_ACK_ONLY |
                                  DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID,
                              0U,
                              1U);
    peer.meta.rx_result_sequence = 0U;
    peer.meta.status_code = DUAL_SPI_STATUS_OK;
    CHECK(dual_spi_tx_observe_peer(&state, &peer, &observation) ==
          DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_ACKED);
    CHECK(state.synchronized);
    CHECK(!state.outstanding);
    CHECK(state.peer_credit_pages == 1U);
    CHECK(state.next_sequence == 1U);

    CHECK(dual_spi_tx_begin_data(&state, &sequence) == DUAL_SPI_RESULT_OK);
    CHECK(sequence == 1U);
    CHECK(state.outstanding);
    CHECK(!state.outstanding_is_reset);
    CHECK(state.peer_credit_pages == 0U);
    CHECK(state.next_sequence == 2U);

    CHECK(dual_spi_tx_begin_data(&state, &sequence) ==
          DUAL_SPI_RESULT_TX_OUTSTANDING);
    CHECK(dual_spi_tx_get_retransmit(&state, &sequence) == DUAL_SPI_RESULT_OK);
    CHECK(sequence == 1U);

    peer = make_sequence_page(DUAL_SPI_PAGE_FLAG_ACK_ONLY, 0U, 1U);
    CHECK(dual_spi_tx_observe_peer(&state, &peer, &observation) ==
          DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_NO_RESULT);
    CHECK(state.outstanding);
    CHECK(state.peer_credit_pages == 1U);

    peer.meta.page_flags |= DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID;
    peer.meta.rx_result_sequence = 0U;
    CHECK(dual_spi_tx_observe_peer(&state, &peer, &observation) ==
          DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_STALE_RESULT);
    CHECK(state.outstanding);

    peer.meta.rx_result_sequence = 1U;
    CHECK(dual_spi_tx_observe_peer(&state, &peer, &observation) ==
          DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_ACKED);
    CHECK(!state.outstanding);

    CHECK(dual_spi_tx_begin_data(&state, &sequence) == DUAL_SPI_RESULT_OK);
    CHECK(sequence == 2U);
    CHECK(state.peer_credit_pages == 0U);

    peer.meta.rx_result_sequence = 2U;
    peer.meta.status_code = DUAL_SPI_STATUS_NO_RX_CREDIT;
    peer.meta.page_flags |= DUAL_SPI_PAGE_FLAG_ERROR;
    peer.meta.rx_credit_pages = 0U;
    CHECK(dual_spi_tx_observe_peer(&state, &peer, &observation) ==
          DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_WAIT_CREDIT);
    CHECK(state.outstanding);

    peer.meta.page_flags = DUAL_SPI_PAGE_FLAG_ACK_ONLY;
    peer.meta.status_code = DUAL_SPI_STATUS_OK;
    peer.meta.rx_credit_pages = 1U;
    CHECK(dual_spi_tx_observe_peer(&state, &peer, &observation) ==
          DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_NO_RESULT);
    CHECK(state.peer_credit_pages == 1U);
    CHECK(state.outstanding);

    CHECK(dual_spi_tx_get_retransmit(&state, &sequence) == DUAL_SPI_RESULT_OK);
    CHECK(sequence == 2U);
    CHECK(state.peer_credit_pages == 1U);

    peer.meta.page_flags = DUAL_SPI_PAGE_FLAG_ACK_ONLY |
                           DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID |
                           DUAL_SPI_PAGE_FLAG_ERROR;
    peer.meta.rx_result_sequence = 2U;
    peer.meta.status_code = DUAL_SPI_STATUS_SEQUENCE_GAP;
    CHECK(dual_spi_tx_observe_peer(&state, &peer, &observation) ==
          DUAL_SPI_RESULT_OK);
    CHECK(observation == DUAL_SPI_TX_OBSERVE_RESET_REQUIRED);
    CHECK(!state.synchronized);
    CHECK(state.outstanding);

    CHECK(dual_spi_tx_begin_reset(&state, &sequence) == DUAL_SPI_RESULT_OK);
    CHECK(sequence == 0U);
    CHECK(state.outstanding);
    CHECK(state.outstanding_is_reset);
    CHECK(state.peer_credit_pages == 0U);
}

int main(void)
{
    test_crc32c_standard_vector();
    test_little_endian_helpers();
    test_tlv_builder_and_iterator();
    test_tlv_rejects_capacity_truncation_and_nonzero_padding();
    test_tlv_rejects_16bit_alignment_overflow();
    test_tlv_iterator_rejects_reserved_type();
    test_page_round_trip();
    test_page_detects_crc_and_length_errors();
    test_page_rejects_invalid_metadata();
    test_page_decode_rejects_reserved_flag_bits();
    test_rx_sequence_reset_duplicate_gap_and_wrap();
    test_tx_outstanding_result_and_credit_state();

    if (g_failures != 0U)
    {
        printf("%u protocol test assertion(s) failed\n", g_failures);
        return 1;
    }

    puts("dual_spi_protocol_tests: PASS");
    return 0;
}


