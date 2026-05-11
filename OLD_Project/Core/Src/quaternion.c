#include "quaternion.h"
#include <math.h>

// Kalman parametreleri
static float q[4] = {1.0f, 0, 0, 0}; // kuaternion
static float P[4][4]; // hata kovaryans matrisi

static const float Q_val = 1e-5f;  // sistem gürültüsü
static const float R_val = 1e-2f;  // ölçüm gürültüsü

void ekf_init(void) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            P[i][j] = (i == j) ? 1e-3f : 0.0f;
}

void ekf_predict(float gx, float gy, float gz, float dt) {

    // Gyro'dan omega matrisi
    float Omega[4][4] = {
        {0, -gx, -gy, -gz},
        {gx,  0,  gz, -gy},
        {gy, -gz,  0,  gx},
        {gz,  gy, -gx,  0}
    };

    // dq = 0.5 * Omega * q
    float dq[4] = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            dq[i] += Omega[i][j] * q[j];
        }
        dq[i] *= 0.5f * dt;
    }

    // q = q + dq
    for (int i = 0; i < 4; i++)
        q[i] += dq[i];

    // Normalize
    float norm = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    for (int i = 0; i < 4; i++)
        q[i] /= norm;

    // P = P + Q
    for (int i = 0; i < 4; i++)
        P[i][i] += Q_val;
}

void ekf_update(float ax, float ay, float az) {

	static float ax_f = 0, ay_f = 0, az_f = 0;
	float alpha = 0.1f;

	ax_f = alpha * ax + (1 - alpha) * ax_f;
	ay_f = alpha * ay + (1 - alpha) * ay_f;
	az_f = alpha * az + (1 - alpha) * az_f;

	float norm = sqrtf(ax_f*ax_f + ay_f*ay_f + az_f*az_f);
	if (fabsf(norm - 1.0f) > 0.1f) return;
	if (norm < 1e-6f) return;
	ax_f /= norm;
	ay_f /= norm;
	az_f /= norm;


    // Gravity vektörü tahmini (dünya referansında [0 0 1])
    float gx = 2 * (q[1]*q[3] - q[0]*q[2]);
    float gy = 2 * (q[0]*q[1] + q[2]*q[3]);
    float gz = q[0]*q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3];

    float y[3] = {ax_f - gx, ay_f - gy, az_f - gz};

    // H sabit alınır (yaklaşım): her q bileşeninin gravity ile türevi
    float H[3][4] = {
        {-2*q[2],  2*q[3], -2*q[0],  2*q[1]},
        { 2*q[1],  2*q[0],  2*q[3],  2*q[2]},
        { 2*q[0], -2*q[1], -2*q[2],  2*q[3]}
    };

    // Kalman gain K = P*H^T * (H*P*H^T + R)^-1
    float HP[3][4] = {0};
    float S[3][3] = {0};
    float K[4][3] = {0};

    // HP = H * P
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                HP[i][j] += H[i][k] * P[k][j];

    // S = HP * H^T + R
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 4; k++)
                S[i][j] += HP[i][k] * H[j][k];

    for (int i = 0; i < 3; i++)
        S[i][i] += R_val;

    // Basit ters alma: sadece diagonal R olduğu için yaklaşık olarak K = P * H^T / S
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            float sum = 0;
            for (int k = 0; k < 4; k++) {
                sum += P[i][k] * H[j][k];
            }
            K[i][j] = sum / S[j][j];
        }
    }

    // q = q + K * y
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            q[i] += K[i][j] * y[j];
        }
    }

    // Normalize q
    float qnorm = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    for (int i = 0; i < 4; i++)
        q[i] /= qnorm;

    // P = (I - K*H) * P
    float KH[4][4] = {0};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 3; k++)
                KH[i][j] += K[i][k] * H[k][j];

    float I_KH[4][4];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            I_KH[i][j] = (i == j ? 1.0f : 0.0f) - KH[i][j];

    float newP[4][4] = {0};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 4; k++)
                newP[i][j] += I_KH[i][k] * P[k][j];

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            P[i][j] = newP[i][j];
}

float ekf_getTheta(void) {
    float r13 = 2 * q[1] * q[3] + 2 * q[2] * q[0];
    float r23 = 2 * q[2] * q[3] - 2 * q[1] * q[0];
    float r33 = 1 - 2 * q[1] * q[1] - 2 * q[2] * q[2];

    float dotProduct = r33;
    float magnitude = sqrtf(r13 * r13 + r23 * r23 + r33 * r33);

    float safeValue = fmaxf(-1.0f, fminf(1.0f, dotProduct / magnitude));
    return acosf(safeValue) * (180.0f / 3.14f);
}

float quaternionToYaw1(){
float yaw = atan2f(2.0f*(q[1]*q[2] + q[0]*q[3]), q[0]*q[0] + q[1]*q[1] - q[2]*q[2] - q[3]*q[3]) * 180.0f / 3.14159f;
return yaw;
}

float quaternionToPitch1(){
float pitch = -asinf(2.0f*(q[1]*q[3] - q[0]*q[2])) * 180.0f / 3.14159f;
return pitch;
}

float quaternionToRoll1(){
float roll = atan2f(2.0f*(q[0]*q[1] + q[2]*q[3]), q[0]*q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3]) * 180.0f / 3.14159f;
return roll;
}

float quaternionToYawDegree1(){
    float w = q[0], x = q[1], y = q[2], z = q[3];
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    return atan2f(siny_cosp, cosy_cosp) * (180.0f / 3.14159265f);
}

float* ekf_getQuaternion(void) {
    return q;
}
