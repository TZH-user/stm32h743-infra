#include "dual_spi_stm32_port.h"

#include "main.h"
#include "spi.h"

#include <stdbool.h>
#include <string.h>

#if defined(__GNUC__)
#define DUAL_SPI_DMA_BUFFER __attribute__((section(".dma_buffer"), aligned(32)))
#define DUAL_SPI_BDMA_BUFFER __attribute__((section(".bdma_buffer"), aligned(32)))
#else
#error "Define DualSpi DMA buffer placement for this compiler."
#endif

static uint8_t g_spi_a_tx_page[DUAL_SPI_PAGE_BYTES] DUAL_SPI_DMA_BUFFER;
static uint8_t g_spi_a_rx_page[DUAL_SPI_PAGE_BYTES] DUAL_SPI_DMA_BUFFER;
static uint8_t g_spi_b_tx_page[DUAL_SPI_PAGE_BYTES] DUAL_SPI_BDMA_BUFFER;
static uint8_t g_spi_b_rx_page[DUAL_SPI_PAGE_BYTES] DUAL_SPI_BDMA_BUFFER;

static dual_spi_transport_link_t g_links[2];
static volatile osThreadId_t g_task_id;
static volatile uint32_t g_transfer_started_ms[2];
static volatile bool g_timeout_armed[2];
/* Captured in the SPI6 error callback before this port starts recovery. */
static volatile uint32_t g_spi_b_error_dma_remaining;
static volatile uint32_t g_spi_a_ready_high_attempts;
static volatile uint32_t g_spi_a_ready_low_rejects;

static bool valid_link_id(dual_spi_link_id_t link_id)
{
    return (link_id == DUAL_SPI_LINK_A) || (link_id == DUAL_SPI_LINK_B);
}

static SPI_HandleTypeDef *spi_handle(dual_spi_link_id_t link_id)
{
    return (link_id == DUAL_SPI_LINK_A) ? &hspi2 : &hspi6;
}

