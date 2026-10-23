#include "dual_spi_protocol.h"

#include <string.h>

enum
{
    DUAL_SPI_OFFSET_MAGIC = 0,
    DUAL_SPI_OFFSET_VERSION = 4,
    DUAL_SPI_OFFSET_HEADER_BYTES = 5,
    DUAL_SPI_OFFSET_LINK_ID = 6,
    DUAL_SPI_OFFSET_PAGE_FLAGS = 7,
    DUAL_SPI_OFFSET_TX_SEQUENCE = 8,
    DUAL_SPI_OFFSET_RX_RESULT_SEQUENCE = 12,
    DUAL_SPI_OFFSET_PAYLOAD_BYTES = 16,
    DUAL_SPI_OFFSET_MESSAGE_COUNT = 18,
    DUAL_SPI_OFFSET_SENDER_TIME_US = 20,
    DUAL_SPI_OFFSET_STATUS_CODE = 24,
    DUAL_SPI_OFFSET_RX_CREDIT_PAGES = 26,
    DUAL_SPI_OFFSET_HEADER_CRC = 28,
    DUAL_SPI_OFFSET_PAYLOAD = 32
};

static const uint32_t g_crc32c_table[256] = {
    UINT32_C(0x00000000), UINT32_C(0xF26B8303), UINT32_C(0xE13B70F7), UINT32_C(0x1350F3F4),
    UINT32_C(0xC79A971F), UINT32_C(0x35F1141C), UINT32_C(0x26A1E7E8), UINT32_C(0xD4CA64EB),
    UINT32_C(0x8AD958CF), UINT32_C(0x78B2DBCC), UINT32_C(0x6BE22838), UINT32_C(0x9989AB3B),
    UINT32_C(0x4D43CFD0), UINT32_C(0xBF284CD3), UINT32_C(0xAC78BF27), UINT32_C(0x5E133C24),
    UINT32_C(0x105EC76F), UINT32_C(0xE235446C), UINT32_C(0xF165B798), UINT32_C(0x030E349B),
    UINT32_C(0xD7C45070), UINT32_C(0x25AFD373), UINT32_C(0x36FF2087), UINT32_C(0xC494A384),
    UINT32_C(0x9A879FA0), UINT32_C(0x68EC1CA3), UINT32_C(0x7BBCEF57), UINT32_C(0x89D76C54),
    UINT32_C(0x5D1D08BF), UINT32_C(0xAF768BBC), UINT32_C(0xBC267848), UINT32_C(0x4E4DFB4B),
    UINT32_C(0x20BD8EDE), UINT32_C(0xD2D60DDD), UINT32_C(0xC186FE29), UINT32_C(0x33ED7D2A),
    UINT32_C(0xE72719C1), UINT32_C(0x154C9AC2), UINT32_C(0x061C6936), UINT32_C(0xF477EA35),
    UINT32_C(0xAA64D611), UINT32_C(0x580F5512), UINT32_C(0x4B5FA6E6), UINT32_C(0xB93425E5),
    UINT32_C(0x6DFE410E), UINT32_C(0x9F95C20D), UINT32_C(0x8CC531F9), UINT32_C(0x7EAEB2FA),
    UINT32_C(0x30E349B1), UINT32_C(0xC288CAB2), UINT32_C(0xD1D83946), UINT32_C(0x23B3BA45),
    UINT32_C(0xF779DEAE), UINT32_C(0x05125DAD), UINT32_C(0x1642AE59), UINT32_C(0xE4292D5A),
    UINT32_C(0xBA3A117E), UINT32_C(0x4851927D), UINT32_C(0x5B016189), UINT32_C(0xA96AE28A),
    UINT32_C(0x7DA08661), UINT32_C(0x8FCB0562), UINT32_C(0x9C9BF696), UINT32_C(0x6EF07595),
    UINT32_C(0x417B1DBC), UINT32_C(0xB3109EBF), UINT32_C(0xA0406D4B), UINT32_C(0x522BEE48),
    UINT32_C(0x86E18AA3), UINT32_C(0x748A09A0), UINT32_C(0x67DAFA54), UINT32_C(0x95B17957),
    UINT32_C(0xCBA24573), UINT32_C(0x39C9C670), UINT32_C(0x2A993584), UINT32_C(0xD8F2B687),
    UINT32_C(0x0C38D26C), UINT32_C(0xFE53516F), UINT32_C(0xED03A29B), UINT32_C(0x1F682198),
    UINT32_C(0x5125DAD3), UINT32_C(0xA34E59D0), UINT32_C(0xB01EAA24), UINT32_C(0x42752927),
    UINT32_C(0x96BF4DCC), UINT32_C(0x64D4CECF), UINT32_C(0x77843D3B), UINT32_C(0x85EFBE38),
    UINT32_C(0xDBFC821C), UINT32_C(0x2997011F), UINT32_C(0x3AC7F2EB), UINT32_C(0xC8AC71E8),
    UINT32_C(0x1C661503), UINT32_C(0xEE0D9600), UINT32_C(0xFD5D65F4), UINT32_C(0x0F36E6F7),
    UINT32_C(0x61C69362), UINT32_C(0x93AD1061), UINT32_C(0x80FDE395), UINT32_C(0x72966096),
    UINT32_C(0xA65C047D), UINT32_C(0x5437877E), UINT32_C(0x4767748A), UINT32_C(0xB50CF789),
    UINT32_C(0xEB1FCBAD), UINT32_C(0x197448AE), UINT32_C(0x0A24BB5A), UINT32_C(0xF84F3859),
    UINT32_C(0x2C855CB2), UINT32_C(0xDEEEDFB1), UINT32_C(0xCDBE2C45), UINT32_C(0x3FD5AF46),
    UINT32_C(0x7198540D), UINT32_C(0x83F3D70E), UINT32_C(0x90A324FA), UINT32_C(0x62C8A7F9),
    UINT32_C(0xB602C312), UINT32_C(0x44694011), UINT32_C(0x5739B3E5), UINT32_C(0xA55230E6),
    UINT32_C(0xFB410CC2), UINT32_C(0x092A8FC1), UINT32_C(0x1A7A7C35), UINT32_C(0xE811FF36),
    UINT32_C(0x3CDB9BDD), UINT32_C(0xCEB018DE), UINT32_C(0xDDE0EB2A), UINT32_C(0x2F8B6829),
    UINT32_C(0x82F63B78), UINT32_C(0x709DB87B), UINT32_C(0x63CD4B8F), UINT32_C(0x91A6C88C),
    UINT32_C(0x456CAC67), UINT32_C(0xB7072F64), UINT32_C(0xA457DC90), UINT32_C(0x563C5F93),
    UINT32_C(0x082F63B7), UINT32_C(0xFA44E0B4), UINT32_C(0xE9141340), UINT32_C(0x1B7F9043),
    UINT32_C(0xCFB5F4A8), UINT32_C(0x3DDE77AB), UINT32_C(0x2E8E845F), UINT32_C(0xDCE5075C),
    UINT32_C(0x92A8FC17), UINT32_C(0x60C37F14), UINT32_C(0x73938CE0), UINT32_C(0x81F80FE3),
    UINT32_C(0x55326B08), UINT32_C(0xA759E80B), UINT32_C(0xB4091BFF), UINT32_C(0x466298FC),
    UINT32_C(0x1871A4D8), UINT32_C(0xEA1A27DB), UINT32_C(0xF94AD42F), UINT32_C(0x0B21572C),
    UINT32_C(0xDFEB33C7), UINT32_C(0x2D80B0C4), UINT32_C(0x3ED04330), UINT32_C(0xCCBBC033),
    UINT32_C(0xA24BB5A6), UINT32_C(0x502036A5), UINT32_C(0x4370C551), UINT32_C(0xB11B4652),
    UINT32_C(0x65D122B9), UINT32_C(0x97BAA1BA), UINT32_C(0x84EA524E), UINT32_C(0x7681D14D),
    UINT32_C(0x2892ED69), UINT32_C(0xDAF96E6A), UINT32_C(0xC9A99D9E), UINT32_C(0x3BC21E9D),
    UINT32_C(0xEF087A76), UINT32_C(0x1D63F975), UINT32_C(0x0E330A81), UINT32_C(0xFC588982),
    UINT32_C(0xB21572C9), UINT32_C(0x407EF1CA), UINT32_C(0x532E023E), UINT32_C(0xA145813D),
    UINT32_C(0x758FE5D6), UINT32_C(0x87E466D5), UINT32_C(0x94B49521), UINT32_C(0x66DF1622),
    UINT32_C(0x38CC2A06), UINT32_C(0xCAA7A905), UINT32_C(0xD9F75AF1), UINT32_C(0x2B9CD9F2),
    UINT32_C(0xFF56BD19), UINT32_C(0x0D3D3E1A), UINT32_C(0x1E6DCDEE), UINT32_C(0xEC064EED),
    UINT32_C(0xC38D26C4), UINT32_C(0x31E6A5C7), UINT32_C(0x22B65633), UINT32_C(0xD0DDD530),
    UINT32_C(0x0417B1DB), UINT32_C(0xF67C32D8), UINT32_C(0xE52CC12C), UINT32_C(0x1747422F),
    UINT32_C(0x49547E0B), UINT32_C(0xBB3FFD08), UINT32_C(0xA86F0EFC), UINT32_C(0x5A048DFF),
    UINT32_C(0x8ECEE914), UINT32_C(0x7CA56A17), UINT32_C(0x6FF599E3), UINT32_C(0x9D9E1AE0),
    UINT32_C(0xD3D3E1AB), UINT32_C(0x21B862A8), UINT32_C(0x32E8915C), UINT32_C(0xC083125F),
    UINT32_C(0x144976B4), UINT32_C(0xE622F5B7), UINT32_C(0xF5720643), UINT32_C(0x07198540),
    UINT32_C(0x590AB964), UINT32_C(0xAB613A67), UINT32_C(0xB831C993), UINT32_C(0x4A5A4A90),
    UINT32_C(0x9E902E7B), UINT32_C(0x6CFBAD78), UINT32_C(0x7FAB5E8C), UINT32_C(0x8DC0DD8F),
    UINT32_C(0xE330A81A), UINT32_C(0x115B2B19), UINT32_C(0x020BD8ED), UINT32_C(0xF0605BEE),
    UINT32_C(0x24AA3F05), UINT32_C(0xD6C1BC06), UINT32_C(0xC5914FF2), UINT32_C(0x37FACCF1),
    UINT32_C(0x69E9F0D5), UINT32_C(0x9B8273D6), UINT32_C(0x88D28022), UINT32_C(0x7AB90321),
    UINT32_C(0xAE7367CA), UINT32_C(0x5C18E4C9), UINT32_C(0x4F48173D), UINT32_C(0xBD23943E),
    UINT32_C(0xF36E6F75), UINT32_C(0x0105EC76), UINT32_C(0x12551F82), UINT32_C(0xE03E9C81),
    UINT32_C(0x34F4F86A), UINT32_C(0xC69F7B69), UINT32_C(0xD5CF889D), UINT32_C(0x27A40B9E),
    UINT32_C(0x79B737BA), UINT32_C(0x8BDCB4B9), UINT32_C(0x988C474D), UINT32_C(0x6AE7C44E),
    UINT32_C(0xBE2DA0A5), UINT32_C(0x4C4623A6), UINT32_C(0x5F16D052), UINT32_C(0xAD7D5351)
};

