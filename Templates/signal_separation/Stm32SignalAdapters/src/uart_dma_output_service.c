/**
  ******************************************************************************
  * @file    debug_io.c
  * @brief   SWO printf redirection and USART VCP DMA ring-buffer logger.
  ******************************************************************************
  */

#include "debug_io.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_IO_DCACHE_LINE_SIZE  32U

/*
 * DMA1/DMA2 on STM32H743 cannot access DTCM.
 * The linker script maps the ".dma_buffer.uart" section to RAM_D2 after the
 * fixed DAC buffer.
 *
 * The start address is aligned to one Cortex-M7 cache line so cache
 * maintenance never touches unrelated objects before the ring buffer.
 */
__attribute__((section(".dma_buffer.uart"),
               aligned(DEBUG_IO_DCACHE_LINE_SIZE)))
static uint8_t s_vcp_tx_ring[DEBUG_IO_VCP_TX_RING_SIZE];

static UART_HandleTypeDef *s_vcp_uart = NULL;
static volatile bool s_swo_enabled = false;

static volatile uint32_t s_vcp_tx_head = 0U;
static volatile uint32_t s_vcp_tx_tail = 0U;
static volatile uint16_t s_vcp_dma_length = 0U;
static volatile bool s_vcp_dma_active = false;
static volatile uint32_t s_vcp_dropped_bytes = 0U;
static volatile uint32_t s_vcp_peak_queued_bytes = 0U;

static uint32_t debug_io_irq_save(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void debug_io_irq_restore(uint32_t primask)
{
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }
}

static void debug_io_clean_dcache(const void *address, uint32_t length)
{
#if (__DCACHE_PRESENT == 1U)
    if ((address == NULL) || (length == 0U))
    {
        return;
    }

    uintptr_t start =
        ((uintptr_t)address)
        & ~((uintptr_t)DEBUG_IO_DCACHE_LINE_SIZE - 1U);

    uintptr_t end =
        ((uintptr_t)address + (uintptr_t)length
         + (uintptr_t)DEBUG_IO_DCACHE_LINE_SIZE - 1U)
        & ~((uintptr_t)DEBUG_IO_DCACHE_LINE_SIZE - 1U);

    SCB_CleanDCache_by_Addr(
        (uint32_t *)start,
        (int32_t)(end - start)
    );

    __DSB();
    __ISB();
#else
    (void)address;
    (void)length;
#endif
}

/*
 * Start one contiguous DMA transfer from the ring.
 *
 * Call only while interrupts are disabled or from the UART callback ISR.
 */
static void debug_io_vcp_start_locked(void)
{
    if ((s_vcp_uart == NULL)
        || (s_vcp_uart->hdmatx == NULL)
        || s_vcp_dma_active
        || (s_vcp_tx_head == s_vcp_tx_tail))
    {
        return;
    }

    uint32_t contiguous_length;

    if (s_vcp_tx_head > s_vcp_tx_tail)
    {
        contiguous_length = s_vcp_tx_head - s_vcp_tx_tail;
    }
    else
    {
        contiguous_length =
            DEBUG_IO_VCP_TX_RING_SIZE - s_vcp_tx_tail;
    }

    if (contiguous_length > UINT16_MAX)
    {
        contiguous_length = UINT16_MAX;
    }

    debug_io_clean_dcache(
        &s_vcp_tx_ring[s_vcp_tx_tail],
        contiguous_length
    );

    s_vcp_dma_length = (uint16_t)contiguous_length;
    s_vcp_dma_active = true;

    if (HAL_UART_Transmit_DMA(
            s_vcp_uart,
            &s_vcp_tx_ring[s_vcp_tx_tail],
            s_vcp_dma_length
        ) != HAL_OK)
    {
        s_vcp_dma_length = 0U;
        s_vcp_dma_active = false;
    }
}

static uint32_t debug_io_vcp_free_locked(void)
{
    if (s_vcp_tx_head >= s_vcp_tx_tail)
    {
        return DEBUG_IO_VCP_TX_RING_SIZE
               - (s_vcp_tx_head - s_vcp_tx_tail)
               - 1U;
    }

    return s_vcp_tx_tail - s_vcp_tx_head - 1U;
}

static uint32_t debug_io_vcp_used_locked(void)
{
    return DEBUG_IO_VCP_TX_RING_SIZE
        - 1U
        - debug_io_vcp_free_locked();
}

void DebugIO_Init(UART_HandleTypeDef *vcp_uart)
{
    uint32_t primask = debug_io_irq_save();

    s_vcp_uart = vcp_uart;
    s_swo_enabled = (DEBUG_IO_SWO_AVAILABLE != 0U);
    s_vcp_tx_head = 0U;
    s_vcp_tx_tail = 0U;
    s_vcp_dma_length = 0U;
    s_vcp_dma_active = false;
    s_vcp_dropped_bytes = 0U;
    s_vcp_peak_queued_bytes = 0U;

    debug_io_irq_restore(primask);

    /*
     * Disable stdout buffering so each printf() call reaches _write()
     * immediately. With PB3 used by AD9226, the SWO sink is intentionally
     * muted and printf() becomes a harmless no-op.
     */
    (void)setvbuf(stdout, NULL, _IONBF, 0);
}

