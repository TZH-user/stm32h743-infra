#include "dual_spi_transport.h"

#include <string.h>

static bool is_aligned(const void *buffer)
{
    return (((uintptr_t)buffer &
             (uintptr_t)(DUAL_SPI_TRANSPORT_DMA_ALIGNMENT - 1U)) == 0U);
}

static bool buffers_overlap(const uint8_t *first,
                            const uint8_t *second,
                            size_t bytes)
{
    uintptr_t first_start = (uintptr_t)first;
    uintptr_t second_start = (uintptr_t)second;

    if ((first_start > (UINTPTR_MAX - bytes)) ||
        (second_start > (UINTPTR_MAX - bytes)))
    {
        return true;
    }

    return (first_start < (second_start + bytes)) &&
           (second_start < (first_start + bytes));
}

static bool is_initialized(const dual_spi_transport_link_t *link)
{
    return (link != NULL) && link->initialized;
}

static bool is_transfer_active(const dual_spi_transport_link_t *link)
{
    return (link->state == DUAL_SPI_TRANSPORT_STATE_STARTING) ||
           (link->state == DUAL_SPI_TRANSPORT_STATE_ARMED) ||
           (link->state == DUAL_SPI_TRANSPORT_STATE_IN_FLIGHT);
}

static uint32_t enter_fault(dual_spi_transport_link_t *link,
                            dual_spi_transport_fault_t fault,
                            uint32_t detail)
{
    link->master_cs_required = false;
    link->slave_ready_required = false;
    link->last_fault = fault;
    link->last_fault_detail = detail;
    link->state = DUAL_SPI_TRANSPORT_STATE_RECOVERY_REQUIRED;
    return DUAL_SPI_TRANSPORT_EVENT_FAULT;
}

dual_spi_transport_result_t dual_spi_transport_init(
    dual_spi_transport_link_t *link,
    dual_spi_transport_role_t role,
    uint8_t *tx_page,
    uint8_t *rx_page,
    size_t page_bytes)
{
    if (link == NULL)
    {
        return DUAL_SPI_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }

    memset(link, 0, sizeof(*link));
    link->state = DUAL_SPI_TRANSPORT_STATE_STOPPED;

    if ((role != DUAL_SPI_TRANSPORT_ROLE_MASTER_A) &&
        (role != DUAL_SPI_TRANSPORT_ROLE_SLAVE_B))
    {
        return DUAL_SPI_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }

    if ((tx_page == NULL) || (rx_page == NULL) ||
        (page_bytes != DUAL_SPI_PAGE_BYTES) ||
        !is_aligned(tx_page) || !is_aligned(rx_page) ||
        buffers_overlap(tx_page, rx_page, page_bytes))
    {
        return DUAL_SPI_TRANSPORT_RESULT_BAD_BUFFER;
    }

    link->tx_page_storage = tx_page;
    link->rx_page_storage = rx_page;
    link->page_bytes = page_bytes;
    link->role = role;
    link->last_fault = DUAL_SPI_TRANSPORT_FAULT_NONE;
    link->initialized = true;
    link->state = DUAL_SPI_TRANSPORT_STATE_IDLE;
    return DUAL_SPI_TRANSPORT_RESULT_OK;
}

dual_spi_transport_state_t dual_spi_transport_state(
    const dual_spi_transport_link_t *link)
{
    if (!is_initialized(link))
    {
        return DUAL_SPI_TRANSPORT_STATE_STOPPED;
    }

    return link->state;
}

uint8_t *dual_spi_transport_tx_page(dual_spi_transport_link_t *link)
{
    if (!is_initialized(link) ||
        (link->state != DUAL_SPI_TRANSPORT_STATE_IDLE))
    {
        return NULL;
    }

    return link->tx_page_storage;
}

const uint8_t *dual_spi_transport_rx_page(
    const dual_spi_transport_link_t *link)
{
    if (!is_initialized(link) ||
        (link->state != DUAL_SPI_TRANSPORT_STATE_RX_READY))
    {
        return NULL;
    }

    return link->rx_page_storage;
}

const dual_spi_transport_stats_t *dual_spi_transport_stats(
    const dual_spi_transport_link_t *link)
{
    if (!is_initialized(link))
    {
        return NULL;
    }

    return &link->stats;
}

dual_spi_transport_fault_t dual_spi_transport_last_fault(
    const dual_spi_transport_link_t *link,
    uint32_t *detail)
{
    if (!is_initialized(link))
    {
        if (detail != NULL)
        {
            *detail = 0U;
        }
        return DUAL_SPI_TRANSPORT_FAULT_NONE;
    }

    if (detail != NULL)
    {
        *detail = link->last_fault_detail;
    }
    return link->last_fault;
}

