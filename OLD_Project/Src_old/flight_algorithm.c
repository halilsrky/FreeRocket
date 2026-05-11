/**
 * @file flight_algorithm.c
 * @brief Core flight algorithm for rocket flight control system
 * @date 2025-07-03
 * @author halilsrky
 *
 * Flight State Machine:
 * =====================
 *
 *  [IDLE] ---(launch detected)---> [BOOST]
 *     |                               |
 *     |                      (burnout detected)
 *     |                               |
 *     |                               v
 *     |                           [COAST]
 *     |                               |
 *     |                      (apogee detected)
 *     |                               |
 *     |                               v
 *     |                          [APOGEE]
 *     |                               |
 *     |                     (drogue deployed)
 *     |                               |
 *     |                               v
 *     |                      [DROGUE_DESCENT]
 *     |                               |
 *     |                   (main altitude reached)
 *     |                               |
 *     |                               v
 *     |                       [MAIN_DESCENT]
 *     |                               |
 *     |                      (landing detected)
 *     |                               |
 *     +----(abort)--->[ABORT]<--------+
 *                               v
 *                           [LANDED]
 */

#include "flight_algorithm.h"
#include "queternion.h"
#include "bme280.h"
#include "main.h"
#include "test_hal_interface.h"
#include <math.h>

// =============================================================================
// External Dependencies
// =============================================================================
extern int is_BME_ok;

// =============================================================================
// State Machine Variables
// =============================================================================
static FlightPhase_t current_phase = PHASE_IDLE;
static FlightPhase_t previous_phase = PHASE_IDLE;

// =============================================================================
// Flight Parameters (Configurable)
// =============================================================================
static float launch_accel_threshold = DEFAULT_LAUNCH_ACCEL_THRESHOLD;
static float launch_velocity_threshold = DEFAULT_LAUNCH_VELOCITY_THRESHOLD;
static float min_arming_altitude = DEFAULT_MIN_ARMING_ALTITUDE;
static float main_chute_altitude = DEFAULT_MAIN_CHUTE_ALTITUDE;
static float max_angle_threshold = DEFAULT_MAX_ANGLE_THRESHOLD;
static float landing_velocity_threshold = DEFAULT_LANDING_VELOCITY;

// =============================================================================
// State Tracking Variables
// =============================================================================
static uint8_t is_armed = 0;              // System armed flag
static uint8_t drogue_deployed = 0;       // Drogue deployment flag
static uint8_t main_deployed = 0;         // Main deployment flag

// =============================================================================
// Counter Variables for Confirmation
// =============================================================================
static uint16_t apogee_counter = 0;       // Apogee detection counter
static uint16_t burnout_counter = 0;      // Burnout detection counter
static uint16_t landing_counter = 0;      // Landing detection counter

// =============================================================================
// Timing Variables
// =============================================================================
static uint32_t flight_start_time = 0;    // Launch time (ms)
static uint32_t phase_start_time = 0;     // Current phase start time (ms)

// =============================================================================
// Previous Values for Change Detection
// =============================================================================
static float prev_velocity = 0.0f;
static float prev_altitude = 0.0f;

// =============================================================================
// Status Variables
// =============================================================================
static uint16_t status_bits = 0;          // Cumulative status bits
static uint8_t durum_verisi = 1;          // Phase indicator (1-8)

// =============================================================================
// Private Function Prototypes
// =============================================================================
static float calculate_total_acceleration(bmi_sample_t* bmi);
static void transition_to_phase(FlightPhase_t new_phase);
static uint8_t check_launch_condition(bmi_sample_t* bmi, fused_sample_t* fusion);
static uint8_t check_burnout_condition(bmi_sample_t* bmi);
static uint8_t check_apogee_condition(fused_sample_t* fusion);
static uint8_t check_angle_threshold(bmi_sample_t* bmi);
static uint8_t check_main_altitude(bme_sample_t* bme);
static uint8_t check_landing_condition(fused_sample_t* fusion);

void deploy_drogue_parachute(void);
void deploy_main_parachute(void);





// =============================================================================
// State Transition Function
// =============================================================================

/**
 * @brief Transition to a new flight phase
 * @param new_phase The phase to transition to
 */
