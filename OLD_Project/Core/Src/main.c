/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body for SkyLord2 Flight Computer
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

/**
  ******************************************************************************
  * SKYRTOS Flight Computer - Main Application
  *
  * This application implements a comprehensive flight computer system for rocket
  * telemetry and control, featuring:
  *
  * SENSORS:
  * - BME280: Environmental sensor (temperature, humidity, pressure)
  * - BMI088: 6-axis IMU (3-axis accelerometer + 3-axis gyroscope)
  * - HMC1021: Single-axis magnetometer for magnetic field measurement
  * - L86 GPS/GNSS: Position and navigation data
  *
  * COMMUNICATION:
  * - E22 LoRa module: Long-range wireless telemetry transmission
  * - UART: Debug and telemetry output
  *
  * PROCESSING:
  * - Real-time sensor fusion using quaternion-based algorithms
  * - Kalman filtering for noise reduction
  * - Flight state estimation and control
  *
  * TIMING:
  * - 100ms periodic operations for data acquisition
  * - 1s periodic operations for magnetometer calibration
  * - Continuous sensor monitoring and data fusion
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "dma.h"
#include "fatfs.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* Standard library includes */
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Sensor drivers */
#include "bme280.h"          // Environmental sensor (temp, humidity, pressure)
#include "bmi088.h"          // 6-axis IMU (accelerometer + gyroscope)
#include "l86_gnss.h"        // GPS/GNSS module

/* Communication modules */
#include "e22_lib.h"         // LoRa wireless communication
#include "packet.h"          // Telemetry packet handling
#include "data_logger.h"     // SD card data logging

/* Algorithm and processing modules */
#include "queternion.h"      // Quaternion mathematics
#include "sensor_fusion.h"   // Sensor fusion algorithms
#include "flight_algorithm.h" // Flight state detection and control
#include "sensor_mailbox.h"  // Mailbox-based inter-task communication
#include "uart_handler.h"    // UART communication and test mode handling
#include "test_modes.h"      // Test mode handlers (SIT, SUT)

/*MATHEMATICAL CONSTANTS*/
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define IMU_I2C_HNDLR	hi2c1 //put your I2C handler

#define CRNT_COEF					(float)0.0008056641	// Current coefficient (A)

#define ACCEL_DMA_FLAG		(0x0001U)
#define GYRO_DMA_FLAG		(0x0002U)
#define BME_DATA_READY_FLAG	(0x0004U)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */


/*==================== SENSOR STRUCTURES ====================*/
// Environmental sensor (BME280) - temperature, humidity, pressure
static BME_280_t BME280_sensor;         // BME280 barometric pressure sensor
bmi088_struct_t BMI_sensor;             // BMI088 IMU sensor (accelerometer + gyroscope)
sensor_fusion_t sensor_output;          // Sensor fusion output data
BME_parameters_t bme_params;            // BME280 calibration parameters
gps_data_t gnss_data;                   // L86 GNSS receiver data
static e22_conf_struct_t lora_1;        // LoRa configuration structure

extern osThreadId_t bmiTaskHandle;
extern osThreadId_t sensorFusionTaskHandle;
/*==================== COMMUNICATION BUFFERS ====================*/
// UART communication buffers
static char uart_buffer[128];
extern unsigned char normal_paket[50];  // Normal mode telemetry packet
extern unsigned char sd_paket[64];      // SD card data packet

uint8_t usart2_rx_buffer[36];  // UART4 receive buffer


volatile uint8_t usart2_tx_busy = 0;    // UART2 transmission busy flag
volatile uint8_t usart2_packet_ready = 0;  // UART2 packet received flag
volatile uint16_t usart2_packet_size = 0;  // Size of received UART2 packet

/*==================== SENSOR STATUS VARIABLES ====================*/
// Sensor initialization and health status
int is_BME_ok = 0;
int is_BMI_ok = 0;
int bmi_status_ok = 0;

/*==================== ADC BUFFERS AND MAGNETOMETER DATA ====================*/
// General ADC buffers
uint16_t adc1_buffer[1];
uint16_t adc2_buffer[1];
float current_mA = 0.0f;
float voltage_V = 0.0f;

// HMC1021 magnetometer (single axis) data
volatile uint16_t hmc1021_adc_buffer[1];
float hmc1021_voltage = 0.0f;
float hmc1021_gauss = 0.0f;