bool dual_spi_transport_master_cs_required(
    const dual_spi_transport_link_t *link)
{
    return is_initialized(link) && link->master_cs_required;
}

bool dual_spi_transport_slave_ready_required(
    const dual_spi_transport_link_t *link)
{
    return is_initialized(link) && link->slave_ready_required;
}

uint32_t dual_spi_transport_on_peer_ready_isr(
    dual_spi_transport_link_t *link)
{
    if (!is_initialized(link) ||
        (link->role != DUAL_SPI_TRANSPORT_ROLE_MASTER_A))
    {
        return DUAL_SPI_TRANSPORT_EVENT_NONE;
    }

    ++link->stats.ready_irq_count;
    return DUAL_SPI_TRANSPORT_EVENT_PEER_READY;
}

dual_spi_transport_result_t dual_spi_transport_master_begin(
    dual_spi_transport_link_t *link,
    bool peer_ready_level)
{
    if (!is_initialized(link))
    {
        return DUAL_SPI_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }
    if (link->role != DUAL_SPI_TRANSPORT_ROLE_MASTER_A)
    {
        return DUAL_SPI_TRANSPORT_RESULT_WRONG_ROLE;
    }
    if (link->state != DUAL_SPI_TRANSPORT_STATE_IDLE)
    {
        return DUAL_SPI_TRANSPORT_RESULT_WRONG_STATE;
    }
    if (!peer_ready_level)
    {
        return DUAL_SPI_TRANSPORT_RESULT_PEER_NOT_READY;
    }

    link->master_cs_required = true;
    link->state = DUAL_SPI_TRANSPORT_STATE_STARTING;
    return DUAL_SPI_TRANSPORT_RESULT_OK;
}

dual_spi_transport_result_t dual_spi_transport_slave_arm_begin(
    dual_spi_transport_link_t *link)
{
    if (!is_initialized(link))
    {
        return DUAL_SPI_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }
    if (link->role != DUAL_SPI_TRANSPORT_ROLE_SLAVE_B)
    {
        return DUAL_SPI_TRANSPORT_RESULT_WRONG_ROLE;
    }
    if (link->state != DUAL_SPI_TRANSPORT_STATE_IDLE)
    {
        return DUAL_SPI_TRANSPORT_RESULT_WRONG_STATE;
    }

    link->state = DUAL_SPI_TRANSPORT_STATE_STARTING;
    return DUAL_SPI_TRANSPORT_RESULT_OK;
}

dual_spi_transport_result_t dual_spi_transport_dma_started(
    dual_spi_transport_link_t *link)
{
    if (!is_initialized(link))
    {
        return DUAL_SPI_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }
    if (link->state != DUAL_SPI_TRANSPORT_STATE_STARTING)
    {
        return DUAL_SPI_TRANSPORT_RESULT_WRONG_STATE;
    }

    ++link->stats.dma_start_count;
    if (link->role == DUAL_SPI_TRANSPORT_ROLE_MASTER_A)
    {
        link->state = DUAL_SPI_TRANSPORT_STATE_IN_FLIGHT;
    }
    else
    {
        link->slave_ready_required = true;
        link->state = DUAL_SPI_TRANSPORT_STATE_ARMED;
    }
    return DUAL_SPI_TRANSPORT_RESULT_OK;
}

uint32_t dual_spi_transport_on_slave_selected_isr(
    dual_spi_transport_link_t *link)
{
    if (!is_initialized(link) ||
        (link->role != DUAL_SPI_TRANSPORT_ROLE_SLAVE_B))
    {
        return DUAL_SPI_TRANSPORT_EVENT_NONE;
    }
    if (link->state != DUAL_SPI_TRANSPORT_STATE_ARMED)
    {
        ++link->stats.stale_callback_count;
        return DUAL_SPI_TRANSPORT_EVENT_NONE;
    }

    link->slave_ready_required = false;
    link->state = DUAL_SPI_TRANSPORT_STATE_IN_FLIGHT;
    return DUAL_SPI_TRANSPORT_EVENT_SLAVE_STARTED;
}

uint32_t dual_spi_transport_on_dma_complete_isr(
    dual_spi_transport_link_t *link)
{
    bool valid_state;

    if (!is_initialized(link))
    {
        return DUAL_SPI_TRANSPORT_EVENT_NONE;
    }

    valid_state = (link->state == DUAL_SPI_TRANSPORT_STATE_IN_FLIGHT) ||
                  ((link->role == DUAL_SPI_TRANSPORT_ROLE_SLAVE_B) &&
                   (link->state == DUAL_SPI_TRANSPORT_STATE_ARMED));
    if (!valid_state)
    {
        ++link->stats.stale_callback_count;
        return DUAL_SPI_TRANSPORT_EVENT_NONE;
    }

    link->master_cs_required = false;
    link->slave_ready_required = false;
    link->state = DUAL_SPI_TRANSPORT_STATE_RX_READY;
    ++link->stats.completed_count;
    return DUAL_SPI_TRANSPORT_EVENT_RX_PAGE_READY;
}

