/**
  ******************************************************************************
  * @file    app_board_flash.c
  * @brief   带范围检查和 RTOS 互斥的板级 Flash 服务实现（UTF-8）。
  ******************************************************************************
  */

#include "app_board_flash.h"

#include "board_flash_w25q64.h"
#include "cmsis_os2.h"
#include "stm32h7xx.h"

#include <stddef.h>
#include <string.h>

#define APP_BOARD_FLASH_SELF_TEST_LENGTH 320U

static volatile bool s_board_flash_ready = false;
static osMutexId_t s_board_flash_mutex = NULL;
static const osMutexAttr_t s_board_flash_mutex_attributes = {
    .name = "BoardFlashMutex"
};

bool AppBoardFlash_RtosInit(void)
{
    if (s_board_flash_mutex == NULL)
    {
        s_board_flash_mutex = osMutexNew(&s_board_flash_mutex_attributes);
    }
    return s_board_flash_mutex != NULL;
}

static void app_board_flash_lock(void)
{
    if (s_board_flash_mutex != NULL)
    {
        (void)osMutexAcquire(s_board_flash_mutex, osWaitForever);
    }
}

static void app_board_flash_unlock(void)
{
    if (s_board_flash_mutex != NULL)
    {
        (void)osMutexRelease(s_board_flash_mutex);
    }
}

static AppBoardFlashResult app_board_flash_map_result(
    BoardFlashW25q64Result result)
{
    switch (result)
    {
        case BOARD_FLASH_W25Q64_OK:
            return APP_BOARD_FLASH_OK;
        case BOARD_FLASH_W25Q64_ERROR_ARG:
            return APP_BOARD_FLASH_ERROR_ARG;
        case BOARD_FLASH_W25Q64_ERROR_INIT:
            return APP_BOARD_FLASH_ERROR_INIT;
        case BOARD_FLASH_W25Q64_ERROR_ID:
            return APP_BOARD_FLASH_ERROR_ID;
        case BOARD_FLASH_W25Q64_ERROR_BUSY:
            return APP_BOARD_FLASH_ERROR_BUSY;
        case BOARD_FLASH_W25Q64_ERROR_QSPI:
        default:
            return APP_BOARD_FLASH_ERROR_QSPI;
    }
}

static bool app_board_flash_range_valid(uint32_t address, uint32_t length)
{
    if (length == 0U)
    {
        return true;
    }
    return (address < APP_BOARD_FLASH_TOTAL_SIZE)
        && (length <= (APP_BOARD_FLASH_TOTAL_SIZE - address));
}

static AppBoardFlashResult app_board_flash_ensure_ready(void)
{
    return AppBoardFlash_IsReady()
        ? APP_BOARD_FLASH_OK : AppBoardFlash_Init();
}

AppBoardFlashResult AppBoardFlash_Init(void)
{
    if (AppBoardFlash_IsReady())
    {
        return APP_BOARD_FLASH_OK;
    }

    app_board_flash_lock();
    AppBoardFlashResult result = app_board_flash_map_result(
        BoardFlashW25q64_Init());
    s_board_flash_ready = (result == APP_BOARD_FLASH_OK);
    __DMB();
    app_board_flash_unlock();
    return result;
}

bool AppBoardFlash_IsReady(void)
{
    __DMB();
    return s_board_flash_ready;
}

AppBoardFlashResult AppBoardFlash_ReadId(uint32_t *jedec_id)
{
    if (jedec_id == NULL)
    {
        return APP_BOARD_FLASH_ERROR_ARG;
    }

    AppBoardFlashResult result = app_board_flash_ensure_ready();
    if (result != APP_BOARD_FLASH_OK)
    {
        return result;
    }

    app_board_flash_lock();
    result = app_board_flash_map_result(BoardFlashW25q64_ReadId(jedec_id));
    app_board_flash_unlock();
    return result;
}

AppBoardFlashResult AppBoardFlash_Read(uint32_t address,
                                      uint8_t *buffer,
                                      uint32_t length)
{
    if (length == 0U)
    {
        return APP_BOARD_FLASH_OK;
    }
    if (buffer == NULL)
    {
        return APP_BOARD_FLASH_ERROR_ARG;
    }
    if (!app_board_flash_range_valid(address, length))
    {
        return APP_BOARD_FLASH_ERROR_RANGE;
    }

    AppBoardFlashResult result = app_board_flash_ensure_ready();
    if (result != APP_BOARD_FLASH_OK)
    {
        return result;
    }

    app_board_flash_lock();
    result = app_board_flash_map_result(
        BoardFlashW25q64_Read(address, buffer, length));
    app_board_flash_unlock();
    return result;
}

