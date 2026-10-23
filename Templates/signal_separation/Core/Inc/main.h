/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define AD9959_SDIO2_Pin GPIO_PIN_5
#define AD9959_SDIO2_GPIO_Port GPIOI
#define AD9226_D6_Pin GPIO_PIN_5
#define AD9226_D6_GPIO_Port GPIOB
#define AD9959_UPDATE_Pin GPIO_PIN_10
#define AD9959_UPDATE_GPIO_Port GPIOC
#define AD9226_D5_Pin GPIO_PIN_6
#define AD9226_D5_GPIO_Port GPIOB
#define AD9226_D7_Pin GPIO_PIN_4
#define AD9226_D7_GPIO_Port GPIOB
#define AD9959_SP2_Pin GPIO_PIN_11
#define AD9959_SP2_GPIO_Port GPIOC
#define AD9959_SP3_Pin GPIO_PIN_14
#define AD9959_SP3_GPIO_Port GPIOH
#define AD9226_D4_Pin GPIO_PIN_7
#define AD9226_D4_GPIO_Port GPIOB
#define AD9226_D8_Pin GPIO_PIN_3
#define AD9226_D8_GPIO_Port GPIOB
#define AD9959_SP1_Pin GPIO_PIN_12
#define AD9959_SP1_GPIO_Port GPIOC
#define AD9226_D2_Pin GPIO_PIN_9
#define AD9226_D2_GPIO_Port GPIOB
#define AD9226_D3_Pin GPIO_PIN_8
#define AD9226_D3_GPIO_Port GPIOB
#define AD9959_RESET_Pin GPIO_PIN_2
#define AD9959_RESET_GPIO_Port GPIOD
#define AD9959_AUX_PH13_Pin GPIO_PIN_13
#define AD9959_AUX_PH13_GPIO_Port GPIOH
#define AD9959_PDC_Pin GPIO_PIN_8
#define AD9959_PDC_GPIO_Port GPIOC
#define AD9959_SP0_Pin GPIO_PIN_9
#define AD9959_SP0_GPIO_Port GPIOC
#define TOUCH_RST_Pin GPIO_PIN_10
#define TOUCH_RST_GPIO_Port GPIOI
#define TOUCH_INT_Pin GPIO_PIN_11
#define TOUCH_INT_GPIO_Port GPIOI
#define TOUCH_INT_EXTI_IRQn EXTI15_10_IRQn
#define CAL_RELAY_ADC_LOOP_Pin GPIO_PIN_7
#define CAL_RELAY_ADC_LOOP_GPIO_Port GPIOC
#define CAL_RELAY_INPUT_SEL_Pin GPIO_PIN_6
#define CAL_RELAY_INPUT_SEL_GPIO_Port GPIOC
#define TOUCH_SDA_Pin GPIO_PIN_7
#define TOUCH_SDA_GPIO_Port GPIOG
#define TOUCH_SCLK_Pin GPIO_PIN_3
#define TOUCH_SCLK_GPIO_Port GPIOG
#define AD9226_CLK_Pin GPIO_PIN_0
#define AD9226_CLK_GPIO_Port GPIOA
#define AD9226_D1_Pin GPIO_PIN_10
#define AD9226_D1_GPIO_Port GPIOB
#define AD9226_D0_Pin GPIO_PIN_11
#define AD9226_D0_GPIO_Port GPIOB
#define AD9959_SCLK_Pin GPIO_PIN_10
#define AD9959_SCLK_GPIO_Port GPIOH
#define AD9959_SDIO0_Pin GPIO_PIN_11
#define AD9959_SDIO0_GPIO_Port GPIOH
#define AD9226_D9_Pin GPIO_PIN_2
#define AD9226_D9_GPIO_Port GPIOB
#define AD9959_SDIO1_Pin GPIO_PIN_9
#define AD9959_SDIO1_GPIO_Port GPIOH
#define AD9959_CS_Pin GPIO_PIN_12
#define AD9959_CS_GPIO_Port GPIOH
#define AD9226_D10_Pin GPIO_PIN_1
#define AD9226_D10_GPIO_Port GPIOB
#define LCD_BL_Pin GPIO_PIN_6
#define LCD_BL_GPIO_Port GPIOH
#define AD9959_SDIO3_Pin GPIO_PIN_8
#define AD9959_SDIO3_GPIO_Port GPIOH
#define AD9226_D11_Pin GPIO_PIN_0
#define AD9226_D11_GPIO_Port GPIOB
#define AD9959_AUX_PH7_Pin GPIO_PIN_7
#define AD9959_AUX_PH7_GPIO_Port GPIOH

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