uint32_t dual_spi_transport_on_dma_error_isr(
    dual_spi_transport_link_t *link,
    dual_spi_transport_fault_t fault,
    uint32_t detail)
{
    if (!is_initialized(link))
    {
        return DUAL_SPI_TRANSPORT_EVENT_NONE;
    }
    if (!is_transfer_active(link) ||
        (fault == DUAL_SPI_TRANSPORT_FAULT_NONE))
    {
        ++link->stats.stale_callback_count;
        return DUAL_SPI_TRANSPORT_EVENT_NONE;
    }

    ++link->stats.transfer_error_count;
    return enter_fault(link, fault, detail);
}

uint32_t dual_spi_transport_on_early_nss_isr(
    dual_spi_transport_link_t *link,
    uint32_t detail)
{
    if (!is_initialized(link) ||
        (link->role != DUAL_SPI_TRANSPORT_ROLE_SLAVE_B))
    {
        return DUAL_SPI_TRANSPORT_EVENT_NONE;
    }
    if (!is_transfer_active(link))
    {
        ++link->stats.stale_callback_count;
        return DUAL_SPI_TRANSPORT_EVENT_NONE;
    }

    ++link->stats.transfer_error_count;
    ++link->stats.early_nss_count;
    return enter_fault(link,
                       DUAL_SPI_TRANSPORT_FAULT_EARLY_NSS,
                       detail);
}

uint32_t dual_spi_transport_start_failed(
    dual_spi_transport_link_t *link,
    uint32_t detail)
{
    if (!is_initialized(link) ||
        (link->state != DUAL_SPI_TRANSPORT_STATE_STARTING))
    {
        return DUAL_SPI_TRANSPORT_EVENT_NONE;
    }

    ++link->stats.start_failure_count;
    return enter_fault(link, DUAL_SPI_TRANSPORT_FAULT_DMA_START, detail);
}

uint32_t dual_spi_transport_timeout(dual_spi_transport_link_t *link)
{
    if (!is_initialized(link) || !is_transfer_active(link))
    {
        return DUAL_SPI_TRANSPORT_EVENT_NONE;
    }

    ++link->stats.timeout_count;
    return enter_fault(link, DUAL_SPI_TRANSPORT_FAULT_TIMEOUT, 0U);
}

dual_spi_transport_result_t dual_spi_transport_release_rx_page(
    dual_spi_transport_link_t *link)
{
    if (!is_initialized(link))
    {
        return DUAL_SPI_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }
    if (link->state != DUAL_SPI_TRANSPORT_STATE_RX_READY)
    {
        return DUAL_SPI_TRANSPORT_RESULT_WRONG_STATE;
    }

    link->state = DUAL_SPI_TRANSPORT_STATE_IDLE;
    return DUAL_SPI_TRANSPORT_RESULT_OK;
}

dual_spi_transport_result_t dual_spi_transport_begin_recovery(
    dual_spi_transport_link_t *link)
{
    if (!is_initialized(link))
    {
        return DUAL_SPI_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }
    if (link->state != DUAL_SPI_TRANSPORT_STATE_RECOVERY_REQUIRED)
    {
        return DUAL_SPI_TRANSPORT_RESULT_WRONG_STATE;
    }

    link->master_cs_required = false;
    link->slave_ready_required = false;
    link->state = DUAL_SPI_TRANSPORT_STATE_RECOVERING;
    ++link->stats.recovery_count;
    return DUAL_SPI_TRANSPORT_RESULT_OK;
}

dual_spi_transport_result_t dual_spi_transport_finish_recovery(
    dual_spi_transport_link_t *link,
    bool success)
{
    if (!is_initialized(link))
    {
        return DUAL_SPI_TRANSPORT_RESULT_INVALID_ARGUMENT;
    }
    if (link->state != DUAL_SPI_TRANSPORT_STATE_RECOVERING)
    {
        return DUAL_SPI_TRANSPORT_RESULT_WRONG_STATE;
    }

    if (success)
    {
        link->state = DUAL_SPI_TRANSPORT_STATE_IDLE;
    }
    else
    {
        link->last_fault = DUAL_SPI_TRANSPORT_FAULT_ABORT_REINIT;
        link->last_fault_detail = 0U;
        link->state = DUAL_SPI_TRANSPORT_STATE_RECOVERY_REQUIRED;
        ++link->stats.recovery_failure_count;
    }
    return DUAL_SPI_TRANSPORT_RESULT_OK;
}




