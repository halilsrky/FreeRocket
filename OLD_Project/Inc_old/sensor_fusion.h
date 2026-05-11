/**
 * @file sensor_fusion.h
 * @brief Sensor fusion interface for rocket flight control system
 * @date 2025-07-03
 * @author halilsrky
 */

#ifndef INC_SENSOR_FUSION_H_
#define INC_SENSOR_FUSION_H_

#include "bme280.h"
#include "bmi088.h"
#include <stdint.h>
#include "sensor_mailbox.h"

/**
 * @brief Sensor fusion output structure
 */
typedef struct {
    float filtered_altitude;   // Kalman filtered altitude
    float velocity;
    float angle;              // Theta angle from Mahony filter
    float yaw;                // Yaw angle
    float pitch;              // Pitch angle
    float roll;               // Roll angle
    uint8_t apogeeDetect;
    uint8_t accel_failure;    // İvme sensörü arıza bayrağı
} sensor_fusion_t;

void sensor_fusion_init();
void sensor_fusion_update_kalman(bme_sample_t* BME, bmi_sample_t* BMI, fused_sample_t* sensor);
void sensor_fusion_update_imu_only(bmi_sample_t* BMI, fused_sample_t* sensor);
void sensor_fusion_update_mahony(bmi_sample_t* BMI, fused_sample_t* sensor);

#endif /* INC_SENSOR_FUSION_H_ */