/*==================== INTERRUPT FLAGS ====================*/
// BMI088 data ready interrupt flags
volatile uint8_t bmi088_accel_data_ready = 0;  // EXTI4 - Accelerometer data ready
volatile uint8_t bmi088_gyro_data_ready = 0;   // EXTI3 - Gyroscope data ready

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
static void bme280_begin(void);
uint8_t bmi_imu_init(void);
void read_ADC(void);
void lora_init(void);
void lora_send_packet_dma(uint8_t *data, uint16_t size);
void uart2_send_packet_dma(uint8_t *data, uint16_t size);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_USART6_UART_Init();
  MX_UART4_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  MX_SPI3_Init();
  /* USER CODE BEGIN 2 */

	/*==================== TIMER AND INTERRUPT CONFIGURATION ====================*/
	// Configure external interrupt priorities for sensor data ready signals
	// Bu öncelikler FreeRTOS için uygun seviyede ayarlanmıştır (>= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY)
	DWT->CTRL |= (1 << 0);
	SEGGER_SYSVIEW_Conf();

	HAL_NVIC_SetPriority(EXTI3_IRQn, 6, 1);
	HAL_NVIC_SetPriority(EXTI4_IRQn, 6, 1);

	// Enable external interrupts for sensor data ready signals
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);
	HAL_NVIC_EnableIRQ(EXTI3_IRQn);

	/* ==== GPS/GNSS INITIALIZATION ==== */
	// Initialize L86 GPS/GNSS module
	HAL_UART_Transmit(&huart6, (uint8_t*)"$PMTK251,57600*2C\r\n", 19, 100);
	HAL_UART_DeInit(&huart6);
	huart6.Init.BaudRate = 57600;
	HAL_UART_Init(&huart6);
	HAL_DMA_Init(&hdma_usart6_rx);
	L86_GNSS_Init(&huart6);

	/* ==== SENSOR INITIALIZATION ==== */
	// Initialize BME280 sensor (temperature, humidity, pressure)
	bme280_begin();
	HAL_Delay(1000);
	bme280_config();
	bme280_update();

	// Initialize BMI088 IMU (accelerometer and gyroscope)
	bmi_imu_init();
	bmi088_config(&BMI_sensor);
	get_offset(&BMI_sensor);

	/*==================== SENSOR FUSION INITIALIZATION ====================*/
	// Initialize quaternion-based sensor fusion
	getInitialQuaternion();
	sensor_fusion_init(&BME280_sensor);

	/* ==== LORA COMMUNICATION SETUP ==== */
	e22_config_mode(&lora_1);
	HAL_Delay(20);
	lora_init();
	HAL_Delay(20);
	e22_transmit_mode(&lora_1);

	/* ==== DATA LOGGER INITIALIZATION ==== */
	//data_logger_init();
	BME280_sensor.altitude = BME280_sensor.altitude - BME280_sensor.base_altitude;

	// Startup beep to indicate system ready
	HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, SET);
	HAL_Delay(300);
	HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, RESET);

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // Main loop is now empty - all work is done in FreeRTOS tasks
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief Initialize BMI088 IMU sensor
 * @return Initialization status
 * @note Configures both accelerometer and gyroscope with optimal settings
 */
uint8_t bmi_imu_init(void)
{
  // Accelerometer configuration
  BMI_sensor.device_config.acc_bandwith = ACC_BWP_OSR4;
  BMI_sensor.device_config.acc_outputDateRate = ACC_ODR_100;
  BMI_sensor.device_config.acc_powerMode = ACC_PWR_SAVE_ACTIVE;
  BMI_sensor.device_config.acc_range = ACC_RANGE_24G;

  // Gyroscope configuration
  BMI_sensor.device_config.gyro_bandWidth = GYRO_BW_12;
  BMI_sensor.device_config.gyro_range = GYRO_RANGE_2000;
  BMI_sensor.device_config.gyro_powerMode = GYRO_LPM_NORMAL;

  // Interrupt and I2C configuration
  BMI_sensor.device_config.acc_IRQ = EXTI3_IRQn;
  BMI_sensor.device_config.gyro_IRQ = EXTI4_IRQn;
  BMI_sensor.device_config.BMI_I2c = &IMU_I2C_HNDLR;
  BMI_sensor.device_config.offsets = NULL; // Offset data stored in backup SRAM

  return bmi088_init(&BMI_sensor);
}