static void transition_to_phase(FlightPhase_t new_phase)
{
    if (current_phase != new_phase) {
        previous_phase = current_phase;
        current_phase = new_phase;
        phase_start_time = hal_get_tick();

        // Update durum_verisi based on new phase
        switch (new_phase) {
            case PHASE_IDLE:           durum_verisi = 1; break;
            case PHASE_BOOST:          durum_verisi = 2; break;
            case PHASE_COAST:          durum_verisi = 3; break;
            case PHASE_APOGEE:         durum_verisi = 4; break;
            case PHASE_DROGUE_DESCENT: durum_verisi = 5; break;
            case PHASE_MAIN_DESCENT:   durum_verisi = 6; break;
            case PHASE_LANDED:         durum_verisi = 7; break;
            case PHASE_ABORT:          durum_verisi = 8; break;
        }
    }
}

// =============================================================================
// Initialization Functions
// =============================================================================

/**
 * @brief Initialize the flight algorithm
 */
void flight_algorithm_init(void)
{
    flight_algorithm_reset();
}

/**
 * @brief Reset flight algorithm to initial state
 */
void flight_algorithm_reset(void)
{
    // Reset state machine
    current_phase = PHASE_IDLE;
    previous_phase = PHASE_IDLE;

    // Reset flags
    is_armed = 0;
    drogue_deployed = 0;
    main_deployed = 0;

    // Reset counters
    apogee_counter = 0;
    burnout_counter = 0;
    landing_counter = 0;

    // Reset timing
    flight_start_time = 0;
    phase_start_time = 0;

    // Reset previous values
    prev_velocity = 0.0f;
    prev_altitude = 0.0f;

    // Reset status
    status_bits = 0;
    durum_verisi = 1;
}

// =============================================================================
// Condition Check Functions
// =============================================================================

/**
 * @brief Check if launch conditions are met
 * @return 1 if launch detected, 0 otherwise
 */
static uint8_t check_launch_condition(bmi_sample_t* bmi, fused_sample_t* fusion)
{
    float total_accel = calculate_total_acceleration(bmi);

    // Primary: Acceleration threshold
    if (total_accel > launch_accel_threshold) {
        return 1;
    }

    // Secondary: Velocity threshold (backup)
    if (fusion->velocity > launch_velocity_threshold) {
        return 1;
    }

    return 0;
}

/**
 * @brief Check if burnout conditions are met
 * @return 1 if burnout detected, 0 otherwise
 */
static uint8_t check_burnout_condition(bmi_sample_t* bmi)
{
    uint32_t elapsed = hal_get_tick() - flight_start_time;

    // Timeout-based burnout detection
    if (elapsed > DEFAULT_BURNOUT_TIMEOUT_MS) {
        return 1;
    }

    // Acceleration-based burnout detection (negative X-axis acceleration)
    if (bmi->accel_z < 0.0f) {
        burnout_counter++;
        if (burnout_counter >= DEFAULT_BURNOUT_CONFIRM_COUNT) {
            return 1;
        }
    } else {
        // Reset counter if condition not met
        if (burnout_counter > 0) {
            burnout_counter--;
        }
    }

    return 0;
}

/**
 * @brief Check if apogee conditions are met
 * @return 1 if apogee detected, 0 otherwise
 */
static uint8_t check_apogee_condition(fused_sample_t* fusion)
{
    // Apogee: velocity negative and decreasing
    if (fusion->velocity < 0.0f && fusion->velocity < prev_velocity) {
        apogee_counter++;
        if (apogee_counter >= DEFAULT_APOGEE_CONFIRM_COUNT) {
            return 1;
        }
    } else {
        // Reset counter with hysteresis
        if (apogee_counter > 0) {
            apogee_counter--;
        }
    }

    return 0;
}

/**
 * @brief Check if angle threshold is exceeded
 * @return 1 if angle exceeds threshold, 0 otherwise
 */
static uint8_t check_angle_threshold(bmi_sample_t* bmi)
{
    return (fabs(bmi->theta) > max_angle_threshold) ? 1 : 0;
}

/**
 * @brief Check if main parachute deployment altitude is reached
 * @return 1 if altitude below threshold, 0 otherwise
 */
static uint8_t check_main_altitude(bme_sample_t* bme)
{
    return (bme->altitude < main_chute_altitude) ? 1 : 0;
}

