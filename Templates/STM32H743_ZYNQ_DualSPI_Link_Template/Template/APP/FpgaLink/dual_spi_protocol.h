#ifndef DUAL_SPI_PROTOCOL_H
#define DUAL_SPI_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DUAL_SPI_PAGE_BYTES          1024U
#define DUAL_SPI_HEADER_BYTES        32U
#define DUAL_SPI_PAYLOAD_BYTES_MAX   988U
#define DUAL_SPI_PAGE_CRC_OFFSET     1020U
#define DUAL_SPI_TLV_HEADER_BYTES    8U
#define DUAL_SPI_MAGIC               UINT32_C(0x31505344)
#define DUAL_SPI_VERSION_MAJOR       1U

#define DUAL_SPI_PAGE_FLAG_ACK_ONLY       UINT8_C(0x01)
#define DUAL_SPI_PAGE_FLAG_RETRANSMIT     UINT8_C(0x02)
#define DUAL_SPI_PAGE_FLAG_RESET_EPOCH    UINT8_C(0x04)
#define DUAL_SPI_PAGE_FLAG_ERROR          UINT8_C(0x08)
#define DUAL_SPI_PAGE_FLAG_MORE_FRAGMENTS UINT8_C(0x10)
#define DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID UINT8_C(0x20)
#define DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID    UINT8_C(0x40)
#define DUAL_SPI_PAGE_FLAG_KNOWN_MASK         UINT8_C(0x7F)

typedef enum
{
    DUAL_SPI_LINK_A = 0,
    DUAL_SPI_LINK_B = 1
} dual_spi_link_id_t;

typedef enum
{
    DUAL_SPI_STATUS_OK = 0x0000,
    DUAL_SPI_STATUS_BAD_MAGIC = 0x0001,
    DUAL_SPI_STATUS_UNSUPPORTED_VERSION = 0x0002,
    DUAL_SPI_STATUS_BAD_HEADER_LENGTH = 0x0003,
    DUAL_SPI_STATUS_BAD_HEADER_CRC = 0x0004,
    DUAL_SPI_STATUS_BAD_PAGE_CRC = 0x0005,
    DUAL_SPI_STATUS_BAD_PAYLOAD_LENGTH = 0x0006,
    DUAL_SPI_STATUS_BAD_TLV = 0x0007,
    DUAL_SPI_STATUS_SEQUENCE_GAP = 0x0008,
    DUAL_SPI_STATUS_NO_RX_CREDIT = 0x0009,
    DUAL_SPI_STATUS_LINK_TIMEOUT = 0x000A,
    DUAL_SPI_STATUS_BAD_LINK_ID = 0x000B,
    DUAL_SPI_STATUS_BAD_FLAGS = 0x000C,
    DUAL_SPI_STATUS_BAD_CREDIT = 0x000D
} dual_spi_status_code_t;

typedef enum
{
    DUAL_SPI_RESULT_OK = 0,
    DUAL_SPI_RESULT_INVALID_ARGUMENT,
    DUAL_SPI_RESULT_NO_SPACE,
    DUAL_SPI_RESULT_BAD_MAGIC,
    DUAL_SPI_RESULT_UNSUPPORTED_VERSION,
    DUAL_SPI_RESULT_BAD_HEADER_LENGTH,
    DUAL_SPI_RESULT_BAD_HEADER_CRC,
    DUAL_SPI_RESULT_BAD_PAGE_CRC,
    DUAL_SPI_RESULT_BAD_PAYLOAD_LENGTH,
    DUAL_SPI_RESULT_BAD_TLV,
    DUAL_SPI_RESULT_BAD_LINK_ID,
    DUAL_SPI_RESULT_BAD_CREDIT,
    DUAL_SPI_RESULT_BAD_FLAGS,
    DUAL_SPI_RESULT_NOT_SYNCHRONIZED,
    DUAL_SPI_RESULT_TX_OUTSTANDING,
    DUAL_SPI_RESULT_NO_TX_OUTSTANDING,
    DUAL_SPI_RESULT_NO_RX_CREDIT
} dual_spi_result_t;

typedef struct
{
    uint8_t link_id;
    uint8_t page_flags;
    uint32_t tx_sequence;
    uint32_t rx_result_sequence;
    uint32_t sender_time_us;
    uint16_t status_code;
    uint16_t rx_credit_pages;
} dual_spi_page_meta_t;

typedef struct
{
    dual_spi_page_meta_t meta;
    uint16_t payload_bytes;
    uint16_t message_count;
    const uint8_t *payload;
} dual_spi_page_view_t;

typedef struct
{
    bool error_result_eligible;
    uint32_t trusted_tx_sequence;
    uint16_t error_status_code;
} dual_spi_page_decode_info_t;