/**
 * @brief GPIO external interrupt callback
 * @param GPIO_Pin The pin that triggered the interrupt
 * @note Handles BMI088 accelerometer and gyroscope data ready interrupts
 * @note Starts corresponding DMA transfers directly from the ISR context
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == GPIO_PIN_4)
  {
    // Gyroscope data ready interrupt (EXTI3)
    bmi088_set_gyro_INT(&BMI_sensor);
    bmi088_start_gyro_dma(&BMI_sensor);
  }
  if(GPIO_Pin == GPIO_PIN_3)
  {
    // Accelerometer data ready interrupt (EXTI4)
    bmi088_set_accel_INT(&BMI_sensor);
    bmi088_start_accel_dma(&BMI_sensor);
  }
}

/**
 * @brief UART transmission complete callback
 * @param huart UART handle
 * @note Clears transmission busy flag
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        usart2_tx_busy = 0;
    }
}

/**
 * @brief Send packet via UART2 using DMA
 * @param data Pointer to data buffer
 * @param size Size of data to send
 * @note Non-blocking transmission using DMA
 */
void uart2_send_packet_dma(uint8_t *data, uint16_t size)
{
    if (!usart2_tx_busy) {
        usart2_tx_busy = 1;
        HAL_UART_Transmit_DMA(&huart2, data, size);
    }
}


/**
 * @brief UART receive event callback
 * @param huart UART handle
 * @param Size Number of bytes received
 * @note Handles UART2 packet reception with DMA
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	if (huart->Instance == USART2) {
		usart2_packet_ready = 1;
		usart2_packet_size = Size;
		// Restart RX DMA for next packet
		HAL_UARTEx_ReceiveToIdle_DMA(&huart2, usart2_rx_buffer, sizeof(usart2_rx_buffer));
		__HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT); // Disable half-transfer interrupt
	}
}


/**
 * @brief I2C Memory read complete callback (DMA)
 * @param hi2c I2C handle
 * @note Handles BMI088 sensor data DMA transfer completion
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    // Debug: increment a counter to see if this callback is called
    static uint32_t i2c_callback_counter = 0;
    i2c_callback_counter++;

    if (hi2c->Instance == I2C1) {
      if (hi2c->Devaddress == ACC_I2C_ADD) {
        // Accelerometer data received (9 bytes: XYZ + sensor time)
        bmi088_accel_dma_complete_callback(&BMI_sensor);
        if(bmiTaskHandle != NULL)
        {
          osThreadFlagsSet(bmiTaskHandle, ACCEL_DMA_FLAG);
        }
      }
      else if (hi2c->Devaddress == GYRO_I2C_ADD) {
        // Gyroscope data received (6 bytes: XYZ)
        bmi088_gyro_dma_complete_callback(&BMI_sensor);
        if(bmiTaskHandle != NULL)
        {
          osThreadFlagsSet(bmiTaskHandle, GYRO_DMA_FLAG);
        }
      }
    }
}

/**
  * @brief  Function implementing the telemetry task.
  * @param  argument: Not used
  * @retval None
  * @note   Uses vTaskDelayUntil for deterministic 10Hz timing
  * @note   Reads latest data from all mailboxes for packet creation
  */
void StartTelemetryTask(void *argument)
{
  /* USER CODE BEGIN StartTelemetryTask */
  fused_sample_t fused_data;
  gnss_sample_t gnss_data_local;
  adc_sample_t adc_data;
  bmi_sample_t bmi_data;
  bme_sample_t bme_data;
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(100);  // 10 Hz
  
  /* Phase offset: 60ms - runs after sensor tasks to get fresh data */
  osDelay(60);
  xLastWakeTime = xTaskGetTickCount();
  
  /* Infinite loop */
  for(;;)
  {
    // Wait for next cycle (deterministic timing)
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
    
        // Get latest data from all mailboxes (non-blocking peek)
    mailbox_peek_fused(&fused_data);
    mailbox_peek_bmi(&bmi_data);
    mailbox_peek_bme(&bme_data);
    mailbox_peek_gnss(&gnss_data_local);
    mailbox_peek_adc(&adc_data);
    
    // Create telemetry packets using updated global structures
    addDataPacketNormal(&bme_data, &bmi_data, &fused_data, &gnss_data_local, adc_data.magnetometer, adc_data.voltage, adc_data.current);

    // Send data via UART2 using DMA
    uart2_send_packet_dma((uint8_t*)normal_paket, 50);

    // Send data via LoRa using DMA
    //lora_send_packet_dma((uint8_t*)normal_paket, 50);
  }
  /* USER CODE END StartTelemetryTask */
}

