#include "dual_spi_transport.h"

#include <stdint.h>
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

static uint8_t g_tx_page[DUAL_SPI_PAGE_BYTES]
    __attribute__((aligned(DUAL_SPI_TRANSPORT_DMA_ALIGNMENT)));

static uint8_t g_rx_page[DUAL_SPI_PAGE_BYTES]
    __attribute__((aligned(DUAL_SPI_TRANSPORT_DMA_ALIGNMENT)));

static void test_init_validates_dma_buffers_and_exposes_only_owned_pages(void)
{
    dual_spi_transport_link_t link;

    memset(&link, 0xA5, sizeof(link));
    CHECK(dual_spi_transport_init(NULL,
                                  DUAL_SPI_TRANSPORT_ROLE_MASTER_A,
                                  g_tx_page,
                                  g_rx_page,
                                  sizeof(g_tx_page)) ==
          DUAL_SPI_TRANSPORT_RESULT_INVALID_ARGUMENT);
    CHECK(dual_spi_transport_init(&link,
                                  DUAL_SPI_TRANSPORT_ROLE_MASTER_A,
                                  &g_tx_page[1],
                                  g_rx_page,
                                  sizeof(g_tx_page) - 1U) ==
          DUAL_SPI_TRANSPORT_RESULT_BAD_BUFFER);
    CHECK(dual_spi_transport_init(&link,
                                  DUAL_SPI_TRANSPORT_ROLE_MASTER_A,
                                  g_tx_page,
                                  g_tx_page,
                                  sizeof(g_tx_page)) ==
          DUAL_SPI_TRANSPORT_RESULT_BAD_BUFFER);

    CHECK(dual_spi_transport_init(&link,
                                  DUAL_SPI_TRANSPORT_ROLE_MASTER_A,
                                  g_tx_page,
                                  g_rx_page,
                                  sizeof(g_tx_page)) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_state(&link) == DUAL_SPI_TRANSPORT_STATE_IDLE);
    CHECK(dual_spi_transport_tx_page(&link) == g_tx_page);
    CHECK(dual_spi_transport_rx_page(&link) == NULL);
    CHECK(!dual_spi_transport_master_cs_required(&link));
    CHECK(!dual_spi_transport_slave_ready_required(&link));
}

