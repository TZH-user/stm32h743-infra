/**
  ******************************************************************************
  * @file    board_flash_w25q64.c
  * @brief   W25Q64 单线命令模式基础实现（UTF-8）。
  ******************************************************************************
  */

#include "board_flash_w25q64.h"

#include "board_flash_qspi_port.h"

#include <stddef.h>
#include <string.h>

#define W25Q64_CMD_WRITE_ENABLE       0x06U
#define W25Q64_CMD_READ_STATUS_REG1   0x05U
#define W25Q64_CMD_PAGE_PROGRAM       0x02U
#define W25Q64_CMD_READ_DATA          0x03U
#define W25Q64_CMD_SECTOR_ERASE_4K    0x20U
#define W25Q64_CMD_BLOCK_ERASE_64K    0xD8U
#define W25Q64_CMD_READ_JEDEC_ID      0x9FU
#define W25Q64_CMD_ENABLE_RESET       0x66U
#define W25Q64_CMD_RESET_DEVICE       0x99U

#define W25Q64_STATUS_BUSY            0x01U
#define W25Q64_STATUS_WEL             0x02U

#define W25Q64_CMD_TIMEOUT_MS         1000U
#define W25Q64_WRITE_TIMEOUT_MS       1000U
#define W25Q64_ERASE_4K_TIMEOUT_MS    1000U
#define W25Q64_ERASE_64K_TIMEOUT_MS   5000U

static void board_flash_w25q64_prepare_command(QSPI_CommandTypeDef *command)
{
    memset(command, 0, sizeof(*command));
    command->InstructionMode = QSPI_INSTRUCTION_1_LINE;
    command->AddressSize = QSPI_ADDRESS_24_BITS;
    command->AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    command->DdrMode = QSPI_DDR_MODE_DISABLE;
    command->DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    command->SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
}

static BoardFlashW25q64Result board_flash_w25q64_send_simple(
    uint8_t instruction)
{
    QSPI_CommandTypeDef command;

    board_flash_w25q64_prepare_command(&command);
    command.Instruction = instruction;
    command.AddressMode = QSPI_ADDRESS_NONE;
    command.DataMode = QSPI_DATA_NONE;

    return (BoardFlashQspiPort_Command(&command, W25Q64_CMD_TIMEOUT_MS)
            == HAL_OK)
        ? BOARD_FLASH_W25Q64_OK : BOARD_FLASH_W25Q64_ERROR_QSPI;
}

static BoardFlashW25q64Result board_flash_w25q64_wait_status(
    uint8_t match,
    uint8_t mask,
    uint32_t timeout_ms)
{
    QSPI_CommandTypeDef command;
    QSPI_AutoPollingTypeDef config;

    board_flash_w25q64_prepare_command(&command);
    command.Instruction = W25Q64_CMD_READ_STATUS_REG1;
    command.AddressMode = QSPI_ADDRESS_NONE;
    command.DataMode = QSPI_DATA_1_LINE;
    command.NbData = 1U;

    memset(&config, 0, sizeof(config));
    config.Match = match;
    config.Mask = mask;
    config.MatchMode = QSPI_MATCH_MODE_AND;
    config.StatusBytesSize = 1U;
    config.Interval = 0x10U;
    config.AutomaticStop = QSPI_AUTOMATIC_STOP_ENABLE;

    return (BoardFlashQspiPort_AutoPolling(&command, &config, timeout_ms)
            == HAL_OK)
        ? BOARD_FLASH_W25Q64_OK : BOARD_FLASH_W25Q64_ERROR_BUSY;
}

static BoardFlashW25q64Result board_flash_w25q64_wait_ready(
    uint32_t timeout_ms)
{
    return board_flash_w25q64_wait_status(0U,
                                         W25Q64_STATUS_BUSY,
                                         timeout_ms);
}

static BoardFlashW25q64Result board_flash_w25q64_write_enable(void)
{
    BoardFlashW25q64Result result = board_flash_w25q64_send_simple(
        W25Q64_CMD_WRITE_ENABLE);
    if (result != BOARD_FLASH_W25Q64_OK)
    {
        return result;
    }

    return board_flash_w25q64_wait_status(W25Q64_STATUS_WEL,
                                         W25Q64_STATUS_WEL,
                                         W25Q64_CMD_TIMEOUT_MS);
}

