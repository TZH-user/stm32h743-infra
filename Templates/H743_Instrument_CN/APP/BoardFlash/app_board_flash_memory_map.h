/**
  ******************************************************************************
  * @file    app_board_flash_memory_map.h
  * @brief   W25Q64 统一逻辑分区（UTF-8）。
  ******************************************************************************
  */

#ifndef APP_BOARDFLASH_APP_BOARD_FLASH_MEMORY_MAP_H_
#define APP_BOARDFLASH_APP_BOARD_FLASH_MEMORY_MAP_H_

#include <stdint.h>

/* 所有地址均为 W25Q64 芯片内偏移，不是 QUADSPI memory-mapped 地址。 */
#define APP_BOARD_FLASH_MAP_TOTAL_SIZE            0x00800000UL
#define APP_BOARD_FLASH_MAP_SECTOR_SIZE           0x00001000UL

/* 保留参考工程已有壁纸资源区：[0x000000, 0x0BD000)。 */
#define APP_BOARD_FLASH_MAP_WALLPAPER_START       0x00000000UL
#define APP_BOARD_FLASH_MAP_WALLPAPER_END         0x000BD000UL

/* 校准区使用 Flash 后部，但避让参考工程最后两个配置扇区。 */
#define APP_BOARD_FLASH_MAP_CALIBRATION_START     0x00600000UL
#define APP_BOARD_FLASH_MAP_CAL_META_A_START      0x00600000UL
#define APP_BOARD_FLASH_MAP_CAL_META_B_START      0x00601000UL
#define APP_BOARD_FLASH_MAP_CAL_PROFILE_A_START   0x00602000UL
#define APP_BOARD_FLASH_MAP_CAL_PROFILE_B_START   0x00603000UL
#define APP_BOARD_FLASH_MAP_CAL_PROFILE_C_START   0x00604000UL
#define APP_BOARD_FLASH_MAP_CAL_SESSION_START     0x00610000UL
#define APP_BOARD_FLASH_MAP_CALIBRATION_END       0x007FE000UL

/* 参考工程既有区域：倒数第二扇区 OCXO，最后一扇区 Sweep。 */
#define APP_BOARD_FLASH_MAP_OCXO_START            0x007FE000UL
#define APP_BOARD_FLASH_MAP_SWEEP_START           0x007FF000UL

#define APP_BOARD_FLASH_MAP_CAL_META_SECTOR_COUNT 2UL
#define APP_BOARD_FLASH_MAP_CAL_PROFILE_SLOT_COUNT 3UL
#define APP_BOARD_FLASH_MAP_CAL_SESSION_SIZE      \
    (APP_BOARD_FLASH_MAP_CALIBRATION_END \
     - APP_BOARD_FLASH_MAP_CAL_SESSION_START)

#if ((APP_BOARD_FLASH_MAP_CALIBRATION_START % APP_BOARD_FLASH_MAP_SECTOR_SIZE) != 0UL)
#error "Calibration region must be sector aligned"
#endif

#if (APP_BOARD_FLASH_MAP_CALIBRATION_END > APP_BOARD_FLASH_MAP_OCXO_START)
#error "Calibration region overlaps the reserved OCXO sector"
#endif

#if ((APP_BOARD_FLASH_MAP_SWEEP_START + APP_BOARD_FLASH_MAP_SECTOR_SIZE) \
     > APP_BOARD_FLASH_MAP_TOTAL_SIZE)
#error "Reserved tail sectors exceed W25Q64 capacity"
#endif

#endif /* APP_BOARDFLASH_APP_BOARD_FLASH_MEMORY_MAP_H_ */