static uint32_t align_up_4(uint32_t value)
{
    return (value + UINT32_C(3)) & ~UINT32_C(3);
}

static dual_spi_result_t validate_page_meta(const dual_spi_page_meta_t *meta,
                                            bool reject_inactive_fields)
{
    bool tx_sequence_valid;
    bool rx_result_valid;
    bool is_ack_only;
    bool is_reset;

    if (meta == NULL)
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    if (meta->link_id > (uint8_t)DUAL_SPI_LINK_B)
    {
        return DUAL_SPI_RESULT_BAD_LINK_ID;
    }

    if (meta->rx_credit_pages > 1U)
    {
        return DUAL_SPI_RESULT_BAD_CREDIT;
    }

    if ((meta->page_flags & (uint8_t)~DUAL_SPI_PAGE_FLAG_KNOWN_MASK) != 0U)
    {
        return DUAL_SPI_RESULT_BAD_FLAGS;
    }

    if ((meta->page_flags & DUAL_SPI_PAGE_FLAG_MORE_FRAGMENTS) != 0U)
    {
        return DUAL_SPI_RESULT_BAD_FLAGS;
    }

    tx_sequence_valid =
        (meta->page_flags & DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID) != 0U;
    rx_result_valid =
        (meta->page_flags & DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID) != 0U;
    is_ack_only = (meta->page_flags & DUAL_SPI_PAGE_FLAG_ACK_ONLY) != 0U;
    is_reset = (meta->page_flags & DUAL_SPI_PAGE_FLAG_RESET_EPOCH) != 0U;

    if ((!tx_sequence_valid &&
         (((meta->page_flags & DUAL_SPI_PAGE_FLAG_RETRANSMIT) != 0U) ||
          is_reset ||
          (reject_inactive_fields && (meta->tx_sequence != 0U)))) ||
        (is_ack_only && tx_sequence_valid && !is_reset) ||
        (is_reset &&
         (!is_ack_only || !tx_sequence_valid || (meta->tx_sequence != 0U))))
    {
        return DUAL_SPI_RESULT_BAD_FLAGS;
    }

    if (!rx_result_valid)
    {
        if ((meta->status_code != DUAL_SPI_STATUS_OK) ||
            ((meta->page_flags & DUAL_SPI_PAGE_FLAG_ERROR) != 0U) ||
            (reject_inactive_fields && (meta->rx_result_sequence != 0U)))
        {
            return DUAL_SPI_RESULT_BAD_FLAGS;
        }
    }
    else if ((meta->status_code != DUAL_SPI_STATUS_OK) !=
             ((meta->page_flags & DUAL_SPI_PAGE_FLAG_ERROR) != 0U))
    {
        return DUAL_SPI_RESULT_BAD_FLAGS;
    }

    return DUAL_SPI_RESULT_OK;
}