static BoardFlashW25q64Result board_flash_w25q64_reset(void)
{
    BoardFlashW25q64Result result = board_flash_w25q64_send_simple(
        W25Q64_CMD_ENABLE_RESET);
    if (result != BOARD_FLASH_W25Q64_OK)
    {
        return result;
    }

    result = board_flash_w25q64_send_simple(W25Q64_CMD_RESET_DEVICE);
    if (result != BOARD_FLASH_W25Q64_OK)
    {
        return result;
    }
    return board_flash_w25q64_wait_ready(W25Q64_CMD_TIMEOUT_MS);
}

static BoardFlashW25q64Result board_flash_w25q64_erase(
    uint32_t address,
    uint8_t instruction,
    uint32_t timeout_ms)
{
    QSPI_CommandTypeDef command;
    BoardFlashW25q64Result result = board_flash_w25q64_write_enable();
    if (result != BOARD_FLASH_W25Q64_OK)
    {
        return result;
    }

    board_flash_w25q64_prepare_command(&command);
    command.Instruction = instruction;
    command.AddressMode = QSPI_ADDRESS_1_LINE;
    command.Address = address;
    command.DataMode = QSPI_DATA_NONE;

    if (BoardFlashQspiPort_Command(&command, W25Q64_CMD_TIMEOUT_MS)
        != HAL_OK)
    {
        return BOARD_FLASH_W25Q64_ERROR_QSPI;
    }
    return board_flash_w25q64_wait_ready(timeout_ms);
}

static BoardFlashW25q64Result board_flash_w25q64_write_page(
    uint32_t address,
    const uint8_t *buffer,
    uint32_t length)
{
    QSPI_CommandTypeDef command;

    if ((buffer == NULL) || (length == 0U)
        || (length > BOARD_FLASH_W25Q64_PAGE_SIZE)
        || ((address % BOARD_FLASH_W25Q64_PAGE_SIZE) + length
            > BOARD_FLASH_W25Q64_PAGE_SIZE))
    {
        return BOARD_FLASH_W25Q64_ERROR_ARG;
    }

    BoardFlashW25q64Result result = board_flash_w25q64_write_enable();
    if (result != BOARD_FLASH_W25Q64_OK)
    {
        return result;
    }

    board_flash_w25q64_prepare_command(&command);
    command.Instruction = W25Q64_CMD_PAGE_PROGRAM;
    command.AddressMode = QSPI_ADDRESS_1_LINE;
    command.Address = address;
    command.DataMode = QSPI_DATA_1_LINE;
    command.NbData = length;

    if ((BoardFlashQspiPort_Command(&command, W25Q64_CMD_TIMEOUT_MS)
         != HAL_OK)
        || (BoardFlashQspiPort_Transmit((uint8_t *)buffer,
                                       W25Q64_WRITE_TIMEOUT_MS) != HAL_OK))
    {
        return BOARD_FLASH_W25Q64_ERROR_QSPI;
    }
    return board_flash_w25q64_wait_ready(W25Q64_WRITE_TIMEOUT_MS);
}

BoardFlashW25q64Result BoardFlashW25q64_Init(void)
{
    uint32_t jedec_id = 0U;

    if (BoardFlashQspiPort_Init() != HAL_OK)
    {
        return BOARD_FLASH_W25Q64_ERROR_INIT;
    }

    BoardFlashW25q64Result result = board_flash_w25q64_reset();
    if (result != BOARD_FLASH_W25Q64_OK)
    {
        return result;
    }

    result = BoardFlashW25q64_ReadId(&jedec_id);
    if (result != BOARD_FLASH_W25Q64_OK)
    {
        return result;
    }
    return (jedec_id == BOARD_FLASH_W25Q64_JEDEC_ID)
        ? BOARD_FLASH_W25Q64_OK : BOARD_FLASH_W25Q64_ERROR_ID;
}

