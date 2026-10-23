#ifndef DUAL_SPI_TRANSPORT_H
#define DUAL_SPI_TRANSPORT_H

#include "dual_spi_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DUAL_SPI_TRANSPORT_DMA_ALIGNMENT 32U

#define DUAL_SPI_TRANSPORT_EVENT_NONE          UINT32_C(0x00000000)
#define DUAL_SPI_TRANSPORT_EVENT_PEER_READY    UINT32_C(0x00000001)
#define DUAL_SPI_TRANSPORT_EVENT_RX_PAGE_READY UINT32_C(0x00000002)
#define DUAL_SPI_TRANSPORT_EVENT_FAULT         UINT32_C(0x00000004)
#define DUAL_SPI_TRANSPORT_EVENT_SLAVE_STARTED UINT32_C(0x00000008)

typedef enum
{
    DUAL_SPI_TRANSPORT_ROLE_MASTER_A = 0,
    DUAL_SPI_TRANSPORT_ROLE_SLAVE_B = 1
} dual_spi_transport_role_t;

typedef enum
{
    DUAL_SPI_TRANSPORT_STATE_STOPPED = 0,
    DUAL_SPI_TRANSPORT_STATE_IDLE,
    DUAL_SPI_TRANSPORT_STATE_STARTING,
    DUAL_SPI_TRANSPORT_STATE_ARMED,
    DUAL_SPI_TRANSPORT_STATE_IN_FLIGHT,
    DUAL_SPI_TRANSPORT_STATE_RX_READY,
    DUAL_SPI_TRANSPORT_STATE_RECOVERY_REQUIRED,
    DUAL_SPI_TRANSPORT_STATE_RECOVERING
} dual_spi_transport_state_t;

typedef enum
{
    DUAL_SPI_TRANSPORT_RESULT_OK = 0,
    DUAL_SPI_TRANSPORT_RESULT_INVALID_ARGUMENT,
    DUAL_SPI_TRANSPORT_RESULT_BAD_BUFFER,
    DUAL_SPI_TRANSPORT_RESULT_WRONG_ROLE,
    DUAL_SPI_TRANSPORT_RESULT_WRONG_STATE,
    DUAL_SPI_TRANSPORT_RESULT_PEER_NOT_READY
} dual_spi_transport_result_t;

typedef enum
{
    DUAL_SPI_TRANSPORT_FAULT_NONE = 0,
    DUAL_SPI_TRANSPORT_FAULT_DMA_START,
    DUAL_SPI_TRANSPORT_FAULT_DMA_TRANSFER,
    DUAL_SPI_TRANSPORT_FAULT_SPI,
    DUAL_SPI_TRANSPORT_FAULT_TIMEOUT,
    DUAL_SPI_TRANSPORT_FAULT_EARLY_NSS,
    DUAL_SPI_TRANSPORT_FAULT_ABORT_REINIT
} dual_spi_transport_fault_t;

typedef struct
{
    volatile uint32_t ready_irq_count;
    volatile uint32_t dma_start_count;
    volatile uint32_t completed_count;
    volatile uint32_t start_failure_count;
    volatile uint32_t transfer_error_count;
    volatile uint32_t timeout_count;
    volatile uint32_t early_nss_count;
    volatile uint32_t recovery_count;
    volatile uint32_t recovery_failure_count;
    volatile uint32_t stale_callback_count;
} dual_spi_transport_stats_t;

/*
 * Treat this structure as private storage. Access state and pages through the
 * functions below. ISR entry points only perform bounded state updates and
 * return notification bits; CRC/TLV processing remains in task context.
 */
typedef struct
{
    uint8_t *tx_page_storage;
    uint8_t *rx_page_storage;
    size_t page_bytes;
    dual_spi_transport_role_t role;
    volatile dual_spi_transport_state_t state;
    volatile dual_spi_transport_fault_t last_fault;
    volatile uint32_t last_fault_detail;
    volatile bool master_cs_required;
    volatile bool slave_ready_required;
    bool initialized;
    dual_spi_transport_stats_t stats;
} dual_spi_transport_link_t;

dual_spi_transport_result_t dual_spi_transport_init(
    dual_spi_transport_link_t *link,
    dual_spi_transport_role_t role,
    uint8_t *tx_page,
    uint8_t *rx_page,
    size_t page_bytes);

dual_spi_transport_state_t dual_spi_transport_state(
    const dual_spi_transport_link_t *link);
uint8_t *dual_spi_transport_tx_page(dual_spi_transport_link_t *link);
const uint8_t *dual_spi_transport_rx_page(
    const dual_spi_transport_link_t *link);
const dual_spi_transport_stats_t *dual_spi_transport_stats(
    const dual_spi_transport_link_t *link);
dual_spi_transport_fault_t dual_spi_transport_last_fault(
    const dual_spi_transport_link_t *link,
    uint32_t *detail);

bool dual_spi_transport_master_cs_required(
    const dual_spi_transport_link_t *link);
bool dual_spi_transport_slave_ready_required(
    const dual_spi_transport_link_t *link);

uint32_t dual_spi_transport_on_peer_ready_isr(
    dual_spi_transport_link_t *link);
dual_spi_transport_result_t dual_spi_transport_master_begin(
    dual_spi_transport_link_t *link,
    bool peer_ready_level);
dual_spi_transport_result_t dual_spi_transport_slave_arm_begin(
    dual_spi_transport_link_t *link);
dual_spi_transport_result_t dual_spi_transport_dma_started(
    dual_spi_transport_link_t *link);
uint32_t dual_spi_transport_on_slave_selected_isr(
    dual_spi_transport_link_t *link);
uint32_t dual_spi_transport_on_dma_complete_isr(
    dual_spi_transport_link_t *link);
uint32_t dual_spi_transport_on_dma_error_isr(
    dual_spi_transport_link_t *link,
    dual_spi_transport_fault_t fault,
    uint32_t detail);
uint32_t dual_spi_transport_on_early_nss_isr(
    dual_spi_transport_link_t *link,
    uint32_t detail);
uint32_t dual_spi_transport_start_failed(
    dual_spi_transport_link_t *link,
    uint32_t detail);
uint32_t dual_spi_transport_timeout(dual_spi_transport_link_t *link);

dual_spi_transport_result_t dual_spi_transport_release_rx_page(
    dual_spi_transport_link_t *link);
dual_spi_transport_result_t dual_spi_transport_begin_recovery(
    dual_spi_transport_link_t *link);
dual_spi_transport_result_t dual_spi_transport_finish_recovery(
    dual_spi_transport_link_t *link,
    bool success);

#ifdef __cplusplus
}
#endif

#endif