static dual_spi_result_t validate_tlv_payload(const uint8_t *payload,
                                              uint16_t payload_bytes,
                                              uint16_t message_count)
{
    dual_spi_tlv_iterator_t iterator;
    dual_spi_tlv_view_t view;
    bool has_value;
    dual_spi_result_t result;

    if ((payload_bytes > 0U) && (payload == NULL))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    dual_spi_tlv_iterator_init(&iterator, payload, payload_bytes, message_count);
    do
    {
        result = dual_spi_tlv_iterator_next(&iterator, &view, &has_value);
        if (result != DUAL_SPI_RESULT_OK)
        {
            return result;
        }
    } while (has_value);

    return DUAL_SPI_RESULT_OK;
}

static void set_decode_error_info(dual_spi_page_decode_info_t *decode_info,
                                  uint32_t trusted_tx_sequence,
                                  uint16_t error_status_code)
{
    if (decode_info == NULL)
    {
        return;
    }

    decode_info->error_result_eligible = true;
    decode_info->trusted_tx_sequence = trusted_tx_sequence;
    decode_info->error_status_code = error_status_code;
}

static uint16_t decode_result_status(dual_spi_result_t result)
{
    switch (result)
    {
        case DUAL_SPI_RESULT_BAD_PAYLOAD_LENGTH:
            return DUAL_SPI_STATUS_BAD_PAYLOAD_LENGTH;

        case DUAL_SPI_RESULT_BAD_TLV:
            return DUAL_SPI_STATUS_BAD_TLV;

        case DUAL_SPI_RESULT_BAD_LINK_ID:
            return DUAL_SPI_STATUS_BAD_LINK_ID;

        case DUAL_SPI_RESULT_BAD_CREDIT:
            return DUAL_SPI_STATUS_BAD_CREDIT;

        case DUAL_SPI_RESULT_BAD_FLAGS:
            return DUAL_SPI_STATUS_BAD_FLAGS;

        default:
            return DUAL_SPI_STATUS_OK;
    }
}

