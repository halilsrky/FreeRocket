/**
 * @file flight_algorithm.h
 * @brief Core flight algorithm for rocket flight control system
 * @date 2025-07-03
 * @author halilsrky
 */

#ifndef INC_FLIGHT_ALGORITHM_H_
#define INC_FLIGHT_ALGORITHM_H_

#include "bme280.h"
#include "bmi088.h"
#include "sensor_fusion.h"
#include <stdint.h>
#include "sensor_mailbox.h"

// =============================================================================
// Flight Status Bits (Cumulative - once set, remain set)
// =============================================================================
#define BIT_LAUNCH_DETECTED       0x0001  // Bit 0: Launch detected
#define BIT_BURNOUT_DETECTED      0x0002  // Bit 1: Motor burnout detected
#define BIT_MIN_ALTITUDE_ARMED    0x0004  // Bit 2: Minimum arming altitude reached
#define BIT_ANGLE_THRESHOLD       0x0008  // Bit 3: Angle threshold exceeded
#define BIT_APOGEE_DETECTED       0x0010  // Bit 4: Apogee detected (descent started)
#define BIT_DROGUE_DEPLOYED       0x0020  // Bit 5: Drogue parachute deployed
#define BIT_MAIN_ALTITUDE         0x0040  // Bit 6: Main deployment altitude reached
#define BIT_MAIN_DEPLOYED         0x0080  // Bit 7: Main parachute deployed
#define BIT_LANDED                0x0100  // Bit 8: Rocket landed
#define BIT_SENSOR_ERROR          0x0200  // Bit 9: Sensor error detected

// =============================================================================
// Flight Phases (State Machine States)
// =============================================================================
typedef enum {
    PHASE_IDLE = 0,         // Pre-launch: On pad, waiting for launch
    PHASE_BOOST,            // Powered flight: Motor burning
    PHASE_COAST,            // Unpowered ascent: After burnout, ascending
    PHASE_APOGEE,           // Apogee: Peak altitude reached
    PHASE_DROGUE_DESCENT,   // Drogue descent: Descending with drogue chute
    PHASE_MAIN_DESCENT,     // Main descent: Descending with main chute
    PHASE_LANDED,           // Landed: On ground, mission complete
    PHASE_ABORT             // Abort: Emergency state
} FlightPhase_t;

// =============================================================================
// Configuration Defaults
// =============================================================================
#define DEFAULT_LAUNCH_ACCEL_THRESHOLD    45.0f   // m/s² - Launch detection threshold
#define DEFAULT_LAUNCH_VELOCITY_THRESHOLD 50.0f   // m/s - Backup launch detection
#define DEFAULT_MIN_ARMING_ALTITUDE       1800.0f // m - Minimum altitude for arming
#define DEFAULT_MAIN_CHUTE_ALTITUDE       500.0f  // m - Main parachute deployment altitude
#define DEFAULT_MAX_ANGLE_THRESHOLD       70.0f   // degrees - Angle for drogue deployment
#define DEFAULT_LANDING_VELOCITY          2.0f    // m/s - Landing detection threshold
#define DEFAULT_BURNOUT_TIMEOUT_MS        8000    // ms - Maximum boost phase duration
#define DEFAULT_APOGEE_CONFIRM_COUNT      9       // samples - Apogee confirmation count
#define DEFAULT_BURNOUT_CONFIRM_COUNT     5      // samples - Burnout confirmation count
#define DEFAULT_LANDING_CONFIRM_COUNT     50      // samples - Landing confirmation count

/**
 * @brief Initialize the flight algorithm
 */
void flight_algorithm_init(void);

/**
 * @brief Reset flight algorithm to initial state
 */
void flight_algorithm_reset(void);

/**
 * @brief Update flight algorithm with sensor data
 * @param bme Pointer to BME280 data structure
 * @param bmi Pointer to BMI088 data structure
 * @param sensor_fusion Pointer to sensor fusion output
 * @return Current status bits
 */
void flight_algorithm_update(bme_sample_t* BME, bmi_sample_t* BMI, fused_sample_t* sensor);
uint32_t flight_algorithm_get_start_time(void);
/**
 * @brief Get the current flight phase
 * @return Current flight phase
 */
FlightPhase_t flight_algorithm_get_phase(void);

/**
 * @brief Get the current status bits
 * @return Status bits as a 16-bit value
 */
uint16_t flight_algorithm_get_status_bits(void);

/**
 * @brief Set flight parameters
 * @param launch_accel_threshold Launch detection acceleration threshold (m/s²)
 * @param min_arming_altitude Minimum altitude for arming (m)
 * @param main_chute_altitude Main parachute deployment altitude (m)
 * @param max_angle_threshold Maximum angle threshold for drogue deployment (degrees)
 */
void flight_algorithm_set_parameters(float launch_accel_threshold,
                                     float min_arming_altitude,
                                     float main_chute_altitude,
                                     float max_angle_threshold);

/**
 * @brief Get the previous flight phase
 * @return Previous flight phase
 */
FlightPhase_t flight_algorithm_get_previous_phase(void);

/**
 * @brief Check if system is armed
 * @return 1 if armed, 0 otherwise
 */
uint8_t flight_algorithm_is_armed(void);

/**
 * @brief Check if drogue parachute is deployed
 * @return 1 if deployed, 0 otherwise
 */
uint8_t flight_algorithm_is_drogue_deployed(void);

/**
 * @brief Check if main parachute is deployed
 * @return 1 if deployed, 0 otherwise
 */
uint8_t flight_algorithm_is_main_deployed(void);

/**
 * @brief Trigger emergency abort
 * @note This will deploy all parachutes immediately
 */
void flight_algorithm_abort(void);

#endif /* INC_FLIGHT_ALGORITHM_H_ */