typedef struct
{
    uint8_t *payload;
    uint16_t capacity;
    uint16_t used;
    uint16_t message_count;
} dual_spi_tlv_builder_t;

typedef struct
{
    uint16_t type;
    uint16_t flags;
    uint16_t length;
    uint16_t tag;
    const uint8_t *body;
} dual_spi_tlv_view_t;

typedef struct
{
    const uint8_t *payload;
    uint16_t payload_bytes;
    uint16_t expected_count;
    uint16_t offset;
    uint16_t seen_count;
} dual_spi_tlv_iterator_t;

typedef enum
{
    DUAL_SPI_RX_UNSEQUENCED = 0,
    DUAL_SPI_RX_ACCEPT_NEW,
    DUAL_SPI_RX_DUPLICATE,
    DUAL_SPI_RX_RESET_ACCEPTED,
    DUAL_SPI_RX_RESET_DUPLICATE,
    DUAL_SPI_RX_SEQUENCE_GAP,
    DUAL_SPI_RX_BACKPRESSURE
} dual_spi_rx_disposition_t;

typedef struct
{
    bool synchronized;
    uint32_t expected_sequence;
} dual_spi_rx_sequence_state_t;

typedef enum
{
    DUAL_SPI_TX_OBSERVE_NO_RESULT = 0,
    DUAL_SPI_TX_OBSERVE_ACKED,
    DUAL_SPI_TX_OBSERVE_STALE_RESULT,
    DUAL_SPI_TX_OBSERVE_WAIT_CREDIT,
    DUAL_SPI_TX_OBSERVE_NACK,
    DUAL_SPI_TX_OBSERVE_RESET_REQUIRED
} dual_spi_tx_observation_t;

typedef struct
{
    bool synchronized;
    bool outstanding;
    bool outstanding_is_reset;
    uint8_t peer_credit_pages;
    uint32_t next_sequence;
    uint32_t outstanding_sequence;
} dual_spi_tx_state_t;

uint16_t dual_spi_get_u16_le(const uint8_t *src);
uint32_t dual_spi_get_u32_le(const uint8_t *src);
void dual_spi_put_u16_le(uint8_t *dst, uint16_t value);
void dual_spi_put_u32_le(uint8_t *dst, uint32_t value);

uint32_t dual_spi_crc32c(const void *data, size_t length);

void dual_spi_tlv_builder_init(dual_spi_tlv_builder_t *builder,
                               uint8_t *payload,
                               uint16_t capacity);
dual_spi_result_t dual_spi_tlv_builder_add(dual_spi_tlv_builder_t *builder,
                                          uint16_t type,
                                          uint16_t flags,
                                          uint16_t tag,
                                          const void *body,
                                          uint16_t body_bytes);

void dual_spi_tlv_iterator_init(dual_spi_tlv_iterator_t *iterator,
                                const uint8_t *payload,
                                uint16_t payload_bytes,
                                uint16_t expected_count);
dual_spi_result_t dual_spi_tlv_iterator_next(dual_spi_tlv_iterator_t *iterator,
                                             dual_spi_tlv_view_t *view,
                                             bool *has_value);

dual_spi_result_t dual_spi_page_encode(uint8_t *page,
                                       size_t page_bytes,
                                       const dual_spi_page_meta_t *meta,
                                       const uint8_t *payload,
                                       uint16_t payload_bytes,
                                       uint16_t message_count);
dual_spi_result_t dual_spi_page_decode(const uint8_t *page,
                                       size_t page_bytes,
                                       dual_spi_page_view_t *view);
dual_spi_result_t dual_spi_page_decode_with_info(
    const uint8_t *page,
    size_t page_bytes,
    dual_spi_page_view_t *view,
    dual_spi_page_decode_info_t *decode_info);

void dual_spi_rx_sequence_init(dual_spi_rx_sequence_state_t *state);
dual_spi_result_t dual_spi_rx_sequence_accept(dual_spi_rx_sequence_state_t *state,
                                              const dual_spi_page_view_t *page,
                                              dual_spi_rx_disposition_t *disposition);

void dual_spi_tx_state_init(dual_spi_tx_state_t *state);
dual_spi_result_t dual_spi_tx_begin_reset(dual_spi_tx_state_t *state,
                                          uint32_t *sequence);
dual_spi_result_t dual_spi_tx_begin_data(dual_spi_tx_state_t *state,
                                         uint32_t *sequence);
dual_spi_result_t dual_spi_tx_get_retransmit(const dual_spi_tx_state_t *state,
                                             uint32_t *sequence);
dual_spi_result_t dual_spi_tx_observe_peer(dual_spi_tx_state_t *state,
                                          const dual_spi_page_view_t *page,
                                          dual_spi_tx_observation_t *observation);

#ifdef __cplusplus
}
#endif

#endif




