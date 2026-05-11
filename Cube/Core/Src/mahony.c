/*
 * mahony.c — 6DOF AHRS implementation. See mahony.h for the API and
 * the diff vs OLD_Project/queternion.c.
 */
#include "mahony.h"
#include <math.h>

#define MAHONY_PI       3.14159265358979f
#define DEG_TO_RAD      (MAHONY_PI / 180.0f)
#define RAD_TO_DEG      (180.0f / MAHONY_PI)

/* Adaptive-gain bounds (OLD code values preserved). */
#define TWO_KP_MAX      4.0f
#define TWO_KP_MIN      0.1f
#define TWO_KI_MAX      0.05f
#define TWO_KI_MIN      0.0f

/* Boost-detect hysteresis (g). Above HIGH -> gyro-only; below LOW -> resume. */
#define ACC_THRESH_HIGH_G   3.3f
#define ACC_THRESH_LOW_G    2.7f

/* Single-pole LPF on accel input. alpha = new sample weight. */
#define ACC_LPF_ALPHA       0.3f

/* Initial Mahony gains (twoKp = 2 * Kp). */
#define TWO_KP_DEFAULT      (2.0f * 2.0f)
#define TWO_KI_DEFAULT      (2.0f * 0.01f)

/* ---- State ----------------------------------------------------------- */

static float s_q[4]   = { 1.0f, 0.0f, 0.0f, 0.0f };  /* w, x, y, z */
static float s_twoKp  = TWO_KP_DEFAULT;
static float s_twoKi  = TWO_KI_DEFAULT;
static float s_iFx    = 0.0f, s_iFy = 0.0f, s_iFz = 0.0f;
static float s_ax_lpf = 0.0f, s_ay_lpf = 0.0f, s_az_lpf = 0.0f;
static bool  s_gyro_only = false;

/* ---- Helpers --------------------------------------------------------- */

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void normalize_q(void) {
    float n2 = s_q[0]*s_q[0] + s_q[1]*s_q[1] + s_q[2]*s_q[2] + s_q[3]*s_q[3];
    if (n2 < 1e-12f) {
        s_q[0] = 1.0f; s_q[1] = 0.0f; s_q[2] = 0.0f; s_q[3] = 0.0f;
        return;
    }
    float inv = 1.0f / sqrtf(n2);
    s_q[0] *= inv; s_q[1] *= inv; s_q[2] *= inv; s_q[3] *= inv;
}

/* ---- Adaptive gain update ------------------------------------------- */

static void update_gains_from_acc_error(float ax, float ay, float az)
{
    /* Estimated gravity direction (column 3 of rotation matrix derived
     * from current quaternion). */
    float gx_est = 2.0f * (s_q[1] * s_q[3] - s_q[0] * s_q[2]);
    float gy_est = 2.0f * (s_q[0] * s_q[1] + s_q[2] * s_q[3]);
    float gz_est = s_q[0]*s_q[0] - s_q[1]*s_q[1] - s_q[2]*s_q[2] + s_q[3]*s_q[3];

    float g_n2 = gx_est*gx_est + gy_est*gy_est + gz_est*gz_est;
    float a_n2 = ax*ax + ay*ay + az*az;
    if (g_n2 < 1e-12f || a_n2 < 1e-12f) return;

    float invG = 1.0f / sqrtf(g_n2);
    float invA = 1.0f / sqrtf(a_n2);
    gx_est *= invG; gy_est *= invG; gz_est *= invG;
    ax *= invA; ay *= invA; az *= invA;

    float dot = clampf(ax * gx_est + ay * gy_est + az * gz_est, -1.0f, 1.0f);
    float err_deg = acosf(dot) * RAD_TO_DEG;

    if (err_deg > 30.0f) {
        s_twoKp = 0.2f; s_twoKi = 0.0f;
    } else if (err_deg > 10.0f) {
        s_twoKp = 2.0f; s_twoKi = 0.01f;
    } else {
        s_twoKp = 8.0f; s_twoKi = 0.05f;
    }
    s_twoKp = clampf(s_twoKp, TWO_KP_MIN, TWO_KP_MAX);
    s_twoKi = clampf(s_twoKi, TWO_KI_MIN, TWO_KI_MAX);
}

