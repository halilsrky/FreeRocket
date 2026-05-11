/*
 * mahony.h — Mahony 6DOF AHRS (accel + gyro fusion, no magnetometer).
 *
 * Ported from OLD_Project/Core/Src/queternion.c (typo intentional in old
 * name). Cleaned up:
 *   - No BMI_sensor struct coupling — floats in/out only
 *   - Quake invSqrt replaced with 1/sqrtf (strict-aliasing UB removed,
 *     F446 has HW FPU so the bit-twiddle was never a win here)
 *   - Thresholds in g (matches bmi088_parse_accel output), not m/s^2
 *   - One canonical roll/pitch/yaw extractor using full-precision pi
 *   - Internal dps -> rad/s; caller passes gyro in BMI088-native dps
 *
 * High-g handling: switches to gyro-only integration when |a| > 3.3 g
 * (boost phase), returns to accel-aided when |a| < 2.7 g (hysteresis).
 * Adaptive Kp/Ki based on accel-vs-estimated-gravity angle error.
 *
 * Typical use:
 *   mahony_reset();
 *   loop, when both accel and gyro samples are fresh:
 *     mahony_update(gx_dps, gy_dps, gz_dps, ax_g, ay_g, az_g, dt_s);
 *     e = mahony_get_euler();
 */
#ifndef MAHONY_H
#define MAHONY_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float w, x, y, z;
} mahony_quat_t;

typedef struct {
    float roll;   /* degrees, rotation around X */
    float pitch;  /* degrees, rotation around Y */
    float yaw;    /* degrees, rotation around Z */
} mahony_euler_t;

/* Reset to identity quaternion, zero integral, exit gyro-only mode. */
void mahony_reset(void);

/* Seed initial quaternion so estimated gravity aligns with measured accel.
 * Run once after IMU settle, before mahony_update. Input in g. Optional —
 * identity-init plus a few seconds of convergence is also fine for bring-up. */
void mahony_set_initial_from_accel(float ax_g, float ay_g, float az_g);

/* One fusion step. gx/gy/gz in dps, ax/ay/az in g, dt in seconds. */
void mahony_update(float gx_dps, float gy_dps, float gz_dps,
                   float ax_g,   float ay_g,   float az_g,
                   float dt);

mahony_quat_t  mahony_get_quat(void);
mahony_euler_t mahony_get_euler(void);

/* True while accel is high-g and filter is integrating gyro alone. */
bool           mahony_is_gyro_only(void);

#endif /* MAHONY_H */