/**
  * @brief  Function implementing the BMI088 sensor task.
  * @param  argument: Not used
  * @retval None
  * @note   Interrupt-driven: Task waits on thread flags raised by DMA completion
  * @note   Uses system tick for delta_time calculation (more reliable in FreeRTOS)
  * @note   Sends data to bmiMailbox for consumer tasks
  */
void StartBMITask(void *argument)
{
  /* USER CODE BEGIN StartBMITask */
  bmi_sample_t bmi_sample;
  uint32_t notifiedFlags;
  
  /* Infinite loop */
  for(;;)
  {
    notifiedFlags = osThreadFlagsWait(ACCEL_DMA_FLAG | GYRO_DMA_FLAG, osFlagsWaitAny, osWaitForever);
    if(((int32_t)notifiedFlags) < 0)
    {
      continue;
    }

    if((notifiedFlags & ACCEL_DMA_FLAG) != 0U)
    {
      SEGGER_SYSVIEW_Print("ACCEL");
      bmi088_process_accel_data(&BMI_sensor);
    }

    if((notifiedFlags & GYRO_DMA_FLAG) != 0U)
    {
      SEGGER_SYSVIEW_Print("GYRO");
      bmi088_process_gyro_data(&BMI_sensor);
    }
    
    // Prepare sample for mailbox
    bmi_sample.timestamp = HAL_GetTick();
    bmi_sample.accel_x = BMI_sensor.datas.acc_x;
    bmi_sample.accel_y = BMI_sensor.datas.acc_y;
    bmi_sample.accel_z = BMI_sensor.datas.acc_z;
    bmi_sample.gyro_x = BMI_sensor.datas.gyro_x;
    bmi_sample.gyro_y = BMI_sensor.datas.gyro_y;
    bmi_sample.gyro_z = BMI_sensor.datas.gyro_z;
    bmi_sample.theta = BMI_sensor.datas.theta;
    bmi_sample.delta_time = BMI_sensor.datas.delta_time;
    bmi_sample.roll = BMI_sensor.datas.roll;
    bmi_sample.pitch = BMI_sensor.datas.pitch;
    bmi_sample.yaw = BMI_sensor.datas.yaw;
    
    // Send to mailbox (overwrites existing, non-blocking)
    mailbox_send_bmi(&bmi_sample);
  }
  /* USER CODE END StartBMITask */
}

/**
  * @brief  Function implementing the BME280 sensor task.
  * @param  argument: Not used
  * @retval None
  * @note   Uses vTaskDelayUntil for deterministic 10Hz timing
  * @note   Sends data to bmeMailbox for consumer tasks
  */
void StartBMETask(void *argument)
{
  /* USER CODE BEGIN StartBMETask */
  bme_sample_t bme_sample;
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(100);  // 10 Hz
  
  /* Phase offset: 0ms (reference task - runs first to provide data) */
  xLastWakeTime = xTaskGetTickCount();
  
  /* Infinite loop */
  for(;;)
  {
    // Wait for next cycle (deterministic timing)
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
    
    // Update BME280 barometric pressure sensor data
    bme280_update();
    
    // Prepare sample for mailbox
    bme_sample.timestamp = HAL_GetTick();
    bme_sample.temperature = BME280_sensor.temperature;
    bme_sample.pressure = BME280_sensor.pressure;
    bme_sample.humidity = BME280_sensor.humidity;
    bme_sample.altitude = BME280_sensor.altitude;
    
    // Send to mailbox (overwrites existing, non-blocking)
    mailbox_send_bme(&bme_sample);

    // Notify sensor fusion task that new BME data is ready
    if(sensorFusionTaskHandle != NULL)
    {
      osThreadFlagsSet(sensorFusionTaskHandle, BME_DATA_READY_FLAG);
    }
  }
  /* USER CODE END StartBMETask */
}

/**
  * @brief  Function implementing the sensor fusion task.
  * @param  argument: Not used
  * @retval None
  * @note   Reads latest data from mailboxes for sensor fusion
  * @note   Outputs fused data to fusedMailbox
  */