uint16_t dual_spi_get_u16_le(const uint8_t *src)
{
    return (uint16_t)((uint16_t)src[0] |
                      ((uint16_t)src[1] << 8U));
}

uint32_t dual_spi_get_u32_le(const uint8_t *src)
{
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8U) |
           ((uint32_t)src[2] << 16U) |
           ((uint32_t)src[3] << 24U);
}

void dual_spi_put_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8U);
}

void dual_spi_put_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8U);
    dst[2] = (uint8_t)(value >> 16U);
    dst[3] = (uint8_t)(value >> 24U);
}

uint32_t dual_spi_crc32c(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    size_t index;

    if ((bytes == NULL) && (length != 0U))
    {
        return 0U;
    }

    for (index = 0U; index < length; ++index)
    {
        crc = g_crc32c_table[(uint8_t)(crc ^ bytes[index])] ^ (crc >> 8U);
    }

    return crc ^ UINT32_C(0xFFFFFFFF);
}

void dual_spi_tlv_builder_init(dual_spi_tlv_builder_t *builder,
                               uint8_t *payload,
                               uint16_t capacity)
{
    if (builder == NULL)
    {
        return;
    }

    builder->payload = payload;
    builder->capacity = capacity;
    builder->used = 0U;
    builder->message_count = 0U;
}

dual_spi_result_t dual_spi_tlv_builder_add(dual_spi_tlv_builder_t *builder,
                                          uint16_t type,
                                          uint16_t flags,
                                          uint16_t tag,
                                          const void *body,
                                          uint16_t body_bytes)
{
    uint32_t raw_bytes;
    uint32_t encoded_bytes;
    uint8_t *destination;

    if ((builder == NULL) || (builder->payload == NULL) ||
        ((body_bytes > 0U) && (body == NULL)))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    if ((type == UINT16_C(0xFFFF)) || (flags != 0U))
    {
        return DUAL_SPI_RESULT_BAD_TLV;
    }

    raw_bytes = (uint32_t)DUAL_SPI_TLV_HEADER_BYTES + body_bytes;
    if (raw_bytes > UINT16_MAX)
    {
        return DUAL_SPI_RESULT_NO_SPACE;
    }

    encoded_bytes = align_up_4(raw_bytes);
    if ((encoded_bytes > UINT16_MAX) ||
        ((uint32_t)builder->used + encoded_bytes > builder->capacity) ||
        (builder->message_count == UINT16_MAX))
    {
        return DUAL_SPI_RESULT_NO_SPACE;
    }

    destination = &builder->payload[builder->used];
    dual_spi_put_u16_le(&destination[0], type);
    dual_spi_put_u16_le(&destination[2], flags);
    dual_spi_put_u16_le(&destination[4], body_bytes);
    dual_spi_put_u16_le(&destination[6], tag);

    if (body_bytes > 0U)
    {
        memcpy(&destination[DUAL_SPI_TLV_HEADER_BYTES], body, body_bytes);
    }

    memset(&destination[DUAL_SPI_TLV_HEADER_BYTES + body_bytes],
           0,
           (size_t)encoded_bytes - DUAL_SPI_TLV_HEADER_BYTES - body_bytes);

    builder->used = (uint16_t)((uint32_t)builder->used + encoded_bytes);
    builder->message_count = (uint16_t)(builder->message_count + 1U);
    return DUAL_SPI_RESULT_OK;
}

