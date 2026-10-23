#include "app_serial_control.h"
#include "debug_io.h"

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    DebugIO_OnUartTxComplete(uart);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *uart, uint16_t size)
{
    AppSerialControl_OnRxEvent(uart, size);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    uint32_t rx_line_error_mask = HAL_UART_ERROR_PE
        | HAL_UART_ERROR_NE
        | HAL_UART_ERROR_FE
        | HAL_UART_ERROR_ORE
        | HAL_UART_ERROR_RTO;
    bool tx_dma_error = (uart != NULL)
        && (uart->hdmatx != NULL)
        && (uart->hdmatx->ErrorCode != HAL_DMA_ERROR_NONE);
    bool rx_dma_error = (uart != NULL)
        && (uart->hdmarx != NULL)
        && (uart->hdmarx->ErrorCode != HAL_DMA_ERROR_NONE);
    bool rx_line_error = (uart != NULL)
        && ((uart->ErrorCode & rx_line_error_mask) != 0U);

    if (tx_dma_error)
    {
        DebugIO_OnUartError(uart);
    }
    if (rx_dma_error || rx_line_error)
    {
        AppSerialControl_OnUartError(uart);
    }

    /* Unknown callback sources are routed to both recovery paths. */
    if (!tx_dma_error && !rx_dma_error && !rx_line_error)
    {
        DebugIO_OnUartError(uart);
        AppSerialControl_OnUartError(uart);
    }
}