AppBoardFlashResult AppBoardFlash_Write(uint32_t address,
                                       const uint8_t *buffer,
                                       uint32_t length)
{
    if (length == 0U)
    {
        return APP_BOARD_FLASH_OK;
    }
    if (buffer == NULL)
    {
        return APP_BOARD_FLASH_ERROR_ARG;
    }
    if (!app_board_flash_range_valid(address, length))
    {
        return APP_BOARD_FLASH_ERROR_RANGE;
    }

    AppBoardFlashResult result = app_board_flash_ensure_ready();
    if (result != APP_BOARD_FLASH_OK)
    {
        return result;
    }

    app_board_flash_lock();
    result = app_board_flash_map_result(
        BoardFlashW25q64_Write(address, buffer, length));
    app_board_flash_unlock();
    return result;
}

AppBoardFlashResult AppBoardFlash_Erase4k(uint32_t address)
{
    if ((address % APP_BOARD_FLASH_SECTOR_SIZE) != 0U)
    {
        return APP_BOARD_FLASH_ERROR_ALIGN;
    }
    if (!app_board_flash_range_valid(address,
                                     APP_BOARD_FLASH_SECTOR_SIZE))
    {
        return APP_BOARD_FLASH_ERROR_RANGE;
    }

    AppBoardFlashResult result = app_board_flash_ensure_ready();
    if (result != APP_BOARD_FLASH_OK)
    {
        return result;
    }

    app_board_flash_lock();
    result = app_board_flash_map_result(BoardFlashW25q64_Erase4k(address));
    app_board_flash_unlock();
    return result;
}

AppBoardFlashResult AppBoardFlash_Erase64k(uint32_t address)
{
    if ((address % APP_BOARD_FLASH_BLOCK64_SIZE) != 0U)
    {
        return APP_BOARD_FLASH_ERROR_ALIGN;
    }
    if (!app_board_flash_range_valid(address,
                                     APP_BOARD_FLASH_BLOCK64_SIZE))
    {
        return APP_BOARD_FLASH_ERROR_RANGE;
    }

    AppBoardFlashResult result = app_board_flash_ensure_ready();
    if (result != APP_BOARD_FLASH_OK)
    {
        return result;
    }

    app_board_flash_lock();
    result = app_board_flash_map_result(BoardFlashW25q64_Erase64k(address));
    app_board_flash_unlock();
    return result;
}

AppBoardFlashResult AppBoardFlash_SelfTest(
    uint32_t scratch_address,
    AppBoardFlashTestResult *result)
{
    static uint8_t transmit_buffer[APP_BOARD_FLASH_SELF_TEST_LENGTH];
    static uint8_t receive_buffer[APP_BOARD_FLASH_SELF_TEST_LENGTH];

    if (result == NULL)
    {
        return APP_BOARD_FLASH_ERROR_ARG;
    }
    memset(result, 0, sizeof(*result));

    AppBoardFlashResult operation = AppBoardFlash_ReadId(&result->jedec_id);
    if (operation != APP_BOARD_FLASH_OK)
    {
        result->status = operation;
        return operation;
    }
    if (result->jedec_id != APP_BOARD_FLASH_EXPECTED_JEDEC_ID)
    {
        result->status = APP_BOARD_FLASH_ERROR_ID;
        return result->status;
    }

    operation = AppBoardFlash_Erase4k(scratch_address);
    if (operation != APP_BOARD_FLASH_OK)
    {
        result->status = operation;
        return operation;
    }

    memset(receive_buffer, 0, sizeof(receive_buffer));
    operation = AppBoardFlash_Read(scratch_address,
                                   receive_buffer,
                                   sizeof(receive_buffer));
    if (operation != APP_BOARD_FLASH_OK)
    {
        result->status = operation;
        return operation;
    }
    for (uint32_t index = 0U; index < sizeof(receive_buffer); ++index)
    {
        if (receive_buffer[index] != 0xFFU)
        {
            result->status = APP_BOARD_FLASH_ERROR_VERIFY;
            result->fail_address = scratch_address + index;
            result->expected = 0xFFU;
            result->actual = receive_buffer[index];
            return result->status;
        }
        transmit_buffer[index] = (uint8_t)((index * 37U) + 0x5AU);
    }

    operation = AppBoardFlash_Write(scratch_address,
                                    transmit_buffer,
                                    sizeof(transmit_buffer));
    if (operation != APP_BOARD_FLASH_OK)
    {
        result->status = operation;
        return operation;
    }

    memset(receive_buffer, 0, sizeof(receive_buffer));
    operation = AppBoardFlash_Read(scratch_address,
                                   receive_buffer,
                                   sizeof(receive_buffer));
    if (operation != APP_BOARD_FLASH_OK)
    {
        result->status = operation;
        return operation;
    }
    for (uint32_t index = 0U; index < sizeof(transmit_buffer); ++index)
    {
        if (receive_buffer[index] != transmit_buffer[index])
        {
            result->status = APP_BOARD_FLASH_ERROR_VERIFY;
            result->fail_address = scratch_address + index;
            result->expected = transmit_buffer[index];
            result->actual = receive_buffer[index];
            return result->status;
        }
    }

    result->status = APP_BOARD_FLASH_OK;
    return APP_BOARD_FLASH_OK;
}
