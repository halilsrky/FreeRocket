/**
 * @file kalman.h
 * @brief Kalman filter for altitude and acceleration estimation
 * @date 2025-07-05
 * @author halilsrky
 */

#ifndef KALMAN_H
#define KALMAN_H

typedef struct {
    // State vector [altitude, velocity, acceleration]
    float x[3];

    // Covariance matrix
    float P[3][3];

    // Process noise coefficient
    float process_noise;

    // Measurement noise coefficients
    float measurement_noise_alt;
    float measurement_noise_acc;

    // Apogee detection variables
    int apogee_detected;
    int apogee_counter;
    float prev_velocity;

    // Mach transition region control
    int in_mach_transition;
} KalmanFilter_t;

void KalmanFilter_Init(KalmanFilter_t *kf);
float KalmanFilter_Update(KalmanFilter_t *kf, float altitude, float accel, float dt);
int KalmanFilter_IsApogeeDetected(KalmanFilter_t *kf);
float Kalman_Get_Velocity(KalmanFilter_t *kf);
#endif /* KALMAN_H */