/* ---- Filter cores --------------------------------------------------- */

static void integrate_gyro_only(float gx, float gy, float gz, float dt)
{
    float qDot0 = 0.5f * (-s_q[1]*gx - s_q[2]*gy - s_q[3]*gz);
    float qDot1 = 0.5f * ( s_q[0]*gx + s_q[2]*gz - s_q[3]*gy);
    float qDot2 = 0.5f * ( s_q[0]*gy - s_q[1]*gz + s_q[3]*gx);
    float qDot3 = 0.5f * ( s_q[0]*gz + s_q[1]*gy - s_q[2]*gx);

    s_q[0] += qDot0 * dt;
    s_q[1] += qDot1 * dt;
    s_q[2] += qDot2 * dt;
    s_q[3] += qDot3 * dt;
    normalize_q();
}

static void mahony_step(float gx, float gy, float gz,
                        float ax, float ay, float az,
                        float dt)
{
    /* Skip accel feedback if all-zero (would NaN on normalize). */
    if (!(ax == 0.0f && ay == 0.0f && az == 0.0f)) {
        float invA = 1.0f / sqrtf(ax*ax + ay*ay + az*az);
        ax *= invA; ay *= invA; az *= invA;

        /* Estimated gravity (gravity-row of body-frame rotation). */
        float vx = s_q[1]*s_q[3] - s_q[0]*s_q[2];
        float vy = s_q[0]*s_q[1] + s_q[2]*s_q[3];
        float vz = s_q[0]*s_q[0] - 0.5f + s_q[3]*s_q[3];

        /* Cross product (measured x estimated) — error sin(theta). */
        float ex = (ay * vz - az * vy);
        float ey = (az * vx - ax * vz);
        float ez = (ax * vy - ay * vx);

        if (s_twoKi > 0.0f) {
            s_iFx += s_twoKi * ex * dt;
            s_iFy += s_twoKi * ey * dt;
            s_iFz += s_twoKi * ez * dt;
            gx += s_iFx;
            gy += s_iFy;
            gz += s_iFz;
        } else {
            s_iFx = 0.0f; s_iFy = 0.0f; s_iFz = 0.0f;
        }

        gx += s_twoKp * ex;
        gy += s_twoKp * ey;
        gz += s_twoKp * ez;
    }

    /* Quaternion integration: q += 0.5 * q (x) omega * dt */
    gx *= 0.5f * dt;
    gy *= 0.5f * dt;
    gz *= 0.5f * dt;
    float qa = s_q[0], qb = s_q[1], qc = s_q[2];
    s_q[0] += (-qb*gx - qc*gy - s_q[3]*gz);
    s_q[1] += ( qa*gx + qc*gz - s_q[3]*gy);
    s_q[2] += ( qa*gy - qb*gz + s_q[3]*gx);
    s_q[3] += ( qa*gz + qb*gy - qc*gx);
    normalize_q();
}

/* ---- Public API ------------------------------------------------------ */

void mahony_reset(void)
{
    s_q[0] = 1.0f; s_q[1] = 0.0f; s_q[2] = 0.0f; s_q[3] = 0.0f;
    s_twoKp = TWO_KP_DEFAULT;
    s_twoKi = TWO_KI_DEFAULT;
    s_iFx = 0.0f; s_iFy = 0.0f; s_iFz = 0.0f;
    s_ax_lpf = 0.0f; s_ay_lpf = 0.0f; s_az_lpf = 0.0f;
    s_gyro_only = false;
}