void StartSensorFusionTask(void *argument)
{
  /* USER CODE BEGIN StartSensorFusionTask */
  bmi_sample_t bmi_data;
  bme_sample_t bme_data;
  fused_sample_t fused_output;
  uint32_t flags;
  uint8_t bme_data_valid;
  uint8_t bmi_data_valid;
  
  /* Infinite loop */
  for(;;)
  {
    // Skip sensor fusion when in test mode (SIT or SUT)
    if (uart_handler_get_mode() != MODE_NORMAL) {
      osDelay(100);  // Wait and check again
      continue;
    }
    
    // Wait for BME data ready notification with 200ms timeout
    flags = osThreadFlagsWait(BME_DATA_READY_FLAG, osFlagsWaitAny, 200);
    
    if(flags == osFlagsErrorTimeout)
    {
      // Timeout: BME data not received within 200ms, use BMI only
      bme_data_valid = 0;
    }
    else if(((int32_t)flags) < 0)
    {
      continue; // Other error occurred, retry
    }
    else
    {
      // BME data received successfully
      bme_data_valid = 1;
    }
    
    // Get latest data from mailboxes (non-blocking peek)
    bmi_data_valid = (mailbox_peek_bmi(&bmi_data) == pdTRUE) ? 1 : 0;
    if(bme_data_valid) {
      bme_data_valid = (mailbox_peek_bme(&bme_data) == pdTRUE) ? 1 : 0;
    }

    // Perform sensor fusion - with or without BME data
    if(bme_data_valid && bmi_data_valid)
    {
      // Full sensor fusion with BME280 + BMI088
      sensor_fusion_update_kalman(&bme_data, &bmi_data, &fused_output);
    }
    else if(bmi_data_valid)
    {
      // IMU-only fusion (no barometer update)
      sensor_fusion_update_imu_only(&bmi_data, &fused_output);
    }
    else
    {
      // No valid sensor data, skip this cycle
      continue;
    }

    // Update flight algorithm (launch detection, apogee, etc.)
    flight_algorithm_update(&bme_data, &bmi_data, &fused_output);
    
    // Send to mailbox (overwrites existing, non-blocking)
    mailbox_send_fused(&fused_output);
  }
  /* USER CODE END StartSensorFusionTask */
}

/**
  * @brief  Function implementing the ADC reading task.
  * @param  argument: Not used
  * @retval None
  * @note   Uses vTaskDelayUntil for deterministic 10Hz timing
  * @note   Sends data to adcMailbox for consumer tasks
  */
void StartADCTask(void *argument)
{
  /* USER CODE BEGIN StartADCTask */
  adc_sample_t adc_sample;
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(100);  // 10 Hz
  
  /* Phase offset: 20ms - stagger CPU load across 100ms period */
  osDelay(20);
  xLastWakeTime = xTaskGetTickCount();
  
  /* Infinite loop */
  for(;;)
  {
    // Wait for next cycle (deterministic timing)
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
    
    // Read ADC channels (magnetometer, voltage, current)
    read_ADC();
    
    // Prepare sample for mailbox
    adc_sample.timestamp = HAL_GetTick();
    adc_sample.magnetometer = hmc1021_gauss;
    adc_sample.voltage = voltage_V;
    adc_sample.current = current_mA;
    
    // Send to mailbox (overwrites existing, non-blocking)
    mailbox_send_adc(&adc_sample);
  }
  /* USER CODE END StartADCTask */
}

/**
  * @brief  Function implementing the GNSS task.
  * @param  argument: Not used
  * @retval None
  * @note   Uses vTaskDelayUntil for deterministic 10Hz timing
  * @note   Sends data to gnssMailbox for consumer tasks
  */
void StartGNSSTask(void *argument)
{
  /* USER CODE BEGIN StartGNSSTask */
  gnss_sample_t gnss_sample;
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(100);  // 10 Hz
  
  /* Phase offset: 40ms - stagger CPU load across 100ms period */
  osDelay(40);
  xLastWakeTime = xTaskGetTickCount();
  
  /* Infinite loop */
  for(;;)
  {
    // Wait for next cycle (deterministic timing)
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
    
    // Update GPS/GNSS data
    L86_GNSS_Update(&gnss_data);
    
    // Prepare sample for mailbox
    gnss_sample.timestamp = HAL_GetTick();
    gnss_sample.latitude = gnss_data.latitude;
    gnss_sample.longitude = gnss_data.longitude;
    gnss_sample.altitude = gnss_data.altitude;
    gnss_sample.speed = gnss_data.speed_over_ground;
    gnss_sample.fix_status = gnss_data.fix_status;
    gnss_sample.satellites = gnss_data.satellites_in_use;
    gnss_sample.is_valid = gnss_data.is_valid;
    
    // Send to mailbox (overwrites existing, non-blocking)
    mailbox_send_gnss(&gnss_sample);
  }
  /* USER CODE END StartGNSSTask */
}