static void set_safe_output(dual_spi_link_id_t link_id)
{
    if (link_id == DUAL_SPI_LINK_A)
    {
        HAL_GPIO_WritePin(ZYNQ_SPI_A_CS_GPIO_Port,
                          ZYNQ_SPI_A_CS_Pin,
                          GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(ZYNQ_SPI_B_READY_GPIO_Port,
                          ZYNQ_SPI_B_READY_Pin,
                          GPIO_PIN_RESET);
    }
}

static void guard_delay_1us(void)
{
    volatile uint32_t loop_count;

    loop_count = (SystemCoreClock + UINT32_C(999999)) /
                 UINT32_C(1000000);
    if (loop_count == 0U)
    {
        loop_count = 1U;
    }
    loop_count *= 2U;
    while (loop_count > 0U)
    {
        --loop_count;
        __NOP();
    }
    __DSB();
}

static bool dcache_is_enabled(void)
{
    return (SCB->CCR & SCB_CCR_DC_Msk) != 0U;
}

static void prepare_pages_for_dma(dual_spi_transport_link_t *link)
{
    if (dcache_is_enabled())
    {
        SCB_CleanDCache_by_Addr((uint32_t *)link->tx_page_storage,
                               (int32_t)DUAL_SPI_PAGE_BYTES);
        SCB_InvalidateDCache_by_Addr((uint32_t *)link->rx_page_storage,
                                    (int32_t)DUAL_SPI_PAGE_BYTES);
        __DSB();
    }
}

static void complete_rx_for_cpu(dual_spi_transport_link_t *link)
{
    if (dcache_is_enabled())
    {
        SCB_InvalidateDCache_by_Addr((uint32_t *)link->rx_page_storage,
                                    (int32_t)DUAL_SPI_PAGE_BYTES);
        __DSB();
    }
}

static uint32_t task_flags_for_event(dual_spi_link_id_t link_id,
                                     uint32_t transport_events)
{
    uint32_t flags = 0U;

    if (link_id == DUAL_SPI_LINK_A)
    {
        if ((transport_events & DUAL_SPI_TRANSPORT_EVENT_PEER_READY) != 0U)
        {
            flags |= DUAL_SPI_PORT_NOTIFY_A_READY;
        }
        if ((transport_events & DUAL_SPI_TRANSPORT_EVENT_RX_PAGE_READY) != 0U)
        {
            flags |= DUAL_SPI_PORT_NOTIFY_A_RX;
        }
        if ((transport_events & DUAL_SPI_TRANSPORT_EVENT_FAULT) != 0U)
        {
            flags |= DUAL_SPI_PORT_NOTIFY_A_FAULT;
        }
    }
    else
    {
        if ((transport_events & DUAL_SPI_TRANSPORT_EVENT_RX_PAGE_READY) != 0U)
        {
            flags |= DUAL_SPI_PORT_NOTIFY_B_RX;
        }
        if ((transport_events & DUAL_SPI_TRANSPORT_EVENT_FAULT) != 0U)
        {
            flags |= DUAL_SPI_PORT_NOTIFY_B_FAULT;
        }
        if ((transport_events & DUAL_SPI_TRANSPORT_EVENT_SLAVE_STARTED) != 0U)
        {
            flags |= DUAL_SPI_PORT_NOTIFY_B_START;
        }
    }
    return flags;
}

static void notify_task(dual_spi_link_id_t link_id, uint32_t events)
{
    uint32_t flags = task_flags_for_event(link_id, events);

    if ((flags != 0U) && (g_task_id != NULL))
    {
        (void)osThreadFlagsSet(g_task_id, flags);
    }
}

static uint32_t hal_failure_detail(HAL_StatusTypeDef status,
                                   SPI_HandleTypeDef *hspi)
{
    return ((uint32_t)status & UINT32_C(0xFF)) |
           (HAL_SPI_GetError(hspi) << 8U);
}

static bool slave_b_nss_active(void)
{
    return (HAL_GPIO_ReadPin(ZYNQ_SPI_B_NSS_GPIO_Port,
                             ZYNQ_SPI_B_NSS_Pin) == GPIO_PIN_RESET);
}

static uint16_t dma_remaining_bytes(const DMA_HandleTypeDef *hdma)
{
    uint32_t remaining;

    if (hdma == NULL)
    {
        return UINT16_MAX;
    }

    remaining = __HAL_DMA_GET_COUNTER(hdma);
    return (remaining > UINT16_MAX) ? UINT16_MAX : (uint16_t)remaining;
}

static uint32_t slave_b_dma_remaining_detail(void)
{
    uint16_t rx_remaining = dma_remaining_bytes(hspi6.hdmarx);
    uint16_t tx_remaining = dma_remaining_bytes(hspi6.hdmatx);

    return ((uint32_t)rx_remaining << 16U) | (uint32_t)tx_remaining;
}

static bool slave_b_page_clocking_complete(void)
{
    return (__HAL_SPI_GET_FLAG(&hspi6, SPI_FLAG_EOT) != RESET) ||
           ((dma_remaining_bytes(hspi6.hdmarx) == 0U) &&
            (dma_remaining_bytes(hspi6.hdmatx) == 0U));
}

static dual_spi_port_result_t map_transport_result(
    dual_spi_transport_result_t result)
{
    if (result == DUAL_SPI_TRANSPORT_RESULT_OK)
    {
        return DUAL_SPI_PORT_RESULT_OK;
    }
    if (result == DUAL_SPI_TRANSPORT_RESULT_PEER_NOT_READY)
    {
        return DUAL_SPI_PORT_RESULT_PEER_NOT_READY;
    }
    if (result == DUAL_SPI_TRANSPORT_RESULT_INVALID_ARGUMENT)
    {
        return DUAL_SPI_PORT_RESULT_INVALID_ARGUMENT;
    }
    return DUAL_SPI_PORT_RESULT_BUSY;
}

dual_spi_port_result_t dual_spi_stm32_port_init(osThreadId_t task_id)
{
    dual_spi_transport_result_t result_a;
    dual_spi_transport_result_t result_b;

    if (task_id == NULL)
    {
        return DUAL_SPI_PORT_RESULT_INVALID_ARGUMENT;
    }

    set_safe_output(DUAL_SPI_LINK_A);
    set_safe_output(DUAL_SPI_LINK_B);
    memset(g_spi_a_tx_page, 0, sizeof(g_spi_a_tx_page));
    memset(g_spi_a_rx_page, 0, sizeof(g_spi_a_rx_page));
    memset(g_spi_b_tx_page, 0, sizeof(g_spi_b_tx_page));
    memset(g_spi_b_rx_page, 0, sizeof(g_spi_b_rx_page));
    g_timeout_armed[DUAL_SPI_LINK_A] = false;
    g_timeout_armed[DUAL_SPI_LINK_B] = false;
    g_spi_b_error_dma_remaining = 0U;
    g_spi_a_ready_high_attempts = 0U;
    g_spi_a_ready_low_rejects = 0U;

    result_a = dual_spi_transport_init(&g_links[DUAL_SPI_LINK_A],
                                       DUAL_SPI_TRANSPORT_ROLE_MASTER_A,
                                       g_spi_a_tx_page,
                                       g_spi_a_rx_page,
                                       sizeof(g_spi_a_tx_page));
    result_b = dual_spi_transport_init(&g_links[DUAL_SPI_LINK_B],
                                       DUAL_SPI_TRANSPORT_ROLE_SLAVE_B,
                                       g_spi_b_tx_page,
                                       g_spi_b_rx_page,
                                       sizeof(g_spi_b_tx_page));
    if ((result_a != DUAL_SPI_TRANSPORT_RESULT_OK) ||
        (result_b != DUAL_SPI_TRANSPORT_RESULT_OK))
    {
        return DUAL_SPI_PORT_RESULT_INVALID_ARGUMENT;
    }

    g_task_id = task_id;
    return DUAL_SPI_PORT_RESULT_OK;
}

dual_spi_transport_link_t *dual_spi_stm32_port_link(dual_spi_link_id_t link_id)
{
    if (!valid_link_id(link_id))
    {
        return NULL;
    }
    return &g_links[link_id];
}

dual_spi_port_result_t dual_spi_stm32_port_start_master_a(void)
{
    dual_spi_transport_link_t *link = &g_links[DUAL_SPI_LINK_A];
    dual_spi_transport_result_t transport_result;
    HAL_StatusTypeDef hal_status;
    bool peer_ready;

    peer_ready = HAL_GPIO_ReadPin(ZYNQ_SPI_A_READY_GPIO_Port,
                                  ZYNQ_SPI_A_READY_Pin) == GPIO_PIN_SET;
    if (peer_ready)
    {
        ++g_spi_a_ready_high_attempts;
    }
    else
    {
        ++g_spi_a_ready_low_rejects;
    }
    transport_result = dual_spi_transport_master_begin(link, peer_ready);
    if (transport_result != DUAL_SPI_TRANSPORT_RESULT_OK)
    {
        return map_transport_result(transport_result);
    }

    prepare_pages_for_dma(link);
    guard_delay_1us();
    HAL_GPIO_WritePin(ZYNQ_SPI_A_CS_GPIO_Port,
                      ZYNQ_SPI_A_CS_Pin,
                      GPIO_PIN_RESET);
    guard_delay_1us();

    hal_status = HAL_SPI_TransmitReceive_DMA(&hspi2,
                                             link->tx_page_storage,
                                             link->rx_page_storage,
                                             DUAL_SPI_PAGE_BYTES);
    if (hal_status != HAL_OK)
    {
        set_safe_output(DUAL_SPI_LINK_A);
        (void)dual_spi_transport_start_failed(
            link,
            hal_failure_detail(hal_status, &hspi2));
        return DUAL_SPI_PORT_RESULT_HAL_ERROR;
    }

    transport_result = dual_spi_transport_dma_started(link);
    if (transport_result != DUAL_SPI_TRANSPORT_RESULT_OK)
    {
        (void)HAL_SPI_Abort(&hspi2);
        set_safe_output(DUAL_SPI_LINK_A);
        (void)dual_spi_transport_start_failed(link, UINT32_MAX);
        return DUAL_SPI_PORT_RESULT_HAL_ERROR;
    }

    g_transfer_started_ms[DUAL_SPI_LINK_A] = HAL_GetTick();
    g_timeout_armed[DUAL_SPI_LINK_A] = true;
    return DUAL_SPI_PORT_RESULT_OK;
}

dual_spi_port_result_t dual_spi_stm32_port_arm_slave_b(void)
{
    dual_spi_transport_link_t *link = &g_links[DUAL_SPI_LINK_B];
    dual_spi_transport_result_t transport_result;
    HAL_StatusTypeDef hal_status;

    if (HAL_GPIO_ReadPin(ZYNQ_SPI_B_NSS_GPIO_Port,
                         ZYNQ_SPI_B_NSS_Pin) != GPIO_PIN_SET)
    {
        return DUAL_SPI_PORT_RESULT_NSS_ACTIVE;
    }

    transport_result = dual_spi_transport_slave_arm_begin(link);
    if (transport_result != DUAL_SPI_TRANSPORT_RESULT_OK)
    {
        return map_transport_result(transport_result);
    }

    prepare_pages_for_dma(link);
    hal_status = HAL_SPI_TransmitReceive_DMA(&hspi6,
                                             link->tx_page_storage,
                                             link->rx_page_storage,
                                             DUAL_SPI_PAGE_BYTES);
    if (hal_status != HAL_OK)
    {
        set_safe_output(DUAL_SPI_LINK_B);
        (void)dual_spi_transport_start_failed(
            link,
            hal_failure_detail(hal_status, &hspi6));
        return DUAL_SPI_PORT_RESULT_HAL_ERROR;
    }

    transport_result = dual_spi_transport_dma_started(link);
    if (transport_result != DUAL_SPI_TRANSPORT_RESULT_OK)
    {
        (void)HAL_SPI_Abort(&hspi6);
        set_safe_output(DUAL_SPI_LINK_B);
        (void)dual_spi_transport_start_failed(link, UINT32_MAX);
        return DUAL_SPI_PORT_RESULT_HAL_ERROR;
    }

    if (slave_b_nss_active())
    {
        set_safe_output(DUAL_SPI_LINK_B);
        g_timeout_armed[DUAL_SPI_LINK_B] = false;
        (void)dual_spi_transport_on_early_nss_isr(
            link,
            slave_b_dma_remaining_detail());
        return DUAL_SPI_PORT_RESULT_NSS_ACTIVE;
    }

    HAL_GPIO_WritePin(ZYNQ_SPI_B_READY_GPIO_Port,
                      ZYNQ_SPI_B_READY_Pin,
                      GPIO_PIN_SET);
    g_timeout_armed[DUAL_SPI_LINK_B] = false;
    return DUAL_SPI_PORT_RESULT_OK;
}

dual_spi_port_result_t dual_spi_stm32_port_recover(dual_spi_link_id_t link_id)
{
    dual_spi_transport_link_t *link;
    SPI_HandleTypeDef *hspi;
    bool success = true;

    if (!valid_link_id(link_id))
    {
        return DUAL_SPI_PORT_RESULT_INVALID_ARGUMENT;
    }
    if ((link_id == DUAL_SPI_LINK_B) &&
        (HAL_GPIO_ReadPin(ZYNQ_SPI_B_NSS_GPIO_Port,
                          ZYNQ_SPI_B_NSS_Pin) != GPIO_PIN_SET))
    {
        return DUAL_SPI_PORT_RESULT_NSS_ACTIVE;
    }

    link = &g_links[link_id];
    if (dual_spi_transport_begin_recovery(link) !=
        DUAL_SPI_TRANSPORT_RESULT_OK)
    {
        return DUAL_SPI_PORT_RESULT_BUSY;
    }

    hspi = spi_handle(link_id);
    set_safe_output(link_id);
    g_timeout_armed[link_id] = false;

    if (HAL_SPI_Abort(hspi) != HAL_OK)
    {
        success = false;
    }
    if (HAL_SPI_DeInit(hspi) != HAL_OK)
    {
        success = false;
    }
    if (HAL_SPI_Init(hspi) != HAL_OK)
    {
        success = false;
    }

    (void)dual_spi_transport_finish_recovery(link, success);
    return success ? DUAL_SPI_PORT_RESULT_OK : DUAL_SPI_PORT_RESULT_HAL_ERROR;
}

uint32_t dual_spi_stm32_port_poll_timeouts(uint32_t timeout_ms)
{
    uint32_t now = HAL_GetTick();
    uint32_t flags = 0U;
    dual_spi_link_id_t link_id;

    if (timeout_ms == 0U)
    {
        return 0U;
    }

    for (link_id = DUAL_SPI_LINK_A;
         link_id <= DUAL_SPI_LINK_B;
         link_id = (dual_spi_link_id_t)((uint32_t)link_id + 1U))
    {
        dual_spi_transport_state_t state =
            dual_spi_transport_state(&g_links[link_id]);

        if ((link_id == DUAL_SPI_LINK_B) &&
            (state == DUAL_SPI_TRANSPORT_STATE_ARMED) &&
            (HAL_GPIO_ReadPin(ZYNQ_SPI_B_NSS_GPIO_Port,
                              ZYNQ_SPI_B_NSS_Pin) == GPIO_PIN_RESET))
        {
            uint32_t events;

            set_safe_output(DUAL_SPI_LINK_B);
            events = dual_spi_transport_on_slave_selected_isr(
                &g_links[DUAL_SPI_LINK_B]);
            if (events != 0U)
            {
                g_transfer_started_ms[DUAL_SPI_LINK_B] = now;
                flags |= task_flags_for_event(DUAL_SPI_LINK_B, events);
                state = DUAL_SPI_TRANSPORT_STATE_IN_FLIGHT;
            }
        }

        if (g_timeout_armed[link_id] &&
            (state == DUAL_SPI_TRANSPORT_STATE_IN_FLIGHT) &&
            ((uint32_t)(now - g_transfer_started_ms[link_id]) >= timeout_ms))
        {
            uint32_t events = dual_spi_transport_timeout(&g_links[link_id]);
            if (events != 0U)
            {
                set_safe_output(link_id);
                g_timeout_armed[link_id] = false;
                flags |= task_flags_for_event(link_id, events);
            }
        }
    }
    return flags;
}

const uint8_t *dual_spi_stm32_port_rx_page(dual_spi_link_id_t link_id)
{
    dual_spi_transport_link_t *link;

    if (!valid_link_id(link_id))
    {
        return NULL;
    }
    link = &g_links[link_id];
    if (dual_spi_transport_rx_page(link) == NULL)
    {
        return NULL;
    }

    complete_rx_for_cpu(link);
    return dual_spi_transport_rx_page(link);
}

uint32_t dual_spi_stm32_port_slave_b_last_error_dma_remaining(void)
{
    return g_spi_b_error_dma_remaining;
}

uint32_t dual_spi_stm32_port_a_ready_high_attempts(void)
{
    return g_spi_a_ready_high_attempts;
}

uint32_t dual_spi_stm32_port_a_ready_low_rejects(void)
{
    return g_spi_a_ready_low_rejects;
}

void DualSpiStm32Port_OnGpioExti(uint16_t gpio_pin)
{
    uint32_t events;

    if (gpio_pin == ZYNQ_SPI_A_READY_Pin)
    {
        events = dual_spi_transport_on_peer_ready_isr(
            &g_links[DUAL_SPI_LINK_A]);
        notify_task(DUAL_SPI_LINK_A, events);
        return;
    }

    if (gpio_pin != ZYNQ_SPI_B_NSS_Pin)
    {
        return;
    }

    set_safe_output(DUAL_SPI_LINK_B);
    if (HAL_GPIO_ReadPin(ZYNQ_SPI_B_NSS_GPIO_Port,
                         ZYNQ_SPI_B_NSS_Pin) == GPIO_PIN_RESET)
    {
        events = dual_spi_transport_on_slave_selected_isr(
            &g_links[DUAL_SPI_LINK_B]);
        if ((events & DUAL_SPI_TRANSPORT_EVENT_SLAVE_STARTED) != 0U)
        {
            g_transfer_started_ms[DUAL_SPI_LINK_B] = HAL_GetTick();
            g_timeout_armed[DUAL_SPI_LINK_B] = true;
        }
    }
    else if (!slave_b_page_clocking_complete())
    {
        g_timeout_armed[DUAL_SPI_LINK_B] = false;
        events = dual_spi_transport_on_early_nss_isr(
            &g_links[DUAL_SPI_LINK_B],
            slave_b_dma_remaining_detail());
    }
    else
    {
        events = DUAL_SPI_TRANSPORT_EVENT_NONE;
    }

    notify_task(DUAL_SPI_LINK_B, events);
}

void HAL_SPI_TxRxHalfCpltCallback(SPI_HandleTypeDef *hspi)
{
    uint32_t events;

    if ((hspi != &hspi6) ||
        (dual_spi_transport_state(&g_links[DUAL_SPI_LINK_B]) !=
         DUAL_SPI_TRANSPORT_STATE_ARMED))
    {
        return;
    }

    set_safe_output(DUAL_SPI_LINK_B);
    events = dual_spi_transport_on_slave_selected_isr(
        &g_links[DUAL_SPI_LINK_B]);
    if (events != 0U)
    {
        g_transfer_started_ms[DUAL_SPI_LINK_B] = HAL_GetTick();
        g_timeout_armed[DUAL_SPI_LINK_B] = true;
    }
    notify_task(DUAL_SPI_LINK_B, events);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    dual_spi_link_id_t link_id;
    uint32_t events;

    if (hspi == &hspi2)
    {
        link_id = DUAL_SPI_LINK_A;
        guard_delay_1us();
    }
    else if (hspi == &hspi6)
    {
        link_id = DUAL_SPI_LINK_B;
        g_spi_b_error_dma_remaining = slave_b_dma_remaining_detail();
    }
    else
    {
        return;
    }

    set_safe_output(link_id);
    g_timeout_armed[link_id] = false;
    events = dual_spi_transport_on_dma_complete_isr(&g_links[link_id]);
    notify_task(link_id, events);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    dual_spi_link_id_t link_id;
    dual_spi_transport_fault_t fault;
    uint32_t error;
    uint32_t events;

    if (hspi == &hspi2)
    {
        link_id = DUAL_SPI_LINK_A;
    }
    else if (hspi == &hspi6)
    {
        link_id = DUAL_SPI_LINK_B;
    }
    else
    {
        return;
    }

    error = HAL_SPI_GetError(hspi);
    fault = ((error & HAL_SPI_ERROR_DMA) != 0U)
                ? DUAL_SPI_TRANSPORT_FAULT_DMA_TRANSFER
                : DUAL_SPI_TRANSPORT_FAULT_SPI;
    set_safe_output(link_id);
    g_timeout_armed[link_id] = false;
    events = dual_spi_transport_on_dma_error_isr(&g_links[link_id],
                                                  fault,
                                                  error);
    notify_task(link_id, events);
}