/**
 * @brief Check if landing conditions are met
 * @return 1 if landing detected, 0 otherwise
 */
static uint8_t check_landing_condition(fused_sample_t* fusion)
{
    // Low velocity indicates landing
    if (fabs(fusion->velocity) < landing_velocity_threshold) {
        landing_counter++;
        if (landing_counter >= DEFAULT_LANDING_CONFIRM_COUNT) {
            return 1;
        }
    } else {
        landing_counter = 0;
    }

    return 0;
}

// =============================================================================
// Main Update Function
// =============================================================================

/**
 * @brief Update flight algorithm with sensor data
 * @param bme Pointer to BME280 data
 * @param bmi Pointer to BMI088 data
 * @param sensor_fusion Pointer to fused sensor data
 */
void flight_algorithm_update(bme_sample_t* bme, bmi_sample_t* bmi, fused_sample_t* sensor_fusion)
{
    // =========================================================================
    // State Machine
    // =========================================================================
    switch (current_phase) {

        // ---------------------------------------------------------------------
        // IDLE: Pre-launch, waiting on pad
        // Entry: System start or reset
        // Exit: Launch detected (high acceleration or velocity)
        // ---------------------------------------------------------------------
        case PHASE_IDLE:
            if (check_launch_condition(bmi, sensor_fusion)) {
                flight_start_time = hal_get_tick();
                status_bits |= BIT_LAUNCH_DETECTED;
                transition_to_phase(PHASE_BOOST);
            }
            break;

        // ---------------------------------------------------------------------
        // BOOST: Powered flight, motor burning
        // Entry: Launch detected
        // Exit: Burnout detected (timeout or acceleration reversal)
        // ---------------------------------------------------------------------
        case PHASE_BOOST:
            if (check_burnout_condition(bmi)) {
                status_bits |= BIT_BURNOUT_DETECTED;
                transition_to_phase(PHASE_COAST);
            }
            break;

        // ---------------------------------------------------------------------
        // COAST: Unpowered ascent after burnout
        // Entry: Burnout detected
        // Exit: Apogee detected OR angle threshold exceeded
        // ---------------------------------------------------------------------
        case PHASE_COAST:
            // Check and set arming status
            if (!is_armed && bme->altitude > min_arming_altitude) {
                is_armed = 1;
                status_bits |= BIT_MIN_ALTITUDE_ARMED;
            }

            // Only check deployment conditions if armed
            if (is_armed) {
                // Check angle threshold (safety deployment)
                if (check_angle_threshold(bmi)) {
                    status_bits |= BIT_ANGLE_THRESHOLD;
                    status_bits |= BIT_APOGEE_DETECTED;
                    transition_to_phase(PHASE_APOGEE);
                    break;
                }

                // Check apogee (normal deployment)
                if (check_apogee_condition(sensor_fusion)) {
                    status_bits |= BIT_APOGEE_DETECTED;
                    transition_to_phase(PHASE_APOGEE);
                }
            }
            break;

        // ---------------------------------------------------------------------
        // APOGEE: Peak altitude reached, deploy drogue
        // Entry: Apogee detected or angle threshold
        // Exit: Drogue deployed (immediate transition)
        // ---------------------------------------------------------------------
        case PHASE_APOGEE:
            // Deploy drogue parachute
            if (!drogue_deployed) {
                drogue_deployed = 1;
                status_bits |= BIT_DROGUE_DEPLOYED;
                // deploy_drogue_parachute(); // Actual hardware deployment
            }
            transition_to_phase(PHASE_DROGUE_DESCENT);
            break;

        // ---------------------------------------------------------------------
        // DROGUE_DESCENT: Descending with drogue parachute
        // Entry: Drogue deployed
        // Exit: Main deployment altitude reached
        // ---------------------------------------------------------------------
        case PHASE_DROGUE_DESCENT:
            if (check_main_altitude(bme)) {
                status_bits |= BIT_MAIN_ALTITUDE;
                transition_to_phase(PHASE_MAIN_DESCENT);
            }
            break;

        // ---------------------------------------------------------------------
        // MAIN_DESCENT: Descending with main parachute
        // Entry: Main deployment altitude reached
        // Exit: Landing detected
        // ---------------------------------------------------------------------
        case PHASE_MAIN_DESCENT:
            // Deploy main parachute
            if (!main_deployed) {
                main_deployed = 1;
                status_bits |= BIT_MAIN_DEPLOYED;
                // deploy_main_parachute(); // Actual hardware deployment
            }

            // Check for landing
            if (check_landing_condition(sensor_fusion)) {
                status_bits |= BIT_LANDED;
                transition_to_phase(PHASE_LANDED);
            }
            break;

        // ---------------------------------------------------------------------
        // LANDED: Mission complete
        // Entry: Landing detected
        // Exit: None (terminal state) or reset
        // ---------------------------------------------------------------------
        case PHASE_LANDED:
            // Terminal state - no transitions
            // System waits for manual reset or power cycle
            break;

        // ---------------------------------------------------------------------
        // ABORT: Emergency state
        // Entry: Critical error or manual abort
        // Exit: Manual reset required
        // ---------------------------------------------------------------------
        case PHASE_ABORT:
            // Emergency state - all deployments should be triggered
            if (!drogue_deployed) {
                drogue_deployed = 1;
                status_bits |= BIT_DROGUE_DEPLOYED;
            }
            if (!main_deployed) {
                main_deployed = 1;
                status_bits |= BIT_MAIN_DEPLOYED;
            }
            break;
    }

    // =========================================================================
    // Update Previous Values for Next Iteration
    // =========================================================================
    prev_velocity = sensor_fusion->velocity;
    prev_altitude = bme->altitude;
}


// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Calculate total acceleration magnitude
 * @param bmi Pointer to BMI088 data
 * @return Total acceleration in m/s²
 */
static float calculate_total_acceleration(bmi_sample_t* bmi)
{
    return sqrtf((bmi->accel_x * bmi->accel_x) +
                 (bmi->accel_y * bmi->accel_y) +
                 (bmi->accel_z * bmi->accel_z));
}

// =============================================================================
// Getter Functions
// =============================================================================

/**
 * @brief Get the current flight phase
 * @return Current flight phase enum value
 */
FlightPhase_t flight_algorithm_get_phase(void)
{
    return current_phase;
}

/**
 * @brief Get the previous flight phase
 * @return Previous flight phase enum value
 */
FlightPhase_t flight_algorithm_get_previous_phase(void)
{
    return previous_phase;
}

/**
 * @brief Get the current status bits
 * @return 16-bit status flags
 */
uint16_t flight_algorithm_get_status_bits(void)
{
    return status_bits;
}

/**
 * @brief Get flight start time
 * @return Flight start timestamp in milliseconds
 */
uint32_t flight_algorithm_get_start_time(void)
{
    return flight_start_time;
}

/**
 * @brief Check if system is armed
 * @return 1 if armed, 0 otherwise
 */
uint8_t flight_algorithm_is_armed(void)
{
    return is_armed;
}

/**
 * @brief Check if drogue is deployed
 * @return 1 if deployed, 0 otherwise
 */
uint8_t flight_algorithm_is_drogue_deployed(void)
{
    return drogue_deployed;
}

/**
 * @brief Check if main is deployed
 * @return 1 if deployed, 0 otherwise
 */
uint8_t flight_algorithm_is_main_deployed(void)
{
    return main_deployed;
}

// =============================================================================
// Setter Functions
// =============================================================================

/**
 * @brief Set flight parameters
 * @param launch_accel Launch acceleration threshold (m/s²)
 * @param min_altitude Minimum arming altitude (m)
 * @param main_altitude Main parachute deployment altitude (m)
 * @param max_angle Maximum angle threshold (degrees)
 */
void flight_algorithm_set_parameters(float launch_accel,
                                     float min_altitude,
                                     float main_altitude,
                                     float max_angle)
{
    launch_accel_threshold = launch_accel;
    min_arming_altitude = min_altitude;
    main_chute_altitude = main_altitude;
    max_angle_threshold = max_angle;
}

/**
 * @brief Trigger abort mode
 * @note This will deploy all parachutes immediately
 */
void flight_algorithm_abort(void)
{
    if (current_phase != PHASE_IDLE && current_phase != PHASE_LANDED) {
        transition_to_phase(PHASE_ABORT);
    }
}