/**
  * @brief  Function implementing the data logger task.
  * @param  argument: Not used
  * @retval None
  * @note   Uses vTaskDelayUntil for deterministic 10Hz timing
  * @note   Reads latest data from all mailboxes for SD card logging
  */
void StartDataLoggerTask(void *argument)
{
  /* USER CODE BEGIN StartDataLoggerTask */
  fused_sample_t fused_data;
  bmi_sample_t bmi_data;
  bme_sample_t bme_data;
  gnss_sample_t gnss_data_local;
  adc_sample_t adc_data;
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(100);  // 10 Hz
  
  /* Phase offset: 80ms - runs last after all sensor data is ready */
  osDelay(80);
  xLastWakeTime = xTaskGetTickCount();
  
  /* Infinite loop */
  for(;;)
  {
    // Wait for next cycle (deterministic timing)
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
    
    // Get latest data from all mailboxes (non-blocking peek)
    mailbox_peek_fused(&fused_data);
    mailbox_peek_bmi(&bmi_data);
    mailbox_peek_bme(&bme_data);
    mailbox_peek_gnss(&gnss_data_local);
    mailbox_peek_adc(&adc_data);
    
    // Create SD card data packet using updated global structures
    addDataPacketSD(&bme_data, &bmi_data, &fused_data, &gnss_data_local, adc_data.magnetometer, adc_data.voltage, adc_data.current);

    // Log data to SD card
    log_normal_packet_data(sd_paket, "gorev.bin");
  }
  /* USER CODE END StartDataLoggerTask */
}

/**
  * @brief  Function implementing the test mode task.
  * @param  argument: Not used
  * @retval None
  * @note   Handles SIT and SUT test modes via UART2
  * @note   When in test mode, StartSensorFusionTask pauses
  */
void StartTestModeTask(void *argument)
{
  /* USER CODE BEGIN StartTestModeTask */
  bmi_sample_t bmi_data;
  bme_sample_t bme_data;
  SystemMode_t current_mode;
  TickType_t xLastWakeTime;
  const TickType_t xPeriod = pdMS_TO_TICKS(100);  // 10 Hz
  
  /* Initialize UART handler and test modes */
  uart_handler_init();
  test_modes_init();
  flight_algorithm_init();
  
  /* Start UART2 DMA reception for test commands */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart2, usart2_rx_buffer, sizeof(usart2_rx_buffer));
  __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);
  
  xLastWakeTime = xTaskGetTickCount();
  
  /* Infinite loop */
  for(;;)
  {
    // Wait for next cycle
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
    
    // Process incoming UART packets (commands and SUT data)
    uart_handler_process_packets();
    
    // Handle command processing and system state transitions
    if (uart_handler_command_ready()) {
      uart_handler_clear_command_flag();
      
      // Reset flight algorithm state when switching to NORMAL mode
      if (uart_handler_get_mode() == MODE_NORMAL) {
        flight_algorithm_reset();
      }
    }
    
    // Get current mode
    current_mode = uart_handler_get_mode();
    
    // Handle different test modes
    switch (current_mode) {
      case MODE_NORMAL:
        // Normal mode - other tasks handle sensor processing
        // Nothing to do here
        break;
        
      case MODE_SIT:
        /* SIT Mode: Sensor Interface Test
         * Tests sensor integration with real-time hardware data
         */
        mailbox_peek_bmi(&bmi_data);
        mailbox_peek_bme(&bme_data);
        test_modes_handle_sit(&bme_data, &bmi_data);
        break;
        
      case MODE_SUT:
        /* SUT Mode: System Under Test
         * Validates algorithms with synthetic flight data
         */
        algorithm_update_sut();
        break;
        
      default:
        // Unknown mode - switch to safe (normal) mode
        set_current_mode(MODE_NORMAL);
        break;
    }
  }
  /* USER CODE END StartTestModeTask */
}

