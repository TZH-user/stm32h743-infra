/**
  ******************************************************************************
  * @file    app_board_flash.h
  * @brief   带范围检查和 RTOS 互斥的板级 Flash 服务（UTF-8）。
  ******************************************************************************
  */

#ifndef APP_BOARDFLASH_APP_BOARD_FLASH_H_
#define APP_BOARDFLASH_APP_BOARD_FLASH_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_BOARD_FLASH_TOTAL_SIZE        (8UL * 1024UL * 1024UL)
#define APP_BOARD_FLASH_SECTOR_SIZE       4096UL
#define APP_BOARD_FLASH_BLOCK64_SIZE      (64UL * 1024UL)
#define APP_BOARD_FLASH_PAGE_SIZE         256UL
#define APP_BOARD_FLASH_EXPECTED_JEDEC_ID 0x00EF4017UL

typedef enum
{
    APP_BOARD_FLASH_OK = 0,
    APP_BOARD_FLASH_ERROR_ARG,
    APP_BOARD_FLASH_ERROR_RANGE,
    APP_BOARD_FLASH_ERROR_ALIGN,
    APP_BOARD_FLASH_ERROR_INIT,
    APP_BOARD_FLASH_ERROR_ID,
    APP_BOARD_FLASH_ERROR_BUSY,
    APP_BOARD_FLASH_ERROR_QSPI,
    APP_BOARD_FLASH_ERROR_VERIFY
} AppBoardFlashResult;

typedef struct
{
    AppBoardFlashResult status;
    uint32_t jedec_id;
    uint32_t fail_address;
    uint8_t expected;
    uint8_t actual;
} AppBoardFlashTestResult;

bool AppBoardFlash_RtosInit(void);
AppBoardFlashResult AppBoardFlash_Init(void);
bool AppBoardFlash_IsReady(void);
AppBoardFlashResult AppBoardFlash_ReadId(uint32_t *jedec_id);
AppBoardFlashResult AppBoardFlash_Read(uint32_t address,
                                      uint8_t *buffer,
                                      uint32_t length);
AppBoardFlashResult AppBoardFlash_Write(uint32_t address,
                                       const uint8_t *buffer,
                                       uint32_t length);
AppBoardFlashResult AppBoardFlash_Erase4k(uint32_t address);
AppBoardFlashResult AppBoardFlash_Erase64k(uint32_t address);
AppBoardFlashResult AppBoardFlash_SelfTest(
    uint32_t scratch_address,
    AppBoardFlashTestResult *result);

#ifdef __cplusplus
}
#endif

#endif /* APP_BOARDFLASH_APP_BOARD_FLASH_H_ */