static void test_master_ready_to_completed_page_lifecycle(void)
{
    dual_spi_transport_link_t link;
    const dual_spi_transport_stats_t *stats;
    uint32_t events;

    memset(g_tx_page, 0x31, sizeof(g_tx_page));
    memset(g_rx_page, 0x00, sizeof(g_rx_page));
    CHECK(dual_spi_transport_init(&link,
                                  DUAL_SPI_TRANSPORT_ROLE_MASTER_A,
                                  g_tx_page,
                                  g_rx_page,
                                  sizeof(g_tx_page)) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);

    events = dual_spi_transport_on_peer_ready_isr(&link);
    CHECK(events == DUAL_SPI_TRANSPORT_EVENT_PEER_READY);

    CHECK(dual_spi_transport_master_begin(&link, false) ==
          DUAL_SPI_TRANSPORT_RESULT_PEER_NOT_READY);
    CHECK(dual_spi_transport_state(&link) == DUAL_SPI_TRANSPORT_STATE_IDLE);

    CHECK(dual_spi_transport_master_begin(&link, true) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_state(&link) == DUAL_SPI_TRANSPORT_STATE_STARTING);
    CHECK(dual_spi_transport_master_cs_required(&link));
    CHECK(dual_spi_transport_tx_page(&link) == NULL);

    CHECK(dual_spi_transport_dma_started(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_state(&link) == DUAL_SPI_TRANSPORT_STATE_IN_FLIGHT);

    g_rx_page[0] = 0x5AU;
    events = dual_spi_transport_on_dma_complete_isr(&link);
    CHECK(events == DUAL_SPI_TRANSPORT_EVENT_RX_PAGE_READY);
    CHECK(dual_spi_transport_state(&link) == DUAL_SPI_TRANSPORT_STATE_RX_READY);
    CHECK(!dual_spi_transport_master_cs_required(&link));
    CHECK(dual_spi_transport_rx_page(&link) == g_rx_page);
    CHECK(dual_spi_transport_rx_page(&link)[0] == 0x5AU);

    stats = dual_spi_transport_stats(&link);
    CHECK(stats != NULL);
    CHECK(stats->ready_irq_count == 1U);
    CHECK(stats->dma_start_count == 1U);
    CHECK(stats->completed_count == 1U);

    CHECK(dual_spi_transport_release_rx_page(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_state(&link) == DUAL_SPI_TRANSPORT_STATE_IDLE);
    CHECK(dual_spi_transport_rx_page(&link) == NULL);
    CHECK(dual_spi_transport_tx_page(&link) == g_tx_page);
}

static void test_slave_arms_before_ready_and_accepts_optional_start_notice(void)
{
    dual_spi_transport_link_t link;
    const dual_spi_transport_stats_t *stats;
    uint32_t events;

    CHECK(dual_spi_transport_init(&link,
                                  DUAL_SPI_TRANSPORT_ROLE_SLAVE_B,
                                  g_tx_page,
                                  g_rx_page,
                                  sizeof(g_tx_page)) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_master_begin(&link, true) ==
          DUAL_SPI_TRANSPORT_RESULT_WRONG_ROLE);

    CHECK(dual_spi_transport_slave_arm_begin(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_state(&link) == DUAL_SPI_TRANSPORT_STATE_STARTING);
    CHECK(!dual_spi_transport_slave_ready_required(&link));

    CHECK(dual_spi_transport_dma_started(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_state(&link) == DUAL_SPI_TRANSPORT_STATE_ARMED);
    CHECK(dual_spi_transport_slave_ready_required(&link));
    CHECK(dual_spi_transport_tx_page(&link) == NULL);

    events = dual_spi_transport_on_slave_selected_isr(&link);
    CHECK(events == DUAL_SPI_TRANSPORT_EVENT_SLAVE_STARTED);
    CHECK(dual_spi_transport_state(&link) == DUAL_SPI_TRANSPORT_STATE_IN_FLIGHT);
    CHECK(!dual_spi_transport_slave_ready_required(&link));

    CHECK(dual_spi_transport_on_slave_selected_isr(&link) ==
          DUAL_SPI_TRANSPORT_EVENT_NONE);
    events = dual_spi_transport_on_dma_complete_isr(&link);
    CHECK(events == DUAL_SPI_TRANSPORT_EVENT_RX_PAGE_READY);
    CHECK(dual_spi_transport_release_rx_page(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);

    /* Completion directly from ARMED is valid when no NSS-falling IRQ exists. */
    CHECK(dual_spi_transport_slave_arm_begin(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_dma_started(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_on_dma_complete_isr(&link) ==
          DUAL_SPI_TRANSPORT_EVENT_RX_PAGE_READY);
    CHECK(!dual_spi_transport_slave_ready_required(&link));

    stats = dual_spi_transport_stats(&link);
    CHECK(stats->dma_start_count == 2U);
    CHECK(stats->completed_count == 2U);
    CHECK(stats->stale_callback_count == 1U);
}

static void test_start_failure_and_recovery_preserve_tx_image(void)
{
    dual_spi_transport_link_t link;
    const dual_spi_transport_stats_t *stats;
    uint32_t detail = 0U;

    memset(g_tx_page, 0xA7, sizeof(g_tx_page));
    CHECK(dual_spi_transport_init(&link,
                                  DUAL_SPI_TRANSPORT_ROLE_MASTER_A,
                                  g_tx_page,
                                  g_rx_page,
                                  sizeof(g_tx_page)) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_master_begin(&link, true) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);

    CHECK(dual_spi_transport_start_failed(&link, UINT32_C(0x11223344)) ==
          DUAL_SPI_TRANSPORT_EVENT_FAULT);
    CHECK(dual_spi_transport_state(&link) ==
          DUAL_SPI_TRANSPORT_STATE_RECOVERY_REQUIRED);
    CHECK(!dual_spi_transport_master_cs_required(&link));
    CHECK(dual_spi_transport_last_fault(&link, &detail) ==
          DUAL_SPI_TRANSPORT_FAULT_DMA_START);
    CHECK(detail == UINT32_C(0x11223344));

    /* A completion from the abandoned DMA must not publish a partial page. */
    CHECK(dual_spi_transport_on_dma_complete_isr(&link) ==
          DUAL_SPI_TRANSPORT_EVENT_NONE);
    CHECK(dual_spi_transport_rx_page(&link) == NULL);

    CHECK(dual_spi_transport_begin_recovery(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_state(&link) ==
          DUAL_SPI_TRANSPORT_STATE_RECOVERING);
    CHECK(dual_spi_transport_finish_recovery(&link, false) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_state(&link) ==
          DUAL_SPI_TRANSPORT_STATE_RECOVERY_REQUIRED);
    CHECK(dual_spi_transport_last_fault(&link, &detail) ==
          DUAL_SPI_TRANSPORT_FAULT_ABORT_REINIT);

    CHECK(dual_spi_transport_begin_recovery(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_finish_recovery(&link, true) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_state(&link) == DUAL_SPI_TRANSPORT_STATE_IDLE);
    CHECK(dual_spi_transport_tx_page(&link)[0] == 0xA7U);

    stats = dual_spi_transport_stats(&link);
    CHECK(stats->start_failure_count == 1U);
    CHECK(stats->recovery_count == 2U);
    CHECK(stats->recovery_failure_count == 1U);
    CHECK(stats->stale_callback_count == 1U);
}

static void test_dma_error_timeout_and_late_callback_are_fail_closed(void)
{
    dual_spi_transport_link_t link;
    const dual_spi_transport_stats_t *stats;
    uint32_t detail = 0U;

    CHECK(dual_spi_transport_init(&link,
                                  DUAL_SPI_TRANSPORT_ROLE_SLAVE_B,
                                  g_tx_page,
                                  g_rx_page,
                                  sizeof(g_tx_page)) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_slave_arm_begin(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_dma_started(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_on_slave_selected_isr(&link) ==
          DUAL_SPI_TRANSPORT_EVENT_SLAVE_STARTED);

    CHECK(dual_spi_transport_on_dma_error_isr(
              &link,
              DUAL_SPI_TRANSPORT_FAULT_SPI,
              UINT32_C(0x55AA)) == DUAL_SPI_TRANSPORT_EVENT_FAULT);
    CHECK(dual_spi_transport_state(&link) ==
          DUAL_SPI_TRANSPORT_STATE_RECOVERY_REQUIRED);
    CHECK(!dual_spi_transport_slave_ready_required(&link));
    CHECK(dual_spi_transport_last_fault(&link, &detail) ==
          DUAL_SPI_TRANSPORT_FAULT_SPI);
    CHECK(detail == UINT32_C(0x55AA));
    CHECK(dual_spi_transport_on_dma_complete_isr(&link) ==
          DUAL_SPI_TRANSPORT_EVENT_NONE);

    CHECK(dual_spi_transport_begin_recovery(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_finish_recovery(&link, true) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_slave_arm_begin(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_dma_started(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);

    CHECK(dual_spi_transport_timeout(&link) ==
          DUAL_SPI_TRANSPORT_EVENT_FAULT);
    CHECK(dual_spi_transport_last_fault(&link, &detail) ==
          DUAL_SPI_TRANSPORT_FAULT_TIMEOUT);
    CHECK(!dual_spi_transport_slave_ready_required(&link));
    CHECK(dual_spi_transport_timeout(&link) ==
          DUAL_SPI_TRANSPORT_EVENT_NONE);

    stats = dual_spi_transport_stats(&link);
    CHECK(stats->transfer_error_count == 1U);
    CHECK(stats->timeout_count == 1U);
    CHECK(stats->stale_callback_count == 1U);
}

static void test_slave_early_nss_requires_recovery_and_never_publishes_page(void)
{
    dual_spi_transport_link_t link;
    const dual_spi_transport_stats_t *stats;
    uint32_t detail = 0U;
    uint32_t events;

    CHECK(dual_spi_transport_init(&link,
                                  DUAL_SPI_TRANSPORT_ROLE_SLAVE_B,
                                  g_tx_page,
                                  g_rx_page,
                                  sizeof(g_tx_page)) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_slave_arm_begin(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);
    CHECK(dual_spi_transport_dma_started(&link) ==
          DUAL_SPI_TRANSPORT_RESULT_OK);

    /*
     * Releasing NSS before a complete page is observable even when a short
     * pulse finishes before the task-level polling fallback sees NSS low.
     */
    events = dual_spi_transport_on_early_nss_isr(
        &link,
        UINT32_C(0x02000300));
    CHECK(events == DUAL_SPI_TRANSPORT_EVENT_FAULT);
    CHECK(dual_spi_transport_state(&link) ==
          DUAL_SPI_TRANSPORT_STATE_RECOVERY_REQUIRED);
    CHECK(!dual_spi_transport_slave_ready_required(&link));
    CHECK(dual_spi_transport_rx_page(&link) == NULL);
    CHECK(dual_spi_transport_last_fault(&link, &detail) ==
          DUAL_SPI_TRANSPORT_FAULT_EARLY_NSS);
    CHECK(detail == UINT32_C(0x02000300));

    /* A late DMA callback from the aborted partial page must be ignored. */
    CHECK(dual_spi_transport_on_dma_complete_isr(&link) ==
          DUAL_SPI_TRANSPORT_EVENT_NONE);

    stats = dual_spi_transport_stats(&link);
    CHECK(stats != NULL);
    CHECK(stats->transfer_error_count == 1U);
    CHECK(stats->early_nss_count == 1U);
    CHECK(stats->stale_callback_count == 1U);
}

int main(void)
{
    test_init_validates_dma_buffers_and_exposes_only_owned_pages();
    test_master_ready_to_completed_page_lifecycle();
    test_slave_arms_before_ready_and_accepts_optional_start_notice();
    test_start_failure_and_recovery_preserve_tx_image();
    test_dma_error_timeout_and_late_callback_are_fail_closed();
    test_slave_early_nss_requires_recovery_and_never_publishes_page();

    if (g_failures != 0U)
    {
        printf("%u transport test assertion(s) failed\n", g_failures);
        return 1;
    }

    puts("dual_spi_transport_tests: PASS");
    return 0;
}