void dual_spi_tlv_iterator_init(dual_spi_tlv_iterator_t *iterator,
                                const uint8_t *payload,
                                uint16_t payload_bytes,
                                uint16_t expected_count)
{
    if (iterator == NULL)
    {
        return;
    }

    iterator->payload = payload;
    iterator->payload_bytes = payload_bytes;
    iterator->expected_count = expected_count;
    iterator->offset = 0U;
    iterator->seen_count = 0U;
}

dual_spi_result_t dual_spi_tlv_iterator_next(dual_spi_tlv_iterator_t *iterator,
                                             dual_spi_tlv_view_t *view,
                                             bool *has_value)
{
    const uint8_t *source;
    uint16_t body_bytes;
    uint32_t encoded_bytes;
    uint32_t padding_index;
    uint32_t raw_bytes;

    if ((iterator == NULL) || (view == NULL) || (has_value == NULL))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    *has_value = false;

    if (iterator->seen_count == iterator->expected_count)
    {
        return (iterator->offset == iterator->payload_bytes) ?
                   DUAL_SPI_RESULT_OK :
                   DUAL_SPI_RESULT_BAD_TLV;
    }

    if ((iterator->payload == NULL) ||
        ((uint32_t)iterator->offset + DUAL_SPI_TLV_HEADER_BYTES >
         iterator->payload_bytes))
    {
        return DUAL_SPI_RESULT_BAD_TLV;
    }

    source = &iterator->payload[iterator->offset];
    body_bytes = dual_spi_get_u16_le(&source[4]);
    raw_bytes = (uint32_t)DUAL_SPI_TLV_HEADER_BYTES + body_bytes;
    if (raw_bytes > UINT16_MAX)
    {
        return DUAL_SPI_RESULT_BAD_TLV;
    }

    encoded_bytes = align_up_4(raw_bytes);
    if ((uint32_t)iterator->offset + encoded_bytes > iterator->payload_bytes)
    {
        return DUAL_SPI_RESULT_BAD_TLV;
    }

    for (padding_index = (uint16_t)raw_bytes;
         padding_index < encoded_bytes;
         ++padding_index)
    {
        if (source[padding_index] != 0U)
        {
            return DUAL_SPI_RESULT_BAD_TLV;
        }
    }

    view->type = dual_spi_get_u16_le(&source[0]);
    view->flags = dual_spi_get_u16_le(&source[2]);
    if ((view->type == UINT16_C(0xFFFF)) || (view->flags != 0U))
    {
        return DUAL_SPI_RESULT_BAD_TLV;
    }
    view->length = body_bytes;
    view->tag = dual_spi_get_u16_le(&source[6]);
    view->body = &source[DUAL_SPI_TLV_HEADER_BYTES];

    iterator->offset =
        (uint16_t)((uint32_t)iterator->offset + encoded_bytes);
    iterator->seen_count = (uint16_t)(iterator->seen_count + 1U);
    *has_value = true;
    return DUAL_SPI_RESULT_OK;
}

