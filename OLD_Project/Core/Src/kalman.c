/**
 * @file kalman.c
 * @brief Kalman filter implementation for rocket altitude tracking
 * @date 2025-07-05
 * @author halilsrky
 */

#include "kalman.h"
#include <math.h>

// Private functions
static void KalmanFilter_TimeUpdate(KalmanFilter_t *kf, float dt);
static void KalmanFilter_MeasurementUpdate(KalmanFilter_t *kf, float altitude, float accel);
static int KalmanFilter_DetectApogee(KalmanFilter_t *kf);

/**
 * @brief Initialize the Kalman filter
 * @param kf Pointer to Kalman filter structure
 */
void KalmanFilter_Init(KalmanFilter_t *kf) {
    // Initialize state vector - başlangıçta güvenli değerler
    kf->x[0] = 0.0f;  // Altitude - başlangıç irtifa
    kf->x[1] = 0.0f;  // Velocity - başlangıçta hız sıfır
    kf->x[2] = 0.0f;  // Acceleration - başlangıçta ivme sıfır

    // Initialize covariance matrix with reasonable initial uncertainty
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            kf->P[i][j] = 0.0f;
        }
    }

    // Başlangıç belirsizlikleri - daha konservatif değerler
    kf->P[0][0] = 1.0f;     // Altitude uncertainty (1 meter)
    kf->P[1][1] = 0.1f;     // Velocity uncertainty (0.1 m/s)
    kf->P[2][2] = 1.0f;     // Acceleration uncertainty (1 m/s²)

    // Set noise parameters - these can be tuned
    kf->process_noise = 0.01f;         // Process noise
    kf->measurement_noise_alt = 0.005f;  // Altitude measurement noise
    kf->measurement_noise_acc = 5.0f;  // Acceleration measurement noise

    // Initialize apogee detection variables
    kf->apogee_detected = 0;
    kf->apogee_counter = 0;
    kf->prev_velocity = 0.0f;

    // Mach transition control
    kf->in_mach_transition = 0;
}

/**
 * @brief Update Kalman filter with new measurements
 * @param kf Pointer to Kalman filter structure
 * @param altitude Measured altitude (meters)
 * @param accel Measured (corrected) acceleration (m/s^2)
 * @param dt Time step (seconds)
 * @return Filtered altitude
 */
float KalmanFilter_Update(KalmanFilter_t *kf, float altitude, float accel, float dt) {
    // Check for Mach transition region (approximately 300-350 m/s)
    // Skip barometer readings in this region due to pressure anomalies
    if (fabsf(kf->x[1]) > 300.0f && fabsf(kf->x[1]) < 350.0f) {
        kf->in_mach_transition = 1;
    } else {
        kf->in_mach_transition = 0;
    }

    // Time update (prediction)
    KalmanFilter_TimeUpdate(kf, dt);

    // Measurement update (correction)
    KalmanFilter_MeasurementUpdate(kf, altitude, accel);

    // Check for apogee
    KalmanFilter_DetectApogee(kf);

    // Return filtered altitude
    return kf->x[0];
}

/**
 * @brief Time update step of Kalman filter (prediction)
 * @param kf Pointer to Kalman filter structure
 * @param dt Time step (seconds)
 */
static void KalmanFilter_TimeUpdate(KalmanFilter_t *kf, float dt) {
    float dt2 = dt * dt;
    float dt3 = dt2 * dt;
    float dt4 = dt2 * dt2;

    // State transition matrix F = [1 dt dt²/2; 0 1 dt; 0 0 1]
    // State prediction: x = F*x
    float x0_new = kf->x[0] + kf->x[1] * dt + kf->x[2] * dt2 / 2.0f;
    float x1_new = kf->x[1] + kf->x[2] * dt;
    float x2_new = kf->x[2];  // Acceleration assumed constant

    kf->x[0] = x0_new;
    kf->x[1] = x1_new;
    kf->x[2] = x2_new;

    // Process noise covariance Q
    float q = kf->process_noise;
    float Q[3][3] = {
        {dt4/4.0f * q, dt3/2.0f * q, dt2/2.0f * q},
        {dt3/2.0f * q, dt2 * q, dt * q},
        {dt2/2.0f * q, dt * q, q}
    };

    // State transition matrix F
    float F[3][3] = {
        {1.0f, dt, dt2/2.0f},
        {0.0f, 1.0f, dt},
        {0.0f, 0.0f, 1.0f}
    };

    // Temporary matrices for calculation
    float FP[3][3] = {0};
    float FPFT[3][3] = {0};

    // P = F*P*F' + Q
    // Step 1: FP = F*P
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            FP[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                FP[i][j] += F[i][k] * kf->P[k][j];
            }
        }
    }

    // Step 2: FPFT = FP*F'
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            FPFT[i][j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                FPFT[i][j] += FP[i][k] * F[j][k];  // F'[k][j] = F[j][k]
            }
        }
    }

    // Step 3: P = FPFT + Q
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            kf->P[i][j] = FPFT[i][j] + Q[i][j];
        }
    }
}

/**
 * @brief Measurement update step of Kalman filter (correction)
 * @param kf Pointer to Kalman filter structure
 * @param altitude Measured altitude (meters)
 * @param accel Measured (corrected) acceleration (m/s^2)
 */
