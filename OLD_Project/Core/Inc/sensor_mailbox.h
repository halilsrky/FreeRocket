/**
 * @file sensor_mailbox.h
 * @brief Mailbox-based inter-task communication for sensor data
 * @date 2025-12-26
 * @author halilsrky
 *
 * This module implements a mailbox pattern using FreeRTOS queues with depth=1
 * and xQueueOverwrite() to ensure consumers always get the latest sensor data.
 *
 * Architecture:
 * - Producers (bmiTask, bmeTask, gnssTask, adcTask) write to mailboxes
 * - Consumers (sensorFusionTask, telemetryTask, dataLoggerTask) peek from mailboxes
 * - No data is lost, consumers always see the most recent sample
 */

#ifndef INC_SENSOR_MAILBOX_H_
#define INC_SENSOR_MAILBOX_H_

#include <stdint.h>
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "queue.h"

/* ============================================================================
 *                           SAMPLE STRUCTURES
 * ============================================================================ */

/**
 * @brief BMI088 IMU sample structure
 * @note Contains accelerometer, gyroscope data and orientation
 */
typedef struct {
    uint32_t timestamp;              // HAL_GetTick() value
    float accel_x, accel_y, accel_z; // m/s²
    float gyro_x, gyro_y, gyro_z;    // rad/s (or deg/s depending on config)
    float theta;                      // Angle (degrees)
    float delta_time;                 // Time since last measurement (s)
    float roll, pitch, yaw;           // Orientation angles (degrees)
} bmi_sample_t;

/**
 * @brief BME280 environmental sensor sample structure
 * @note Contains temperature, pressure, humidity and calculated altitude
 */
typedef struct {
    uint32_t timestamp;
    float temperature;               // °C
    float pressure;                  // hPa
    float humidity;                  // %RH
    float altitude;                  // m (barometric, base-corrected)
} bme_sample_t;

/**
 * @brief L86 GNSS sample structure
 * @note Contains position, speed and fix information
 */
typedef struct {
    uint32_t timestamp;
    float latitude;                  // Degrees (decimal)
    float longitude;                 // Degrees (decimal)
    float altitude;                  // m (GPS altitude)
    float speed;                     // m/s (speed over ground)
    uint8_t fix_status;              // 0=invalid, 1=GPS, 2=DGPS
    uint8_t satellites;              // Number of satellites in use
    char is_valid;                   // 'A'=valid, 'V'=invalid
} gnss_sample_t;

/**
 * @brief ADC sample structure
 * @note Contains magnetometer, voltage and current readings
 */
typedef struct {
    uint32_t timestamp;
    float magnetometer;              // Gauss (HMC1021)
    float voltage;                   // V (battery voltage)
    float current;                   // mA (current draw)
} adc_sample_t;

/**
 * @brief Sensor fusion output structure
 * @note Contains filtered altitude, velocity, orientation and flight state
 */
typedef struct {
    uint32_t timestamp;
    float filtered_altitude;         // m (Kalman filtered)
    float velocity;                  // m/s (vertical velocity)
    float roll, pitch, yaw;          // degrees
    float theta;                     // angle from vertical
    uint8_t flight_phase;            // Flight phase enum
    uint8_t apogee_detected;         // bool
    uint8_t drogue_deployed;         // bool
    uint8_t main_deployed;           // bool
    uint8_t accel_failure;           // Accelerometer failure flag
} fused_sample_t;

/* ============================================================================
 *                           MAILBOX HANDLES
 * ============================================================================ */

extern osMessageQueueId_t bmiMailboxHandle;
extern osMessageQueueId_t bmeMailboxHandle;
extern osMessageQueueId_t gnssMailboxHandle;
extern osMessageQueueId_t adcMailboxHandle;
extern osMessageQueueId_t fusedMailboxHandle;

/* ============================================================================
 *                           INITIALIZATION
 * ============================================================================ */

/**
 * @brief Initialize all sensor mailboxes
 * @note Must be called after osKernelInitialize() but before osKernelStart()
 */
void mailbox_init(void);

/* ============================================================================
 *                        PRODUCER FUNCTIONS
 * ============================================================================
 * These functions use xQueueOverwrite() to always write the latest data.
 * Non-blocking, never fails (always overwrites existing data).
 */

/**
 * @brief Send BMI088 sample to mailbox (overwrites existing)
 * @param sample Pointer to sample data
 */
void mailbox_send_bmi(const bmi_sample_t *sample);

/**
 * @brief Send BME280 sample to mailbox (overwrites existing)
 * @param sample Pointer to sample data
 */
void mailbox_send_bme(const bme_sample_t *sample);

/**
 * @brief Send GNSS sample to mailbox (overwrites existing)
 * @param sample Pointer to sample data
 */
void mailbox_send_gnss(const gnss_sample_t *sample);

/**
 * @brief Send ADC sample to mailbox (overwrites existing)
 * @param sample Pointer to sample data
 */
void mailbox_send_adc(const adc_sample_t *sample);

/**
 * @brief Send fused sensor output to mailbox (overwrites existing)
 * @param sample Pointer to sample data
 */
void mailbox_send_fused(const fused_sample_t *sample);

/* ============================================================================
 *                        CONSUMER FUNCTIONS
 * ============================================================================
 * These functions use xQueuePeek() to read the latest data without removing it.
 * Non-blocking, returns pdTRUE if data is available, pdFALSE if mailbox is empty.
 */

/**
 * @brief Peek BMI088 sample from mailbox (non-blocking)
 * @param sample Pointer to store sample data
 * @return pdTRUE if data available, pdFALSE if mailbox empty
 */
BaseType_t mailbox_peek_bmi(bmi_sample_t *sample);

/**
 * @brief Peek BME280 sample from mailbox (non-blocking)
 * @param sample Pointer to store sample data
 * @return pdTRUE if data available, pdFALSE if mailbox empty
 */
BaseType_t mailbox_peek_bme(bme_sample_t *sample);

/**
 * @brief Peek GNSS sample from mailbox (non-blocking)
 * @param sample Pointer to store sample data
 * @return pdTRUE if data available, pdFALSE if mailbox empty
 */
BaseType_t mailbox_peek_gnss(gnss_sample_t *sample);

/**
 * @brief Peek ADC sample from mailbox (non-blocking)
 * @param sample Pointer to store sample data
 * @return pdTRUE if data available, pdFALSE if mailbox empty
 */
BaseType_t mailbox_peek_adc(adc_sample_t *sample);

/**
 * @brief Peek fused sensor output from mailbox (non-blocking)
 * @param sample Pointer to store sample data
 * @return pdTRUE if data available, pdFALSE if mailbox empty
 */
BaseType_t mailbox_peek_fused(fused_sample_t *sample);

/* ============================================================================
 *                        UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief Check if BMI mailbox has valid data
 * @return 1 if data available, 0 if empty
 */
uint8_t mailbox_is_bmi_valid(void);

/**
 * @brief Check if BME mailbox has valid data
 * @return 1 if data available, 0 if empty
 */
uint8_t mailbox_is_bme_valid(void);

/**
 * @brief Check if GNSS mailbox has valid data
 * @return 1 if data available, 0 if empty
 */
uint8_t mailbox_is_gnss_valid(void);

/**
 * @brief Check if ADC mailbox has valid data
 * @return 1 if data available, 0 if empty
 */
uint8_t mailbox_is_adc_valid(void);

/**
 * @brief Check if fused mailbox has valid data
 * @return 1 if data available, 0 if empty
 */
uint8_t mailbox_is_fused_valid(void);

#endif /* INC_SENSOR_MAILBOX_H_ */