dual_spi_result_t dual_spi_page_encode(uint8_t *page,
                                       size_t page_bytes,
                                       const dual_spi_page_meta_t *meta,
                                       const uint8_t *payload,
                                       uint16_t payload_bytes,
                                       uint16_t message_count)
{
    dual_spi_result_t result;

    if ((page == NULL) || (meta == NULL))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    if ((page_bytes != DUAL_SPI_PAGE_BYTES) ||
        (payload_bytes > DUAL_SPI_PAYLOAD_BYTES_MAX))
    {
        return DUAL_SPI_RESULT_BAD_PAYLOAD_LENGTH;
    }

    if (((meta->page_flags & DUAL_SPI_PAGE_FLAG_ACK_ONLY) != 0U) &&
        ((payload_bytes != 0U) || (message_count != 0U)))
    {
        return DUAL_SPI_RESULT_BAD_FLAGS;
    }

    result = validate_page_meta(meta, true);
    if (result != DUAL_SPI_RESULT_OK)
    {
        return result;
    }

    result = validate_tlv_payload(payload, payload_bytes, message_count);
    if (result != DUAL_SPI_RESULT_OK)
    {
        return result;
    }

    memset(page, 0, DUAL_SPI_PAGE_BYTES);
    dual_spi_put_u32_le(&page[DUAL_SPI_OFFSET_MAGIC], DUAL_SPI_MAGIC);
    page[DUAL_SPI_OFFSET_VERSION] = DUAL_SPI_VERSION_MAJOR;
    page[DUAL_SPI_OFFSET_HEADER_BYTES] = DUAL_SPI_HEADER_BYTES;
    page[DUAL_SPI_OFFSET_LINK_ID] = meta->link_id;
    page[DUAL_SPI_OFFSET_PAGE_FLAGS] = meta->page_flags;
    dual_spi_put_u32_le(&page[DUAL_SPI_OFFSET_TX_SEQUENCE], meta->tx_sequence);
    dual_spi_put_u32_le(&page[DUAL_SPI_OFFSET_RX_RESULT_SEQUENCE],
                        meta->rx_result_sequence);
    dual_spi_put_u16_le(&page[DUAL_SPI_OFFSET_PAYLOAD_BYTES], payload_bytes);
    dual_spi_put_u16_le(&page[DUAL_SPI_OFFSET_MESSAGE_COUNT], message_count);
    dual_spi_put_u32_le(&page[DUAL_SPI_OFFSET_SENDER_TIME_US], meta->sender_time_us);
    dual_spi_put_u16_le(&page[DUAL_SPI_OFFSET_STATUS_CODE], meta->status_code);
    dual_spi_put_u16_le(&page[DUAL_SPI_OFFSET_RX_CREDIT_PAGES], meta->rx_credit_pages);
    dual_spi_put_u32_le(&page[DUAL_SPI_OFFSET_HEADER_CRC],
                        dual_spi_crc32c(page, DUAL_SPI_OFFSET_HEADER_CRC));

    if (payload_bytes > 0U)
    {
        memcpy(&page[DUAL_SPI_OFFSET_PAYLOAD], payload, payload_bytes);
    }

    dual_spi_put_u32_le(&page[DUAL_SPI_PAGE_CRC_OFFSET],
                        dual_spi_crc32c(page, DUAL_SPI_PAGE_CRC_OFFSET));
    return DUAL_SPI_RESULT_OK;
}

dual_spi_result_t dual_spi_page_decode(const uint8_t *page,
                                       size_t page_bytes,
                                       dual_spi_page_view_t *view)
{
    return dual_spi_page_decode_with_info(page,
                                          page_bytes,
                                          view,
                                          NULL);
}