static void KalmanFilter_MeasurementUpdate(KalmanFilter_t *kf, float altitude, float accel) {
    // In Mach transition region, only use acceleration measurement
    if (kf->in_mach_transition) {
        // Only use acceleration measurement
        // H = [0 0 1]
        float H[1][3] = {{0.0f, 0.0f, 1.0f}};
        float z = accel;
        float y = z - kf->x[2];  // Innovation

        // S = H*P*H' + R
        float HP[1][3] = {0};
        float S = 0.0f;

        for (int i = 0; i < 3; i++) {
            HP[0][i] = H[0][0]*kf->P[0][i] + H[0][1]*kf->P[1][i] + H[0][2]*kf->P[2][i];
        }

        S = HP[0][0]*H[0][0] + HP[0][1]*H[0][1] + HP[0][2]*H[0][2] + kf->measurement_noise_acc;

        // K = P*H'*S^-1
        float K[3] = {0};
        float S_inv = 1.0f / S;

        for (int i = 0; i < 3; i++) {
            K[i] = (kf->P[i][0]*H[0][0] + kf->P[i][1]*H[0][1] + kf->P[i][2]*H[0][2]) * S_inv;
        }

        // State update: x = x + K*y
        for (int i = 0; i < 3; i++) {
            kf->x[i] += K[i] * y;
        }

        // Covariance update: P = (I - K*H)*P
        float KH[3][3] = {0};
        float IKH[3][3];
        float Pnew[3][3] = {0};

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                KH[i][j] = K[i] * H[0][j];
                IKH[i][j] = (i == j ? 1.0f : 0.0f) - KH[i][j];
            }
        }

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                Pnew[i][j] = 0.0f;
                for (int k = 0; k < 3; k++) {
                    Pnew[i][j] += IKH[i][k] * kf->P[k][j];
                }
            }
        }

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                kf->P[i][j] = Pnew[i][j];
            }
        }
    } else {
        // Use both altitude and acceleration measurements
        // H = [1 0 0; 0 0 1]
        float H[2][3] = {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f}
        };

        float z[2] = {altitude, accel};
        float y[2] = {z[0] - kf->x[0], z[1] - kf->x[2]};  // Innovation

        // S = H*P*H' + R
        float HP[2][3] = {0};
        float S[2][2] = {0};
        float R[2][2] = {
            {kf->measurement_noise_alt, 0.0f},
            {0.0f, kf->measurement_noise_acc}
        };

        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 3; j++) {
                HP[i][j] = 0.0f;
                for (int k = 0; k < 3; k++) {
                    HP[i][j] += H[i][k] * kf->P[k][j];
                }
            }
        }

        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 2; j++) {
                S[i][j] = 0.0f;
                for (int k = 0; k < 3; k++) {
                    S[i][j] += HP[i][k] * H[j][k];  // H'[k][j] = H[j][k]
                }
                S[i][j] += R[i][j];
            }
        }

        // Calculate S^-1 (inverse of 2x2 matrix)
        float det = S[0][0] * S[1][1] - S[0][1] * S[1][0];
        if (fabsf(det) < 1e-6f) {
            // Matrix is singular, skip update
            return;
        }

        float S_inv[2][2] = {
            {S[1][1] / det, -S[0][1] / det},
            {-S[1][0] / det, S[0][0] / det}
        };

        // K = P*H'*S^-1
        float PHt[3][2] = {0};
        float K[3][2] = {0};

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 2; j++) {
                PHt[i][j] = 0.0f;
                for (int k = 0; k < 3; k++) {
                    PHt[i][j] += kf->P[i][k] * H[j][k];  // H'[k][j] = H[j][k]
                }
            }
        }

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 2; j++) {
                K[i][j] = 0.0f;
                for (int k = 0; k < 2; k++) {
                    K[i][j] += PHt[i][k] * S_inv[k][j];
                }
            }
        }

        // State update: x = x + K*y
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 2; j++) {
                kf->x[i] += K[i][j] * y[j];
            }
        }

        // Covariance update: P = (I - K*H)*P
        float KH[3][3] = {0};
        float IKH[3][3];
        float Pnew[3][3] = {0};

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                KH[i][j] = 0.0f;
                for (int k = 0; k < 2; k++) {
                    KH[i][j] += K[i][k] * H[k][j];
                }
                IKH[i][j] = (i == j ? 1.0f : 0.0f) - KH[i][j];
            }
        }

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                Pnew[i][j] = 0.0f;
                for (int k = 0; k < 3; k++) {
                    Pnew[i][j] += IKH[i][k] * kf->P[k][j];
                }
            }
        }

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                kf->P[i][j] = Pnew[i][j];
            }
        }
    }
}

/**
 * @brief Detect apogee based on velocity trend
 * @param kf Pointer to Kalman filter structure
 * @return 1 if apogee is detected, 0 otherwise
 */
static int KalmanFilter_DetectApogee(KalmanFilter_t *kf) {
    // Velocity is negative and magnitude is increasing
	if(kf->x[2] > 30.0){


	}
    if (kf->x[1] < 0.0f && kf->x[1] < kf->prev_velocity) {
        kf->apogee_counter++;
        if (kf->apogee_counter >= 5) {  // Confirm apogee after 5 consecutive samples
            kf->apogee_detected = 1;
        }
    } else {
        kf->apogee_counter = 0;
    }

    kf->prev_velocity = kf->x[1];
    return kf->apogee_detected;
}
float Kalman_Get_Velocity(KalmanFilter_t *kf){
	return kf->x[1];
}
/**
 * @brief Check if apogee has been detected
 * @param kf Pointer to Kalman filter structure
 * @return 1 if apogee is detected, 0 otherwise
 */
int KalmanFilter_IsApogeeDetected(KalmanFilter_t *kf) {
    return kf->apogee_detected;
}
