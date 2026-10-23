/**
  ******************************************************************************
  * @file    app_instrument_screen.h
  * @brief   信号分离装置结果页与波形详情页入口（UTF-8）。
  ******************************************************************************
  */

#ifndef APP_INSTRUMENT_SCREEN_H
#define APP_INSTRUMENT_SCREEN_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *AppInstrumentScreen_Create(void);
lv_obj_t *AppInstrumentDetailScreen_Create(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_INSTRUMENT_SCREEN_H */
