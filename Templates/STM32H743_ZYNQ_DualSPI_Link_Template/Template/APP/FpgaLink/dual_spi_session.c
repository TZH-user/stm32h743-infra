#include "dual_spi_session.h"

#include <string.h>

static void set_pending_result(dual_spi_session_t *session,
                               uint32_t sequence,
                               uint16_t status)
{
    session->pending_result_valid = true;
    session->pending_result_sequence = sequence;
    session->pending_result_status = status;
}

static bool nack_requires_epoch_reset(uint16_t status)
{
    return (status == DUAL_SPI_STATUS_BAD_PAYLOAD_LENGTH) ||
           (status == DUAL_SPI_STATUS_BAD_TLV) ||
           (status == DUAL_SPI_STATUS_BAD_FLAGS) ||
           (status == DUAL_SPI_STATUS_BAD_LINK_ID) ||
           (status == DUAL_SPI_STATUS_BAD_CREDIT);
}

static dual_spi_result_t begin_reset_if_required(dual_spi_session_t *session)
{
    uint32_t sequence;
    dual_spi_result_t result;
    bool replacing_data_page;

    if (session->tx_state.synchronized &&
        (!session->tx_state.outstanding ||
         !session->tx_state.outstanding_is_reset))
    {
        return DUAL_SPI_RESULT_OK;
    }

    if (session->tx_state.outstanding &&
        session->tx_state.outstanding_is_reset)
    {
        return DUAL_SPI_RESULT_OK;
    }

    replacing_data_page = session->tx_state.outstanding &&
                          !session->tx_state.outstanding_is_reset;
    result = dual_spi_tx_begin_reset(&session->tx_state, &sequence);
    if (result != DUAL_SPI_RESULT_OK)
    {
        return result;
    }

    session->replay_after_reset = replacing_data_page;
    if (!replacing_data_page)
    {
        session->outstanding_payload_bytes = 0U;
        session->outstanding_message_count = 0U;
    }
    session->outstanding_transmitted = false;
    return DUAL_SPI_RESULT_OK;
}

static dual_spi_result_t begin_pending_data_if_possible(
    dual_spi_session_t *session)
{
    uint32_t sequence;
    dual_spi_result_t result;

    if (session->tx_state.outstanding || !session->tx_state.synchronized ||
        (session->tx_state.peer_credit_pages == 0U))
    {
        return DUAL_SPI_RESULT_OK;
    }

    if (!session->replay_after_reset &&
        (session->pending_message_count == 0U))
    {
        return DUAL_SPI_RESULT_OK;
    }

    result = dual_spi_tx_begin_data(&session->tx_state, &sequence);
    if (result != DUAL_SPI_RESULT_OK)
    {
        return result;
    }

    if (session->replay_after_reset)
    {
        session->replay_after_reset = false;
    }
    else
    {
        memcpy(session->outstanding_payload,
               session->pending_payload,
               session->pending_payload_bytes);
        session->outstanding_payload_bytes = session->pending_payload_bytes;
        session->outstanding_message_count = session->pending_message_count;
        session->pending_payload_bytes = 0U;
        session->pending_message_count = 0U;
    }
    session->outstanding_transmitted = false;
    return DUAL_SPI_RESULT_OK;
}

static bool queue_new_rx_page(dual_spi_session_t *session,
                              const dual_spi_page_view_t *view)
{
    if (view->message_count == 0U)
    {
        return true;
    }
    if (session->rx_page_valid)
    {
        return false;
    }

    session->rx_page_meta = view->meta;
    memcpy(session->rx_page_payload,
           view->payload,
           view->payload_bytes);
    session->rx_page_payload_bytes = view->payload_bytes;
    session->rx_page_message_count = view->message_count;
    session->rx_page_valid = true;
    session->stats.dispatched_messages += view->message_count;
    return true;
}

void dual_spi_session_init(dual_spi_session_t *session,
                           dual_spi_link_id_t link_id)
{
    if (session == NULL)
    {
        return;
    }

    memset(session, 0, sizeof(*session));
    session->link_id = link_id;
    dual_spi_rx_sequence_init(&session->rx_state);
    dual_spi_tx_state_init(&session->tx_state);
}