/* USER CODE BEGIN 4 */

/**
 * @brief Initialize LoRa E22 module
 * @note Configures LoRa module for telemetry transmission
 */
void lora_init(void)
{
	lora_1.baud_rate 		= 	E22_BAUD_RATE_115200;
	lora_1.parity_bit		=	E22_PARITY_8N1;
	lora_1.air_rate			=	E22_AIR_DATA_RATE_38400;
	lora_1.packet_size		=	E22_PACKET_SIZE_64;
	lora_1.rssi_noise		=	E22_RSSI_NOISE_DISABLE;
	lora_1.power			=	E22_TRANSMITTING_POWER_22;
	lora_1.rssi_enable		=	E22_ENABLE_RSSI_DISABLE;
	lora_1.mode				= 	E22_TRANSMISSION_MODE_TRANSPARENT;
	lora_1.repeater_func	=	E22_REPEATER_FUNC_DISABLE;
	lora_1.lbt				=	E22_LBT_DISABLE;
	lora_1.wor				=	E22_WOR_RECEIVER;
	lora_1.wor_cycle		=	E22_WOR_CYCLE_1000;
	lora_1.channel			=	28;

	e22_init(&lora_1, &huart4);

	HAL_UART_DeInit(&huart4);
	HAL_Delay(20);
	huart4.Init.BaudRate = 115200;
	HAL_Delay(20);
	HAL_UART_Init(&huart4);
}

/**
 * @brief Initialize BME280 environmental sensor
 * @note Configures BME280 with predefined settings for normal operation
 */
void bme280_begin()
{
  BME280_sensor.device_config.bme280_filter = BME280_FILTER_2;
  BME280_sensor.device_config.bme280_mode = BME280_MODE_NORMAL;
  BME280_sensor.device_config.bme280_output_speed = BME280_OS_4;
  BME280_sensor.device_config.bme280_standby_time = BME280_STBY_250;
  bme280_init(&BME280_sensor, &hi2c3);
}

/**
 * @brief Read HMC1021 magnetometer ADC values
 * @note Converts ADC readings to magnetic field strength, voltage and current
 */
void read_ADC()
{
    static uint16_t adc1_raw = 0;  // ADC1 value (Channel 9 - Magnetometer)
    static uint16_t adc2_raw = 0;  // ADC2 value (Channel 10 - Voltage)
    static uint16_t adc3_raw = 0;  // ADC3 value (Channel 11 - Current)

    // Read ADC1 (Magnetometer)
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 5) == HAL_OK) {
        adc1_raw = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    // Read ADC2 (Voltage)
    HAL_ADC_Start(&hadc2);
    if (HAL_ADC_PollForConversion(&hadc2, 5) == HAL_OK) {
        adc2_raw = HAL_ADC_GetValue(&hadc2);
    }
    HAL_ADC_Stop(&hadc2);

    // Read ADC3 (Current)
    HAL_ADC_Start(&hadc3);
    if (HAL_ADC_PollForConversion(&hadc3, 5) == HAL_OK) {
        adc3_raw = HAL_ADC_GetValue(&hadc3);
    }
    HAL_ADC_Stop(&hadc3);

    // Calculate calibrated values
    hmc1021_voltage = (adc1_raw * 3.3f) / 4096.0f;  // 3.3V reference, 12-bit ADC
    voltage_V = (adc2_raw * 13.2f) / 4096.0f;        // Voltage divider calculation
    current_mA = (adc3_raw * CRNT_COEF);             // Current sensor calibration
    hmc1021_gauss = (hmc1021_voltage - 1.65f) / 1.0f; // 1V/Gauss sensitivity

    // LoRa power management based on battery voltage
    if(voltage_V < 7.0){
        e22_sleep_mode(&lora_1);
    }else if(voltage_V >= 7.0){
        e22_transmit_mode(&lora_1);
    }
}


/**
 * @brief Send packet with LORA using DMA
 * @param data Pointer to data buffer
 * @param size Size of data to send
 * @note Non-blocking transmission using DMA
 */
void lora_send_packet_dma(uint8_t *data, uint16_t size)
{
	if (!usart2_tx_busy) {
		usart2_tx_busy = 1;
		HAL_UART_Transmit_DMA(&huart2, data, size);
	}
}

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