dual_spi_result_t dual_spi_page_decode_with_info(
    const uint8_t *page,
    size_t page_bytes,
    dual_spi_page_view_t *view,
    dual_spi_page_decode_info_t *decode_info)
{
    dual_spi_page_meta_t meta;
    uint16_t payload_bytes;
    uint16_t message_count;
    uint32_t expected_crc;
    uint32_t trusted_tx_sequence;
    uint16_t error_status;
    dual_spi_result_t result;

    if (decode_info != NULL)
    {
        decode_info->error_result_eligible = false;
        decode_info->trusted_tx_sequence = 0U;
        decode_info->error_status_code = DUAL_SPI_STATUS_OK;
    }

    if ((page == NULL) || (view == NULL))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    if (page_bytes != DUAL_SPI_PAGE_BYTES)
    {
        return DUAL_SPI_RESULT_BAD_PAYLOAD_LENGTH;
    }

    if (dual_spi_get_u32_le(&page[DUAL_SPI_OFFSET_MAGIC]) != DUAL_SPI_MAGIC)
    {
        return DUAL_SPI_RESULT_BAD_MAGIC;
    }

    if (page[DUAL_SPI_OFFSET_VERSION] != DUAL_SPI_VERSION_MAJOR)
    {
        return DUAL_SPI_RESULT_UNSUPPORTED_VERSION;
    }

    if (page[DUAL_SPI_OFFSET_HEADER_BYTES] != DUAL_SPI_HEADER_BYTES)
    {
        return DUAL_SPI_RESULT_BAD_HEADER_LENGTH;
    }

    expected_crc = dual_spi_crc32c(page, DUAL_SPI_OFFSET_HEADER_CRC);
    if (dual_spi_get_u32_le(&page[DUAL_SPI_OFFSET_HEADER_CRC]) != expected_crc)
    {
        return DUAL_SPI_RESULT_BAD_HEADER_CRC;
    }

    expected_crc = dual_spi_crc32c(page, DUAL_SPI_PAGE_CRC_OFFSET);
    if (dual_spi_get_u32_le(&page[DUAL_SPI_PAGE_CRC_OFFSET]) != expected_crc)
    {
        return DUAL_SPI_RESULT_BAD_PAGE_CRC;
    }

    trusted_tx_sequence =
        dual_spi_get_u32_le(&page[DUAL_SPI_OFFSET_TX_SEQUENCE]);
    payload_bytes = dual_spi_get_u16_le(&page[DUAL_SPI_OFFSET_PAYLOAD_BYTES]);
    message_count = dual_spi_get_u16_le(&page[DUAL_SPI_OFFSET_MESSAGE_COUNT]);
    if (payload_bytes > DUAL_SPI_PAYLOAD_BYTES_MAX)
    {
        set_decode_error_info(decode_info,
                              trusted_tx_sequence,
                              DUAL_SPI_STATUS_BAD_PAYLOAD_LENGTH);
        return DUAL_SPI_RESULT_BAD_PAYLOAD_LENGTH;
    }

    result = validate_tlv_payload(&page[DUAL_SPI_OFFSET_PAYLOAD],
                                  payload_bytes,
                                  message_count);
    if (result != DUAL_SPI_RESULT_OK)
    {
        set_decode_error_info(decode_info,
                              trusted_tx_sequence,
                              DUAL_SPI_STATUS_BAD_TLV);
        return DUAL_SPI_RESULT_BAD_TLV;
    }

    meta.link_id = page[DUAL_SPI_OFFSET_LINK_ID];
    meta.page_flags = page[DUAL_SPI_OFFSET_PAGE_FLAGS];
    meta.tx_sequence = trusted_tx_sequence;
    meta.rx_result_sequence =
        dual_spi_get_u32_le(&page[DUAL_SPI_OFFSET_RX_RESULT_SEQUENCE]);
    meta.sender_time_us =
        dual_spi_get_u32_le(&page[DUAL_SPI_OFFSET_SENDER_TIME_US]);
    meta.status_code = dual_spi_get_u16_le(&page[DUAL_SPI_OFFSET_STATUS_CODE]);
    meta.rx_credit_pages =
        dual_spi_get_u16_le(&page[DUAL_SPI_OFFSET_RX_CREDIT_PAGES]);

    result = validate_page_meta(&meta, false);
    if (result != DUAL_SPI_RESULT_OK)
    {
        error_status = decode_result_status(result);
        if (error_status != DUAL_SPI_STATUS_OK)
        {
            set_decode_error_info(decode_info,
                                  trusted_tx_sequence,
                                  error_status);
        }
        return result;
    }

    if (((meta.page_flags & DUAL_SPI_PAGE_FLAG_ACK_ONLY) != 0U) &&
        ((payload_bytes != 0U) || (message_count != 0U)))
    {
        set_decode_error_info(decode_info,
                              trusted_tx_sequence,
                              DUAL_SPI_STATUS_BAD_FLAGS);
        return DUAL_SPI_RESULT_BAD_FLAGS;
    }

    for (expected_crc = (uint32_t)DUAL_SPI_OFFSET_PAYLOAD + payload_bytes;
         expected_crc < DUAL_SPI_PAGE_CRC_OFFSET;
         ++expected_crc)
    {
        if (page[expected_crc] != 0U)
        {
            set_decode_error_info(decode_info,
                                  trusted_tx_sequence,
                                  DUAL_SPI_STATUS_BAD_TLV);
            return DUAL_SPI_RESULT_BAD_TLV;
        }
    }

    view->meta = meta;
    view->payload_bytes = payload_bytes;
    view->message_count = message_count;
    view->payload = &page[DUAL_SPI_OFFSET_PAYLOAD];
    return DUAL_SPI_RESULT_OK;
}

void dual_spi_rx_sequence_init(dual_spi_rx_sequence_state_t *state)
{
    if (state == NULL)
    {
        return;
    }

    state->synchronized = false;
    state->expected_sequence = 0U;
}

dual_spi_result_t dual_spi_rx_sequence_accept(dual_spi_rx_sequence_state_t *state,
                                              const dual_spi_page_view_t *page,
                                              dual_spi_rx_disposition_t *disposition)
{
    dual_spi_result_t result;
    bool tx_sequence_valid;
    bool is_reset;

    if ((state == NULL) || (page == NULL) || (disposition == NULL))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    result = validate_page_meta(&page->meta, false);
    if (result != DUAL_SPI_RESULT_OK)
    {
        return result;
    }

    tx_sequence_valid =
        (page->meta.page_flags & DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID) != 0U;
    is_reset =
        (page->meta.page_flags & DUAL_SPI_PAGE_FLAG_RESET_EPOCH) != 0U;

    if (!tx_sequence_valid)
    {
        *disposition = DUAL_SPI_RX_UNSEQUENCED;
        return DUAL_SPI_RESULT_OK;
    }

    if (is_reset)
    {
        if (state->synchronized && (state->expected_sequence == 1U))
        {
            *disposition = DUAL_SPI_RX_RESET_DUPLICATE;
        }
        else
        {
            state->synchronized = true;
            state->expected_sequence = 1U;
            *disposition = DUAL_SPI_RX_RESET_ACCEPTED;
        }
        return DUAL_SPI_RESULT_OK;
    }

    if (!state->synchronized)
    {
        *disposition = DUAL_SPI_RX_SEQUENCE_GAP;
        return DUAL_SPI_RESULT_OK;
    }

    if (page->meta.tx_sequence == state->expected_sequence)
    {
        state->expected_sequence = state->expected_sequence + 1U;
        *disposition = DUAL_SPI_RX_ACCEPT_NEW;
    }
    else if (page->meta.tx_sequence == (state->expected_sequence - 1U))
    {
        *disposition = DUAL_SPI_RX_DUPLICATE;
    }
    else
    {
        *disposition = DUAL_SPI_RX_SEQUENCE_GAP;
    }

    return DUAL_SPI_RESULT_OK;
}