void mahony_set_initial_from_accel(float ax, float ay, float az)
{
    /* Shortest-arc quaternion that rotates world +Z to measured accel
     * direction (assuming accel == -gravity when stationary; here we
     * use the raw measured direction). For az ≈ -1 (upside down) we
     * fall back to a 180° flip around X to avoid the singularity. */
    float n2 = ax*ax + ay*ay + az*az;
    if (n2 < 1e-6f) return;
    float inv = 1.0f / sqrtf(n2);
    ax *= inv; ay *= inv; az *= inv;

    float w = 1.0f + az;
    float n2q = w*w + ax*ax + ay*ay;
    if (n2q < 1e-9f) {
        s_q[0] = 0.0f; s_q[1] = 1.0f; s_q[2] = 0.0f; s_q[3] = 0.0f;
        return;
    }
    float invn = 1.0f / sqrtf(n2q);
    s_q[0] =  w  * invn;
    s_q[1] = -ay * invn;
    s_q[2] =  ax * invn;
    s_q[3] =  0.0f;
}

void mahony_update(float gx_dps, float gy_dps, float gz_dps,
                   float ax_g,   float ay_g,   float az_g,
                   float dt)
{
    /* LPF on accel — smooths vibration before the trust decision. */
    s_ax_lpf = ACC_LPF_ALPHA * ax_g + (1.0f - ACC_LPF_ALPHA) * s_ax_lpf;
    s_ay_lpf = ACC_LPF_ALPHA * ay_g + (1.0f - ACC_LPF_ALPHA) * s_ay_lpf;
    s_az_lpf = ACC_LPF_ALPHA * az_g + (1.0f - ACC_LPF_ALPHA) * s_az_lpf;

    /* Boost hysteresis on accel magnitude (compared in g^2 to skip sqrt). */
    float accMag2 = s_ax_lpf*s_ax_lpf + s_ay_lpf*s_ay_lpf + s_az_lpf*s_az_lpf;
    if (s_gyro_only) {
        if (accMag2 < (ACC_THRESH_LOW_G * ACC_THRESH_LOW_G)) {
            s_gyro_only = false;
        }
    } else {
        if (accMag2 > (ACC_THRESH_HIGH_G * ACC_THRESH_HIGH_G)) {
            s_gyro_only = true;
        }
    }

    /* Adapt Kp/Ki only when accel is trusted. */
    if (!s_gyro_only) {
        update_gains_from_acc_error(s_ax_lpf, s_ay_lpf, s_az_lpf);
    }

    /* BMI088 outputs dps; standard Mahony math wants rad/s. */
    float gx = gx_dps * DEG_TO_RAD;
    float gy = gy_dps * DEG_TO_RAD;
    float gz = gz_dps * DEG_TO_RAD;

    if (s_gyro_only) {
        integrate_gyro_only(gx, gy, gz, dt);
    } else {
        mahony_step(gx, gy, gz, s_ax_lpf, s_ay_lpf, s_az_lpf, dt);
    }
}

mahony_quat_t mahony_get_quat(void)
{
    mahony_quat_t r = { s_q[0], s_q[1], s_q[2], s_q[3] };
    return r;
}

mahony_euler_t mahony_get_euler(void)
{
    float w = s_q[0], x = s_q[1], y = s_q[2], z = s_q[3];
    mahony_euler_t e;

    /* roll (x-axis) */
    float sinr = 2.0f * (w*x + y*z);
    float cosr = 1.0f - 2.0f * (x*x + y*y);
    e.roll = atan2f(sinr, cosr) * RAD_TO_DEG;

    /* pitch (y-axis) — clamp before asin to avoid NaN at gimbal lock */
    float sinp = 2.0f * (w*y - z*x);
    sinp = clampf(sinp, -1.0f, 1.0f);
    e.pitch = asinf(sinp) * RAD_TO_DEG;

    /* yaw (z-axis) */
    float siny = 2.0f * (w*z + x*y);
    float cosy = 1.0f - 2.0f * (y*y + z*z);
    e.yaw = atan2f(siny, cosy) * RAD_TO_DEG;

    return e;
}

bool mahony_is_gyro_only(void)
{
    return s_gyro_only;
}
