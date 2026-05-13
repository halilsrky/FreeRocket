#ifndef INC_ALT_KALMAN_H_
#define INC_ALT_KALMAN_H_

/*
 * 3-state altitude Kalman filter: [altitude_rel(m), velocity(m/s), accel(m/s²)]
 *
 * Inputs each step:
 *   alt_rel    — baro altitude relative to boot (m)
 *   accel_vert — world-frame vertical acceleration minus gravity (m/s²)
 *   dt         — time since last call (s)
 */

typedef struct {
    float x[3];    /* state: [alt_rel, vel, acc] */
    float P[3][3]; /* error covariance */
    float q;       /* process noise scalar */
    float r_alt;   /* baro noise variance  (m²)       — tune up for noisy baro */
    float r_acc;   /* accel noise variance ((m/s²)²)  — tune up for noisy IMU  */
} alt_kalman_t;

void  alt_kalman_init(alt_kalman_t *kf);
float alt_kalman_update(alt_kalman_t *kf, float alt_rel, float accel_vert, float dt);

static inline float alt_kalman_velocity(const alt_kalman_t *kf) { return kf->x[1]; }
static inline float alt_kalman_accel   (const alt_kalman_t *kf) { return kf->x[2]; }

#endif /* INC_ALT_KALMAN_H_ */