void dual_spi_tx_state_init(dual_spi_tx_state_t *state)
{
    if (state == NULL)
    {
        return;
    }

    state->synchronized = false;
    state->outstanding = false;
    state->outstanding_is_reset = false;
    state->peer_credit_pages = 0U;
    state->next_sequence = 0U;
    state->outstanding_sequence = 0U;
}

dual_spi_result_t dual_spi_tx_begin_reset(dual_spi_tx_state_t *state,
                                          uint32_t *sequence)
{
    if ((state == NULL) || (sequence == NULL))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    state->synchronized = false;
    state->outstanding = true;
    state->outstanding_is_reset = true;
    state->peer_credit_pages = 0U;
    state->next_sequence = 1U;
    state->outstanding_sequence = 0U;
    *sequence = 0U;
    return DUAL_SPI_RESULT_OK;
}

dual_spi_result_t dual_spi_tx_begin_data(dual_spi_tx_state_t *state,
                                         uint32_t *sequence)
{
    if ((state == NULL) || (sequence == NULL))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    if (!state->synchronized)
    {
        return DUAL_SPI_RESULT_NOT_SYNCHRONIZED;
    }

    if (state->outstanding)
    {
        return DUAL_SPI_RESULT_TX_OUTSTANDING;
    }

    if (state->peer_credit_pages == 0U)
    {
        return DUAL_SPI_RESULT_NO_RX_CREDIT;
    }

    state->outstanding = true;
    state->outstanding_is_reset = false;
    state->outstanding_sequence = state->next_sequence;
    state->next_sequence = state->next_sequence + 1U;
    state->peer_credit_pages = 0U;
    *sequence = state->outstanding_sequence;
    return DUAL_SPI_RESULT_OK;
}

dual_spi_result_t dual_spi_tx_get_retransmit(const dual_spi_tx_state_t *state,
                                             uint32_t *sequence)
{
    if ((state == NULL) || (sequence == NULL))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    if (!state->outstanding)
    {
        return DUAL_SPI_RESULT_NO_TX_OUTSTANDING;
    }

    *sequence = state->outstanding_sequence;
    return DUAL_SPI_RESULT_OK;
}

dual_spi_result_t dual_spi_tx_observe_peer(dual_spi_tx_state_t *state,
                                          const dual_spi_page_view_t *page,
                                          dual_spi_tx_observation_t *observation)
{
    dual_spi_result_t result;

    if ((state == NULL) || (page == NULL) || (observation == NULL))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    result = validate_page_meta(&page->meta, false);
    if (result != DUAL_SPI_RESULT_OK)
    {
        return result;
    }

    state->peer_credit_pages = (uint8_t)page->meta.rx_credit_pages;

    if ((page->meta.page_flags & DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID) == 0U)
    {
        *observation = DUAL_SPI_TX_OBSERVE_NO_RESULT;
        return DUAL_SPI_RESULT_OK;
    }

    if (!state->outstanding ||
        (page->meta.rx_result_sequence != state->outstanding_sequence))
    {
        *observation = DUAL_SPI_TX_OBSERVE_STALE_RESULT;
        return DUAL_SPI_RESULT_OK;
    }

    if (page->meta.status_code == DUAL_SPI_STATUS_OK)
    {
        if (state->outstanding_is_reset)
        {
            state->synchronized = true;
        }

        state->outstanding = false;
        state->outstanding_is_reset = false;
        *observation = DUAL_SPI_TX_OBSERVE_ACKED;
    }
    else if (page->meta.status_code == DUAL_SPI_STATUS_NO_RX_CREDIT)
    {
        *observation = DUAL_SPI_TX_OBSERVE_WAIT_CREDIT;
    }
    else if (page->meta.status_code == DUAL_SPI_STATUS_SEQUENCE_GAP)
    {
        state->synchronized = false;
        *observation = DUAL_SPI_TX_OBSERVE_RESET_REQUIRED;
    }
    else
    {
        *observation = DUAL_SPI_TX_OBSERVE_NACK;
    }

    return DUAL_SPI_RESULT_OK;
}




