/**
 * @file bmi088_conversions.h
 * @brief BMI088 raw data to physical units conversion functions
 * 
 * These pure math functions are extracted from bmi088.c to enable:
 * 1. Direct unit testing of conversion formulas
 * 2. Reuse across different contexts
 * 3. Clear separation of concerns
 * 
 * CRITICAL: This file is part of production code.
 *           The tested code flies. The flying code is tested.
 * 
 * Usage:
 *   Production: #include "bmi088_conversions.h" in bmi088.c
 *   Testing:    #include "../../Core/Inc/bmi088_conversions.h" in tests
 */

#ifndef INC_BMI088_CONVERSIONS_H_
#define INC_BMI088_CONVERSIONS_H_

#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#ifndef DEG_TO_RAD
#define DEG_TO_RAD  0.01745329251994329576923690768489f  /* PI / 180 */
#endif

#ifndef RAD_TO_DEG
#define RAD_TO_DEG  57.295779513082320876798154814105f   /* 180 / PI */
#endif

#ifndef GRAVITY_MS2
#define GRAVITY_MS2 9.81f
#endif

/* ============================================================================
 * ACCELEROMETER CONVERSIONS
 * ============================================================================ */

/**
 * @brief Convert raw 16-bit accelerometer value to m/s^2
 * 
 * BMI088 Accelerometer formula:
 * Range setting maps to: 3G=0, 6G=1, 12G=2, 24G=3
 * Full scale = 1.5 * 2^(range+1) = 3, 6, 12, 24 G
 * 
 * @param raw_value  Raw 16-bit signed value from sensor
 * @param acc_range  Accelerometer range setting (0-3)
 * @param offset_mg  Offset in milli-g (calibration)
 * @return Acceleration in m/s^2
 */
static inline float bmi088_accel_raw_to_ms2(int16_t raw_value, uint8_t acc_range, float offset_mg)
{
    /* 
     * Formula from datasheet:
     * accel_mg = (raw / 32768) * full_scale_mg
     * full_scale_mg = 1500 * 2^(range+1) = 3000, 6000, 12000, 24000 mG
     * 
     * Simplified: (raw / 32768) * 1000 * 1.5 * 2^(range+1) - offset
     * Then convert mG to m/s^2: * 9.81 / 1000
     */
    float full_scale_multiplier = 1.5f * powf(2.0f, (float)(acc_range + 1));
    float accel_mg = ((float)raw_value / 32768.0f) * 1000.0f * full_scale_multiplier;
    float accel_corrected_mg = accel_mg - offset_mg;
    return accel_corrected_mg * GRAVITY_MS2 / 1000.0f;
}

/**
 * @brief Optimized version without pow() for embedded use
 */
static inline float bmi088_accel_raw_to_ms2_fast(int16_t raw_value, uint8_t acc_range, float offset_mg)
{
    /* Lookup table for full scale in mG: 3000, 6000, 12000, 24000 */
    static const float full_scale_mg[4] = { 3000.0f, 6000.0f, 12000.0f, 24000.0f };
    
    float accel_mg = ((float)raw_value / 32768.0f) * full_scale_mg[acc_range & 0x03];
    float accel_corrected_mg = accel_mg - offset_mg;
    return accel_corrected_mg * GRAVITY_MS2 / 1000.0f;
}

/**
 * @brief Convert raw bytes to signed 16-bit value
 */
static inline int16_t bmi088_bytes_to_int16(uint8_t lsb, uint8_t msb)
{
    return (int16_t)((msb << 8) | lsb);
}

/* ============================================================================
 * GYROSCOPE CONVERSIONS
 * ============================================================================ */

/**
 * @brief Convert raw 16-bit gyroscope value to rad/s
 * 
 * BMI088 Gyroscope formula:
 * Range setting: 0=2000°/s, 1=1000°/s, 2=500°/s, 3=250°/s, 4=125°/s
 * Full scale = 2000 >> range
 * 
 * @param raw_value   Raw 16-bit signed value from sensor
 * @param gyro_range  Gyroscope range setting (0-4)
 * @param offset_dps  Offset in degrees per second (calibration)
 * @return Angular velocity in rad/s
 */
static inline float bmi088_gyro_raw_to_rads(int16_t raw_value, uint8_t gyro_range, float offset_dps)
{
    /*
     * Formula from datasheet:
     * gyro_dps = (raw / 32767) * full_scale_dps
     * full_scale_dps = 2000 >> range = 2000, 1000, 500, 250, 125
     */
    float full_scale_dps = (float)(2000 >> gyro_range);
    float gyro_dps = ((float)raw_value / 32767.0f) * full_scale_dps;
    float gyro_corrected_dps = gyro_dps - offset_dps;
    return gyro_corrected_dps * DEG_TO_RAD;
}

/**
 * @brief Convert raw 16-bit gyroscope value to degrees per second
 * 
 * @param raw_value   Raw 16-bit signed value from sensor
 * @param gyro_range  Gyroscope range setting (0-4)
 * @param offset_dps  Offset in degrees per second (calibration)
 * @return Angular velocity in degrees/s
 */
static inline float bmi088_gyro_raw_to_dps(int16_t raw_value, uint8_t gyro_range, float offset_dps)
{
    float full_scale_dps = (float)(2000 >> gyro_range);
    float gyro_dps = ((float)raw_value / 32767.0f) * full_scale_dps;
    return gyro_dps - offset_dps;
}

/* ============================================================================
 * TEMPERATURE CONVERSION
 * ============================================================================ */

/**
 * @brief Convert raw temperature bytes to Celsius
 * 
 * BMI088 temperature sensor has 11-bit resolution
 * Sensitivity: 125 LSB/°C, offset at 23°C = 0
 * 
 * @param msb Upper byte (MSB)
 * @param lsb Lower byte (LSB, only upper 3 bits used)
 * @return Temperature in Celsius
 */
static inline float bmi088_temp_raw_to_celsius(uint8_t msb, uint8_t lsb)
{
    int16_t raw_temp = ((int16_t)msb << 3) | ((lsb >> 5) & 0x07);
    
    /* Sign extension for 11-bit value */
    if (raw_temp > 1023) {
        raw_temp -= 2048;
    }
    
    return ((float)raw_temp * 0.125f) + 23.0f;
}

/* ============================================================================
 * VALIDATION HELPERS
 * ============================================================================ */

/**
 * @brief Check if acceleration value is within expected range
 * 
 * @param accel_ms2   Acceleration in m/s^2
 * @param max_g       Maximum expected G force
 * @return 1 if valid, 0 if out of range
 */
static inline int bmi088_accel_is_valid(float accel_ms2, float max_g)
{
    float max_ms2 = max_g * GRAVITY_MS2;
    return (accel_ms2 >= -max_ms2 && accel_ms2 <= max_ms2) ? 1 : 0;
}

/**
 * @brief Check if angular velocity is within expected range
 * 
 * @param gyro_rads   Angular velocity in rad/s
 * @param max_dps     Maximum expected degrees per second
 * @return 1 if valid, 0 if out of range
 */
static inline int bmi088_gyro_is_valid(float gyro_rads, float max_dps)
{
    float max_rads = max_dps * DEG_TO_RAD;
    return (gyro_rads >= -max_rads && gyro_rads <= max_rads) ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* INC_BMI088_CONVERSIONS_H_ */