dual_spi_result_t dual_spi_session_queue_tlv(dual_spi_session_t *session,
                                             uint16_t type,
                                             uint16_t tag,
                                             const void *body,
                                             uint16_t body_bytes)
{
    dual_spi_tlv_builder_t builder;
    dual_spi_result_t result;

    if (session == NULL)
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    builder.payload = session->pending_payload;
    builder.capacity = DUAL_SPI_PAYLOAD_BYTES_MAX;
    builder.used = session->pending_payload_bytes;
    builder.message_count = session->pending_message_count;
    result = dual_spi_tlv_builder_add(&builder,
                                      type,
                                      0U,
                                      tag,
                                      body,
                                      body_bytes);
    if (result == DUAL_SPI_RESULT_OK)
    {
        session->pending_payload_bytes = builder.used;
        session->pending_message_count = builder.message_count;
    }
    return result;
}

dual_spi_result_t dual_spi_session_build_tx_page(
    dual_spi_session_t *session,
    uint8_t page[DUAL_SPI_PAGE_BYTES],
    uint32_t sender_time_us,
    uint16_t rx_credit_pages)
{
    dual_spi_page_meta_t meta;
    const uint8_t *payload = NULL;
    uint16_t payload_bytes = 0U;
    uint16_t message_count = 0U;
    dual_spi_result_t result;

    if ((session == NULL) || (page == NULL))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }
    if ((session->link_id != DUAL_SPI_LINK_A) &&
        (session->link_id != DUAL_SPI_LINK_B))
    {
        return DUAL_SPI_RESULT_BAD_LINK_ID;
    }
    if (rx_credit_pages > 1U)
    {
        return DUAL_SPI_RESULT_BAD_CREDIT;
    }

    result = begin_reset_if_required(session);
    if (result != DUAL_SPI_RESULT_OK)
    {
        return result;
    }
    result = begin_pending_data_if_possible(session);
    if (result != DUAL_SPI_RESULT_OK)
    {
        return result;
    }

    memset(&meta, 0, sizeof(meta));
    meta.link_id = (uint8_t)session->link_id;
    meta.sender_time_us = sender_time_us;
    meta.rx_credit_pages = rx_credit_pages;

    if (session->tx_state.outstanding)
    {
        meta.page_flags |= DUAL_SPI_PAGE_FLAG_TX_SEQUENCE_VALID;
        meta.tx_sequence = session->tx_state.outstanding_sequence;
        if (session->tx_state.outstanding_is_reset)
        {
            meta.page_flags |= DUAL_SPI_PAGE_FLAG_ACK_ONLY |
                               DUAL_SPI_PAGE_FLAG_RESET_EPOCH;
        }
        else
        {
            payload = session->outstanding_payload;
            payload_bytes = session->outstanding_payload_bytes;
            message_count = session->outstanding_message_count;
        }

        if (session->outstanding_transmitted)
        {
            meta.page_flags |= DUAL_SPI_PAGE_FLAG_RETRANSMIT;
            ++session->stats.retransmit_pages_built;
        }
    }
    else
    {
        meta.page_flags |= DUAL_SPI_PAGE_FLAG_ACK_ONLY;
    }

    if (session->pending_result_valid)
    {
        meta.page_flags |= DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID;
        meta.rx_result_sequence = session->pending_result_sequence;
        meta.status_code = session->pending_result_status;
        if (meta.status_code != DUAL_SPI_STATUS_OK)
        {
            meta.page_flags |= DUAL_SPI_PAGE_FLAG_ERROR;
        }
    }

    return dual_spi_page_encode(page,
                                DUAL_SPI_PAGE_BYTES,
                                &meta,
                                payload,
                                payload_bytes,
                                message_count);
}

void dual_spi_session_mark_tx_attempted(dual_spi_session_t *session)
{
    if ((session != NULL) && session->tx_state.outstanding)
    {
        session->outstanding_transmitted = true;
    }
}

bool dual_spi_session_peek_rx_page(const dual_spi_session_t *session,
                                   dual_spi_page_view_t *view)
{
    if ((session == NULL) || (view == NULL) || !session->rx_page_valid)
    {
        return false;
    }

    view->meta = session->rx_page_meta;
    view->payload_bytes = session->rx_page_payload_bytes;
    view->message_count = session->rx_page_message_count;
    view->payload = session->rx_page_payload;
    return true;
}

void dual_spi_session_release_rx_page(dual_spi_session_t *session)
{
    if (session == NULL)
    {
        return;
    }

    session->rx_page_valid = false;
    session->rx_page_payload_bytes = 0U;
    session->rx_page_message_count = 0U;
}

uint16_t dual_spi_session_rx_credit_pages(
    const dual_spi_session_t *session)
{
    if ((session == NULL) || session->rx_page_valid)
    {
        return 0U;
    }
    return 1U;
}