void DebugIO_SetSwoEnabled(bool enabled)
{
#if DEBUG_IO_SWO_AVAILABLE
    s_swo_enabled = enabled;
#else
    (void)enabled;
    s_swo_enabled = false;
#endif
    __DMB();
}

bool DebugIO_IsSwoEnabled(void)
{
    return s_swo_enabled;
}

int DebugIO_SwoWrite(const uint8_t *data, size_t length)
{
    if ((data == NULL) && (length > 0U))
    {
        return -1;
    }

    if (!s_swo_enabled)
    {
        return (int)length;
    }

    for (size_t i = 0U; i < length; ++i)
    {
        ITM_SendChar((uint32_t)data[i]);
    }

    return (int)length;
}

/*
 * Strong newlib write hook. It overrides CubeMX's weak _write(). When SWO is
 * unavailable, printf() reports success without touching PB3.
 */
int _write(int file, char *ptr, int len)
{
    (void)file;

    if ((ptr == NULL) || (len <= 0))
    {
        return 0;
    }

    return DebugIO_SwoWrite(
        (const uint8_t *)ptr,
        (size_t)len
    );
}

int vcp_write(const uint8_t *data, uint16_t length)
{
    if (length == 0U)
    {
        return 0;
    }

    if ((data == NULL) || (s_vcp_uart == NULL))
    {
        return -1;
    }

    uint32_t primask = debug_io_irq_save();
    uint32_t free_space = debug_io_vcp_free_locked();

    if ((uint32_t)length > free_space)
    {
        s_vcp_dropped_bytes += length;
        debug_io_irq_restore(primask);
        return -2;
    }

    uint32_t first_part =
        DEBUG_IO_VCP_TX_RING_SIZE - s_vcp_tx_head;

    if (first_part > (uint32_t)length)
    {
        first_part = length;
    }

    memcpy(
        &s_vcp_tx_ring[s_vcp_tx_head],
        data,
        first_part
    );

    uint32_t second_part = (uint32_t)length - first_part;

    if (second_part > 0U)
    {
        memcpy(
            &s_vcp_tx_ring[0],
            &data[first_part],
            second_part
        );
    }

    s_vcp_tx_head =
        (s_vcp_tx_head + (uint32_t)length)
        % DEBUG_IO_VCP_TX_RING_SIZE;

    uint32_t queued_bytes = debug_io_vcp_used_locked();
    if (queued_bytes > s_vcp_peak_queued_bytes)
    {
        s_vcp_peak_queued_bytes = queued_bytes;
    }

    debug_io_vcp_start_locked();
    debug_io_irq_restore(primask);

    return (int)length;
}

int vcp_printf(const char *format, ...)
{
    if (format == NULL)
    {
        return -1;
    }

    char buffer[DEBUG_IO_VCP_PRINTF_BUFFER_SIZE];

    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length < 0)
    {
        return length;
    }

    uint16_t transmit_length =
        (length >= (int)sizeof(buffer))
        ? (uint16_t)(sizeof(buffer) - 1U)
        : (uint16_t)length;

    return vcp_write(
        (const uint8_t *)buffer,
        transmit_length
    );
}

uint32_t DebugIO_GetVcpDroppedBytes(void)
{
    return s_vcp_dropped_bytes;
}

uint32_t DebugIO_GetVcpQueuedBytes(void)
{
    uint32_t primask = debug_io_irq_save();
    uint32_t queued_bytes = debug_io_vcp_used_locked();
    debug_io_irq_restore(primask);
    return queued_bytes;
}

uint32_t DebugIO_GetVcpPeakQueuedBytes(void)
{
    return s_vcp_peak_queued_bytes;
}

bool DebugIO_IsVcpBusy(void)
{
    return s_vcp_dma_active;
}

bool DebugIO_IsVcpIdle(void)
{
    uint32_t primask = debug_io_irq_save();
    bool idle = !s_vcp_dma_active && (s_vcp_tx_head == s_vcp_tx_tail);
    debug_io_irq_restore(primask);
    return idle;
}

void DebugIO_OnUartTxComplete(UART_HandleTypeDef *huart)
{
    if ((huart == NULL) || (huart != s_vcp_uart))
    {
        return;
    }

    s_vcp_tx_tail =
        (s_vcp_tx_tail + (uint32_t)s_vcp_dma_length)
        % DEBUG_IO_VCP_TX_RING_SIZE;

    s_vcp_dma_length = 0U;
    s_vcp_dma_active = false;

    debug_io_vcp_start_locked();
}

void DebugIO_OnUartError(UART_HandleTypeDef *huart)
{
    if ((huart == NULL) || (huart != s_vcp_uart))
    {
        return;
    }

    /*
     * Keep the queued bytes. A later vcp_write()/vcp_printf() call will retry
     * the same unsent block after the HAL UART state becomes ready again.
     */
    s_vcp_dma_length = 0U;
    s_vcp_dma_active = false;
}

#if DEBUG_IO_OWN_HAL_UART_CALLBACKS

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    DebugIO_OnUartTxComplete(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    DebugIO_OnUartError(huart);
}

#endif /* DEBUG_IO_OWN_HAL_UART_CALLBACKS */
