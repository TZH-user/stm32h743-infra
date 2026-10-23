/**
  ******************************************************************************
  * @file    board_flash_qspi_port.h
  * @brief   W25Q64 对 STM32 HAL QUADSPI 的适配接口（UTF-8）。
  ******************************************************************************
  */

#ifndef APP_BOARDFLASH_BOARD_FLASH_QSPI_PORT_H_
#define APP_BOARDFLASH_BOARD_FLASH_QSPI_PORT_H_

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

HAL_StatusTypeDef BoardFlashQspiPort_Init(void);
QSPI_HandleTypeDef *BoardFlashQspiPort_Handle(void);
HAL_StatusTypeDef BoardFlashQspiPort_Command(QSPI_CommandTypeDef *command,
                                             uint32_t timeout_ms);
HAL_StatusTypeDef BoardFlashQspiPort_Transmit(uint8_t *data,
                                              uint32_t timeout_ms);
HAL_StatusTypeDef BoardFlashQspiPort_Receive(uint8_t *data,
                                             uint32_t timeout_ms);
HAL_StatusTypeDef BoardFlashQspiPort_AutoPolling(
    QSPI_CommandTypeDef *command,
    QSPI_AutoPollingTypeDef *config,
    uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_BOARDFLASH_BOARD_FLASH_QSPI_PORT_H_ */
