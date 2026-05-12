/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sensor_mailbox.h"
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

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* Definitions for bmiTask */
osThreadId_t bmiTaskHandle;
const osThreadAttr_t bmiTask_attributes = {
  .name = "bmiTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityRealtime,  // En yüksek öncelik
};

/* Definitions for bmeTask */
osThreadId_t bmeTaskHandle;
const osThreadAttr_t bmeTask_attributes = {
  .name = "bmeTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

/* Definitions for sensorFusionTask */
osThreadId_t sensorFusionTaskHandle;
const osThreadAttr_t sensorFusionTask_attributes = {
  .name = "sensorFusionTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

/* Definitions for adcTask */
osThreadId_t adcTaskHandle;
const osThreadAttr_t adcTask_attributes = {
  .name = "adcTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for gnssTask */
osThreadId_t gnssTaskHandle;
const osThreadAttr_t gnssTask_attributes = {
  .name = "gnssTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for telemetryTask */
osThreadId_t telemetryTaskHandle;
const osThreadAttr_t telemetryTask_attributes = {
  .name = "telemetryTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for dataLoggerTask */
osThreadId_t dataLoggerTaskHandle;
const osThreadAttr_t dataLoggerTask_attributes = {
  .name = "dataLoggerTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

/* Definitions for testModeTask */
osThreadId_t testModeTaskHandle;
const osThreadAttr_t testModeTask_attributes = {
  .name = "testModeTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

// FreeRTOS Task Functions
void StartBMITask(void *argument);
void StartBMETask(void *argument);
void StartSensorFusionTask(void *argument);
void StartADCTask(void *argument);
void StartGNSSTask(void *argument);
void StartTelemetryTask(void *argument);
void StartDataLoggerTask(void *argument);
void StartTestModeTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  mailbox_init();
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */


  /* creation of bmiTask */

  bmiTaskHandle = osThreadNew(StartBMITask, NULL, &bmiTask_attributes);

  /* creation of bmeTask */
  bmeTaskHandle = osThreadNew(StartBMETask, NULL, &bmeTask_attributes);

  /* creation of sensorFusionTask */
  sensorFusionTaskHandle = osThreadNew(StartSensorFusionTask, NULL, &sensorFusionTask_attributes);

  /* creation of adcTask */
  adcTaskHandle = osThreadNew(StartADCTask, NULL, &adcTask_attributes);

  /* creation of gnssTask */
  gnssTaskHandle = osThreadNew(StartGNSSTask, NULL, &gnssTask_attributes);

  /* creation of telemetryTask */
  telemetryTaskHandle = osThreadNew(StartTelemetryTask, NULL, &telemetryTask_attributes);

  /* creation of dataLoggerTask */
  dataLoggerTaskHandle = osThreadNew(StartDataLoggerTask, NULL, &dataLoggerTask_attributes);

  // Create test mode task for handling SIT/SUT modes via UART2
  testModeTaskHandle = osThreadNew(StartTestModeTask, NULL, &testModeTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
	  SEGGER_SYSVIEW_Start();

  for(;;)
  {
    osDelay(100000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