dual_spi_result_t dual_spi_session_receive_page(
    dual_spi_session_t *session,
    const uint8_t *page,
    size_t page_bytes,
    dual_spi_rx_disposition_t *disposition,
    dual_spi_tx_observation_t *observation)
{
    dual_spi_page_view_t view;
    dual_spi_page_decode_info_t decode_info;
    dual_spi_rx_sequence_state_t candidate_rx_state;
    dual_spi_result_t result;
    bool result_targets_reset;

    if ((session == NULL) || (page == NULL) ||
        (disposition == NULL) || (observation == NULL))
    {
        return DUAL_SPI_RESULT_INVALID_ARGUMENT;
    }

    result = dual_spi_page_decode_with_info(page,
                                            page_bytes,
                                            &view,
                                            &decode_info);
    if (result != DUAL_SPI_RESULT_OK)
    {
        ++session->stats.protocol_errors;
        if (decode_info.error_result_eligible)
        {
            set_pending_result(session,
                               decode_info.trusted_tx_sequence,
                               decode_info.error_status_code);
        }
        return result;
    }
    if (view.meta.link_id != (uint8_t)session->link_id)
    {
        ++session->stats.protocol_errors;
        set_pending_result(session,
                           view.meta.tx_sequence,
                           DUAL_SPI_STATUS_BAD_LINK_ID);
        return DUAL_SPI_RESULT_BAD_LINK_ID;
    }
    ++session->stats.valid_pages;

    result_targets_reset = session->tx_state.outstanding &&
                           session->tx_state.outstanding_is_reset &&
                           ((view.meta.page_flags &
                             DUAL_SPI_PAGE_FLAG_RX_RESULT_VALID) != 0U) &&
                           (view.meta.rx_result_sequence ==
                            session->tx_state.outstanding_sequence);
    result = dual_spi_tx_observe_peer(&session->tx_state,
                                      &view,
                                      observation);
    if (result != DUAL_SPI_RESULT_OK)
    {
        ++session->stats.protocol_errors;
        return result;
    }
    if (*observation == DUAL_SPI_TX_OBSERVE_ACKED)
    {
        if (!(result_targets_reset && session->replay_after_reset))
        {
            session->outstanding_payload_bytes = 0U;
            session->outstanding_message_count = 0U;
            session->replay_after_reset = false;
        }
        session->outstanding_transmitted = false;
        ++session->stats.acked_tx_pages;
    }
    else if ((*observation == DUAL_SPI_TX_OBSERVE_NACK) &&
             nack_requires_epoch_reset(view.meta.status_code))
    {
        session->tx_state.synchronized = false;
    }

    candidate_rx_state = session->rx_state;
    result = dual_spi_rx_sequence_accept(&candidate_rx_state,
                                         &view,
                                         disposition);
    if (result != DUAL_SPI_RESULT_OK)
    {
        ++session->stats.protocol_errors;
        return result;
    }

    switch (*disposition)
    {
        case DUAL_SPI_RX_RESET_ACCEPTED:
            session->rx_state = candidate_rx_state;
            ++session->stats.accepted_pages;
            set_pending_result(session, 0U, DUAL_SPI_STATUS_OK);
            break;

        case DUAL_SPI_RX_RESET_DUPLICATE:
            ++session->stats.duplicate_pages;
            set_pending_result(session, 0U, DUAL_SPI_STATUS_OK);
            break;

        case DUAL_SPI_RX_ACCEPT_NEW:
            if (!queue_new_rx_page(session, &view))
            {
                *disposition = DUAL_SPI_RX_BACKPRESSURE;
                ++session->stats.backpressured_pages;
                return DUAL_SPI_RESULT_NO_RX_CREDIT;
            }
            session->rx_state = candidate_rx_state;
            ++session->stats.accepted_pages;
            set_pending_result(session,
                               view.meta.tx_sequence,
                               DUAL_SPI_STATUS_OK);
            break;

        case DUAL_SPI_RX_DUPLICATE:
            ++session->stats.duplicate_pages;
            set_pending_result(session,
                               view.meta.tx_sequence,
                               DUAL_SPI_STATUS_OK);
            break;

        case DUAL_SPI_RX_SEQUENCE_GAP:
            ++session->stats.sequence_gaps;
            set_pending_result(session,
                               view.meta.tx_sequence,
                               DUAL_SPI_STATUS_SEQUENCE_GAP);
            break;

        case DUAL_SPI_RX_BACKPRESSURE:
        case DUAL_SPI_RX_UNSEQUENCED:
        default:
            break;
    }

    return DUAL_SPI_RESULT_OK;
}




