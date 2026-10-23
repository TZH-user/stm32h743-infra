/**
  ******************************************************************************
  * @file    board_flash_w25q64.h
  * @brief   W25Q64 基础命令层（UTF-8）。
  ******************************************************************************
  */

#ifndef APP_BOARDFLASH_BOARD_FLASH_W25Q64_H_
#define APP_BOARDFLASH_BOARD_FLASH_W25Q64_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_FLASH_W25Q64_TOTAL_SIZE   (8UL * 1024UL * 1024UL)
#define BOARD_FLASH_W25Q64_SECTOR_SIZE  4096UL
#define BOARD_FLASH_W25Q64_BLOCK64_SIZE (64UL * 1024UL)
#define BOARD_FLASH_W25Q64_PAGE_SIZE    256UL
#define BOARD_FLASH_W25Q64_JEDEC_ID     0x00EF4017UL

typedef enum
{
    BOARD_FLASH_W25Q64_OK = 0,
    BOARD_FLASH_W25Q64_ERROR_ARG,
    BOARD_FLASH_W25Q64_ERROR_INIT,
    BOARD_FLASH_W25Q64_ERROR_ID,
    BOARD_FLASH_W25Q64_ERROR_BUSY,
    BOARD_FLASH_W25Q64_ERROR_QSPI
} BoardFlashW25q64Result;

BoardFlashW25q64Result BoardFlashW25q64_Init(void);
BoardFlashW25q64Result BoardFlashW25q64_ReadId(uint32_t *jedec_id);
BoardFlashW25q64Result BoardFlashW25q64_Read(uint32_t address,
                                            uint8_t *buffer,
                                            uint32_t length);
BoardFlashW25q64Result BoardFlashW25q64_Write(uint32_t address,
                                             const uint8_t *buffer,
                                             uint32_t length);
BoardFlashW25q64Result BoardFlashW25q64_Erase4k(uint32_t address);
BoardFlashW25q64Result BoardFlashW25q64_Erase64k(uint32_t address);

#ifdef __cplusplus
}
#endif

#endif /* APP_BOARDFLASH_BOARD_FLASH_W25Q64_H_ */
