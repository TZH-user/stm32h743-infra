/**
  ******************************************************************************
  * @file    board_flash_qspi_port.c
  * @brief   W25Q64 对当前工程 hqspi 句柄的适配实现（UTF-8）。
  ******************************************************************************
  */

#include "board_flash_qspi_port.h"

#include "quadspi.h"

HAL_StatusTypeDef BoardFlashQspiPort_Init(void)
{
    /* main.c 已初始化 QUADSPI；仅在句柄仍为复位态时补做初始化。 */
    if ((hqspi.Instance != QUADSPI)
        || (hqspi.State == HAL_QSPI_STATE_RESET))
    {
        MX_QUADSPI_Init();
    }

    return ((hqspi.Instance == QUADSPI)
            && (hqspi.State == HAL_QSPI_STATE_READY))
        ? HAL_OK : HAL_ERROR;
}

QSPI_HandleTypeDef *BoardFlashQspiPort_Handle(void)
{
    return &hqspi;
}

HAL_StatusTypeDef BoardFlashQspiPort_Command(QSPI_CommandTypeDef *command,
                                             uint32_t timeout_ms)
{
    return HAL_QSPI_Command(BoardFlashQspiPort_Handle(), command, timeout_ms);
}

HAL_StatusTypeDef BoardFlashQspiPort_Transmit(uint8_t *data,
                                              uint32_t timeout_ms)
{
    return HAL_QSPI_Transmit(BoardFlashQspiPort_Handle(), data, timeout_ms);
}

HAL_StatusTypeDef BoardFlashQspiPort_Receive(uint8_t *data,
                                             uint32_t timeout_ms)
{
    return HAL_QSPI_Receive(BoardFlashQspiPort_Handle(), data, timeout_ms);
}

HAL_StatusTypeDef BoardFlashQspiPort_AutoPolling(
    QSPI_CommandTypeDef *command,
    QSPI_AutoPollingTypeDef *config,
    uint32_t timeout_ms)
{
    return HAL_QSPI_AutoPolling(BoardFlashQspiPort_Handle(),
                                command,
                                config,
                                timeout_ms);
}
