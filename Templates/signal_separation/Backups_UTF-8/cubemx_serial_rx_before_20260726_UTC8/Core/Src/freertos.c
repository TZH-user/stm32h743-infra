/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_serial_control.h"
#include "app_ui_remote_control.h"
#include "usart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
osThreadId_t SerialControlTaskHandle;
static const osThreadAttr_t SerialControlTask_attributes = {
  .name = "SerialControlTask",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* USER CODE END Variables */
/* Definitions for DisplayTask */
osThreadId_t DisplayTaskHandle;
const osThreadAttr_t DisplayTask_attributes = {
  .name = "DisplayTask",
  .stack_size = 4096 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for TouchTask */
osThreadId_t TouchTaskHandle;
const osThreadAttr_t TouchTask_attributes = {
  .name = "TouchTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for PrintfTask */
osThreadId_t PrintfTaskHandle;
const osThreadAttr_t PrintfTask_attributes = {
  .name = "PrintfTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for MainTask */
osThreadId_t MainTaskHandle;
const osThreadAttr_t MainTask_attributes = {
  .name = "MainTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for PhaseLockTask */
osThreadId_t PhaseLockTaskHandle;
const osThreadAttr_t PhaseLockTask_attributes = {
  .name = "PhaseLockTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for PrintQueue */
osMessageQueueId_t PrintQueueHandle;
const osMessageQueueAttr_t PrintQueue_attributes = {
  .name = "PrintQueue"
};
/* Definitions for AdcFrameReadySem */
osSemaphoreId_t AdcFrameReadySemHandle;
const osSemaphoreAttr_t AdcFrameReadySem_attributes = {
  .name = "AdcFrameReadySem"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDisplayTask(void *argument);
void StartTouchTask(void *argument);
void StartPrintfTask(void *argument);
void StartMainTask(void *argument);
void StartPhaseLockTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  if (!AppSerialControl_Init(&huart1) || !AppUiRemoteControl_Init())
  {
    Error_Handler();
  }
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of AdcFrameReadySem */
  AdcFrameReadySemHandle = osSemaphoreNew(1, 0, &AdcFrameReadySem_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of PrintQueue */
  PrintQueueHandle = osMessageQueueNew (8, 128, &PrintQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of DisplayTask */
  DisplayTaskHandle = osThreadNew(StartDisplayTask, NULL, &DisplayTask_attributes);

  /* creation of TouchTask */
  TouchTaskHandle = osThreadNew(StartTouchTask, NULL, &TouchTask_attributes);

  /* creation of PrintfTask */
  PrintfTaskHandle = osThreadNew(StartPrintfTask, NULL, &PrintfTask_attributes);

  /* creation of MainTask */
  MainTaskHandle = osThreadNew(StartMainTask, NULL, &MainTask_attributes);

  /* creation of PhaseLockTask */
  PhaseLockTaskHandle = osThreadNew(StartPhaseLockTask, NULL, &PhaseLockTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  SerialControlTaskHandle = osThreadNew(
      StartSerialControlTask,
      NULL,
      &SerialControlTask_attributes);
  if (SerialControlTaskHandle == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDisplayTask */
/**
  * @brief  Function implementing the DisplayTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDisplayTask */
__weak void StartDisplayTask(void *argument)
{
  /* USER CODE BEGIN StartDisplayTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDisplayTask */
}

/* USER CODE BEGIN Header_StartTouchTask */
/**
* @brief Function implementing the TouchTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTouchTask */
__weak void StartTouchTask(void *argument)
{
  /* USER CODE BEGIN StartTouchTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTouchTask */
}

/* USER CODE BEGIN Header_StartPrintfTask */
/**
* @brief Function implementing the PrintfTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartPrintfTask */
__weak void StartPrintfTask(void *argument)
{
  /* USER CODE BEGIN StartPrintfTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartPrintfTask */
}

/* USER CODE BEGIN Header_StartMainTask */
/**
* @brief Function implementing the MainTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartMainTask */
__weak void StartMainTask(void *argument)
{
  /* USER CODE BEGIN StartMainTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartMainTask */
}

/* USER CODE BEGIN Header_StartPhaseLockTask */
/**
* @brief Function implementing the PhaseLockTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartPhaseLockTask */
__weak void StartPhaseLockTask(void *argument)
{
  /* USER CODE BEGIN StartPhaseLockTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartPhaseLockTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