BoardFlashW25q64Result BoardFlashW25q64_ReadId(uint32_t *jedec_id)
{
    QSPI_CommandTypeDef command;
    uint8_t bytes[3];

    if (jedec_id == NULL)
    {
        return BOARD_FLASH_W25Q64_ERROR_ARG;
    }

    board_flash_w25q64_prepare_command(&command);
    command.Instruction = W25Q64_CMD_READ_JEDEC_ID;
    command.AddressMode = QSPI_ADDRESS_NONE;
    command.DataMode = QSPI_DATA_1_LINE;
    command.NbData = sizeof(bytes);

    if ((BoardFlashQspiPort_Command(&command, W25Q64_CMD_TIMEOUT_MS)
         != HAL_OK)
        || (BoardFlashQspiPort_Receive(bytes, W25Q64_CMD_TIMEOUT_MS)
            != HAL_OK))
    {
        return BOARD_FLASH_W25Q64_ERROR_QSPI;
    }

    *jedec_id = ((uint32_t)bytes[0] << 16U)
        | ((uint32_t)bytes[1] << 8U)
        | (uint32_t)bytes[2];
    return BOARD_FLASH_W25Q64_OK;
}

BoardFlashW25q64Result BoardFlashW25q64_Read(uint32_t address,
                                            uint8_t *buffer,
                                            uint32_t length)
{
    QSPI_CommandTypeDef command;

    if (length == 0U)
    {
        return BOARD_FLASH_W25Q64_OK;
    }
    if (buffer == NULL)
    {
        return BOARD_FLASH_W25Q64_ERROR_ARG;
    }

    board_flash_w25q64_prepare_command(&command);
    command.Instruction = W25Q64_CMD_READ_DATA;
    command.AddressMode = QSPI_ADDRESS_1_LINE;
    command.Address = address;
    command.DataMode = QSPI_DATA_1_LINE;
    command.NbData = length;

    if ((BoardFlashQspiPort_Command(&command, W25Q64_CMD_TIMEOUT_MS)
         != HAL_OK)
        || (BoardFlashQspiPort_Receive(buffer, W25Q64_CMD_TIMEOUT_MS)
            != HAL_OK))
    {
        return BOARD_FLASH_W25Q64_ERROR_QSPI;
    }
    return BOARD_FLASH_W25Q64_OK;
}

BoardFlashW25q64Result BoardFlashW25q64_Write(uint32_t address,
                                             const uint8_t *buffer,
                                             uint32_t length)
{
    if (length == 0U)
    {
        return BOARD_FLASH_W25Q64_OK;
    }
    if (buffer == NULL)
    {
        return BOARD_FLASH_W25Q64_ERROR_ARG;
    }

    uint32_t current_address = address;
    uint32_t remaining = length;
    const uint8_t *current_buffer = buffer;
    while (remaining > 0U)
    {
        uint32_t page_remaining = BOARD_FLASH_W25Q64_PAGE_SIZE
            - (current_address % BOARD_FLASH_W25Q64_PAGE_SIZE);
        uint32_t write_length = (remaining < page_remaining)
            ? remaining : page_remaining;
        BoardFlashW25q64Result result = board_flash_w25q64_write_page(
            current_address, current_buffer, write_length);
        if (result != BOARD_FLASH_W25Q64_OK)
        {
            return result;
        }

        current_address += write_length;
        current_buffer += write_length;
        remaining -= write_length;
    }
    return BOARD_FLASH_W25Q64_OK;
}

BoardFlashW25q64Result BoardFlashW25q64_Erase4k(uint32_t address)
{
    return board_flash_w25q64_erase(address,
                                    W25Q64_CMD_SECTOR_ERASE_4K,
                                    W25Q64_ERASE_4K_TIMEOUT_MS);
}

BoardFlashW25q64Result BoardFlashW25q64_Erase64k(uint32_t address)
{
    return board_flash_w25q64_erase(address,
                                    W25Q64_CMD_BLOCK_ERASE_64K,
                                    W25Q64_ERASE_64K_TIMEOUT_MS);
}
