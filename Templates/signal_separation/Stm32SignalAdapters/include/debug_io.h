/**
  ******************************************************************************
  * @file    debug_io.h
  * @brief   Optional SWO sink and USART VCP DMA transport interface.
  ******************************************************************************
  */

#ifndef APP_DEBUG_DEBUG_IO_H_
#define APP_DEBUG_DEBUG_IO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Set this macro to 0 if another source file owns HAL_UART_TxCpltCallback()
 * and HAL_UART_ErrorCallback(). In that case, forward those callbacks to
 * DebugIO_OnUartTxComplete() and DebugIO_OnUartError() manually.
 */
#ifndef DEBUG_IO_OWN_HAL_UART_CALLBACKS
#define DEBUG_IO_OWN_HAL_UART_CALLBACKS  1
#endif


/*
 * PB3 is permanently assigned to AD9226_D8 in this project. Keep this at 0.
 * SWD debugging on PA13/PA14 remains available.
 */
#ifndef DEBUG_IO_SWO_AVAILABLE
#define DEBUG_IO_SWO_AVAILABLE            0U
#endif

#ifndef DEBUG_IO_VCP_PRINTF_BUFFER_SIZE
#define DEBUG_IO_VCP_PRINTF_BUFFER_SIZE  256U
#endif

#ifndef DEBUG_IO_VCP_TX_RING_SIZE
#define DEBUG_IO_VCP_TX_RING_SIZE        4096U
#endif

/**
 * @brief Initialize the debug I/O module.
 *
 * Call this once after MX_DMA_Init() and MX_USARTx_UART_Init().
 *
 * In this project PB3 is an AD9226 input, so SWO remains disabled. SWD on
 * PA13/PA14 is unaffected.
 *
 * @param vcp_uart UART connected to the ST-LINK virtual COM port.
 */
void DebugIO_Init(UART_HandleTypeDef *vcp_uart);

/**
 * @brief Enable or mute SWO writes.
 *
 * When DEBUG_IO_SWO_AVAILABLE is 0, enabling is ignored and printf() is a no-op.
 */
void DebugIO_SetSwoEnabled(bool enabled);

/** @brief Return whether SWO writes are currently enabled. */
bool DebugIO_IsSwoEnabled(void);

/**
 * @brief Write raw bytes to the SWO/ITM stimulus port 0.
 * @return Number of bytes submitted, or -1 for an invalid argument.
 */
int DebugIO_SwoWrite(const uint8_t *data, size_t length);

/**
 * @brief Queue raw bytes for DMA transmission through the VCP UART.
 *
 * This function is task-safe on a single-core Cortex-M target. Do not call it
 * from an interrupt service routine.
 *
 * @return Number of queued bytes, 0 for zero length, -1 for an invalid
 *         argument or uninitialized UART, or -2 if the ring buffer is full.
 */
int vcp_write(const uint8_t *data, uint16_t length);

/**
 * @brief Format and queue text for DMA transmission through the VCP UART.
 *
 * The formatted text is copied into the ring buffer before this function
 * returns, so DMA never references a temporary stack buffer.
 *
 * @return Number of queued bytes, a negative formatting error, -1 for an
 *         invalid argument or uninitialized UART, or -2 if the ring is full.
 */
#if defined(__GNUC__)
int vcp_printf(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
#else
int vcp_printf(const char *format, ...);
#endif

/**
 * @brief 返回因VCP环形缓冲区已满而被拒绝入队的累计字节数。
 *
 * 调用方可能在稍后重试，因此该计数不等同于最终串口数据丢失量。
 */
uint32_t DebugIO_GetVcpDroppedBytes(void);

/** @brief 返回当前仍在VCP DMA环形缓冲区中排队的字节数。 */
uint32_t DebugIO_GetVcpQueuedBytes(void);

/** @brief 返回本次启动以来VCP DMA环形缓冲区的最大排队字节数。 */
uint32_t DebugIO_GetVcpPeakQueuedBytes(void);

/** @brief Return true while a VCP DMA transfer is active. */
bool DebugIO_IsVcpBusy(void);

/** @brief Return true when the VCP DMA ring contains no queued bytes. */
bool DebugIO_IsVcpIdle(void);

/**
 * @brief Forward a UART transmit-complete callback to this module.
 */
void DebugIO_OnUartTxComplete(UART_HandleTypeDef *huart);

/**
 * @brief Forward a UART error callback to this module.
 */
void DebugIO_OnUartError(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* APP_DEBUG_DEBUG_IO_H_ */
