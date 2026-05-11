#include "queternion.h"
#include <math.h>
#include "stdbool.h"

extern bmi088_struct_t BMI_sensor;
static float q[4] = {1.0f, 0.0f, 0.0f, 0.0f};  // [w, x, y, z]




//*****************************************************************************************************
//               MAHONY
//*****************************************************************************************************
#define twoKpDef	(2.0f * 2.0f)	// hızlı düzeltme, sensör ivmelerine karşı daha dirençli
#define twoKiDef	(2.0f * 0.01f)	// gyro bias'ına karşı yavaş ama kararlı düzeltme
volatile float twoKp = twoKpDef;											// 2 * proportional gain (Kp)
volatile float twoKi = twoKiDef;											// 2 * integral gain (Ki)
volatile float integralFBx = 0.0f,  integralFBy = 0.0f, integralFBz = 0.0f;	// integral error terms scaled by Ki
//*****************************************************************************************************

#define G 9.80665f
#define ACC_THRESHOLD_HIGH (3.3f * G)
#define ACC_THRESHOLD_LOW  (2.7f * G)

#define TWO_KP_MAX 4.0f
#define TWO_KP_MIN 0.1f
#define TWO_KI_MAX 0.05f
#define TWO_KI_MIN 0.0f
#define ACC_NOMINAL 9.81f
#define ACC_TOLERANCE 0.5f


uint8_t Gain = 0;
uint8_t gyroOnlyMode = 0;


void UpdateMahonyGains(float ax, float ay, float az) {
    // Quaternion normalize edilmemişse gravity yön tahmini hatalı olur
    float norm_q = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (norm_q == 0.0f) return; // Koruma
    q[0] /= norm_q;
    q[1] /= norm_q;
    q[2] /= norm_q;
    q[3] /= norm_q;

    // Gravity yön vektörü (tam vektör formu)
    float gx = 2.0f * (q[1] * q[3] - q[0] * q[2]);
    float gy = 2.0f * (q[0] * q[1] + q[2] * q[3]);
    float gz = q[0]*q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3];

    // Normalize gravity tahmini
    float recipNormG = invSqrt(gx * gx + gy * gy + gz * gz);
    gx *= recipNormG;
    gy *= recipNormG;
    gz *= recipNormG;

    // İvmeölçer zaten normalize edilmiş olarak gelmeli (gelmiyorsa normalize et!)
    float recipNormA = invSqrt(ax * ax + ay * ay + az * az);
    ax *= recipNormA;
    ay *= recipNormA;
    az *= recipNormA;

    // Dot product
    float dot = ax * gx + ay * gy + az * gz;
    if (dot > 1.0f) dot = 1.0f;
    if (dot < -1.0f) dot = -1.0f;

    // Açı farkı (derece)
    float accErrorAngle = acosf(dot) * (180.0f / 3.1415926f);

    // Duruma göre kazanç ayarla
    if (accErrorAngle > 30.0f) {
        Gain = 1;
        twoKp = 0.2f;
        twoKi = 0.0f;
    } else if (accErrorAngle > 10.0f) {
        Gain = 2;
        twoKp = 2.0f;
        twoKi = 0.01f;
    } else {
        Gain = 3;
        twoKp = 8.0f;
        twoKi = 0.05f;
    }

    // Güvenlik: kazanç sınırla (opsiyonel ama önerilir)
    twoKp = fmaxf(TWO_KP_MIN, fminf(twoKp, TWO_KP_MAX));
    twoKi = fmaxf(TWO_KI_MIN, fminf(twoKi, TWO_KI_MAX));
}





void Orientation_Update(float gx, float gy, float gz, float ax, float ay, float az, float dt) {
	static float ax_f = 0, ay_f = 0, az_f = 0;
    const float alpha = 0.3f;

    // LPF filtre
    ax_f = alpha * ax + (1.0f - alpha) * ax_f;
    ay_f = alpha * ay + (1.0f - alpha) * ay_f;
    az_f = alpha * az + (1.0f - alpha) * az_f;

    // Kazançları güncelle
    UpdateMahonyGains(ax_f, ay_f, az_f);

    // Acc magnitude
    float accMag = sqrtf(ax_f*ax_f + ay_f*ay_f + az_f*az_f);

    if (gyroOnlyMode) {
            if (accMag < ACC_THRESHOLD_LOW)
                gyroOnlyMode = 0;
	} else {
		if (accMag > ACC_THRESHOLD_HIGH)
			gyroOnlyMode = 1;
	}



    // Filtre çağrısı
    if (gyroOnlyMode)
        updateQuaternion(gx, gy, gz, dt);
    else
        MahonyAHRSupdateIMU(gx, gy, gz, ax_f, ay_f, az_f, dt);
}


void updateQuaternion(float gx, float gy, float gz, float dt) {

    // Quaternion türevleri
    float qDot1 = 0.5f * (-q[1] * gx - q[2] * gy - q[3] * gz);
    float qDot2 = 0.5f * ( q[0] * gx + q[2] * gz - q[3] * gy);
    float qDot3 = 0.5f * ( q[0] * gy - q[1] * gz + q[3] * gx);
    float qDot4 = 0.5f * ( q[0] * gz + q[1] * gy - q[2] * gx);

    // Entegrasyon
    q[0] += qDot1 * dt;
    q[1] += qDot2 * dt;
    q[2] += qDot3 * dt;
    q[3] += qDot4 * dt;

    // Normalize et
    float norm = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (norm > 1e-6f) {
        q[0] /= norm;
        q[1] /= norm;
        q[2] /= norm;
        q[3] /= norm;
    }
}

void quaternionSet_zero(void) {
    float ax = (BMI_sensor.datas.acc_y/100);
    float ay = (BMI_sensor.datas.acc_z/100);
    float az = (-BMI_sensor.datas.acc_x/100);

    float norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm < 1e-6f) return;

    ax /= norm;
    ay /= norm;
    az /= norm;

    float half_z = sqrtf((1.0f + az) * 0.5f);
    float half_x = -ay / (2.0f * half_z);
    float half_y = ax / (2.0f * half_z);

    q[0] = half_z;
    q[1] = half_x;
    q[2] = half_y;
    q[3] = 0.0f;

    float norm_q = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (norm_q > 1e-6f) {
        q[0] /= norm_q;
        q[1] /= norm_q;
        q[2] /= norm_q;
        q[3] /= norm_q;
    }
}

void MahonyAHRSupdateIMU(float gx, float gy, float gz, float ax_f, float ay_f, float az_f, float dt) {
	float recipNorm;
	float halfvx, halfvy, halfvz;
	float halfex, halfey, halfez;
	float qa, qb, qc;

	// Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
		if(!((ax_f == 0.0f) && (ay_f == 0.0f) && (az_f == 0.0f))) {

		// Normalise accelerometer measurement
		recipNorm = invSqrt(ax_f * ax_f + ay_f * ay_f + az_f * az_f);
		ax_f *= recipNorm;
		ay_f *= recipNorm;
		az_f *= recipNorm;

		// Estimated direction of gravity and vector perpendicular to magnetic flux
		halfvx = q[1] * q[3] - q[0] * q[2];
		halfvy = q[0] * q[1] + q[2] * q[3];
		halfvz = q[0] * q[0] - 0.5f + q[3] * q[3];

		// Error is sum of cross product between estimated and measured direction of gravity
		halfex = (ay_f * halfvz - az_f * halfvy);
		halfey = (az_f * halfvx - ax_f * halfvz);
		halfez = (ax_f * halfvy - ay_f * halfvx);

		// Compute and apply integral feedback if enabled
		if(twoKi > 0.0f) {
			integralFBx += twoKi * halfex * dt;	// integral error scaled by Ki
			integralFBy += twoKi * halfey * dt;
			integralFBz += twoKi * halfez * dt;
			gx += integralFBx;	// apply integral feedback
			gy += integralFBy;
			gz += integralFBz;
		}
		else {
			integralFBx = 0.0f;	// prevent integral windup
			integralFBy = 0.0f;
			integralFBz = 0.0f;
		}

		// Apply proportional feedback
		gx += twoKp * halfex;
		gy += twoKp * halfey;
		gz += twoKp * halfez;
	}

	// Integrate rate of change of quaternion
	gx *= (0.5f * dt);		// pre-multiply common factors
	gy *= (0.5f * dt);
	gz *= (0.5f * dt);
	qa = q[0];
	qb = q[1];
	qc = q[2];
	q[0] += (-qb * gx - qc * gy - q[3] * gz);
	q[1] += (qa * gx + qc * gz - q[3] * gy);
	q[2] += (qa * gy - qb * gz + q[3] * gx);
	q[3] += (qa * gz + qb * gy - qc * gx);

	// Normalise quaternion
	recipNorm = invSqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
	q[0] *= recipNorm;
	q[1] *= recipNorm;
	q[2] *= recipNorm;
	q[3] *= recipNorm;
}


float quaternionToThetaZ() {
    float r13 = 2 * q[1] * q[3] + 2 * q[2] * q[0];
    float r23 = 2 * q[2] * q[3] - 2 * q[1] * q[0];
    float r33 = 1 - 2 * q[1] * q[1] - 2 * q[2] * q[2];

    float dotProduct = r33;
    float magnitude = sqrtf(r13 * r13 + r23 * r23 + r33 * r33);

    float safeValue = fmaxf(-1.0f, fminf(1.0f, dotProduct / magnitude));
    return acosf(safeValue) * (180.0f / 3.14f);
}

float quaternionToYawDegree() {
    float w = q[0], x = q[1], y = q[2], z = q[3];
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    return atan2f(siny_cosp, cosy_cosp) * (180.0f / 3.14159265f);
}

float quaternionToPitchDegree(){
    float w = q[0], x = q[1], y = q[2], z = q[3];
    float sinp = 2.0f * (w * y - z * x);
    float cosp = 1.0f - 2.0f * (y * y + x * x);
    return atan2f(sinp, cosp) * (180.0f / 3.14159265f);
}


float quaternionToRollDegree(){
    float w = q[0], x = q[1], y = q[2], z = q[3];
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    return atan2f(sinr_cosp, cosr_cosp) * (180.0f / 3.14159265f);
}


float quaternionToYaw(){
float yaw = atan2f(2.0f*(q[1]*q[2] + q[0]*q[3]), q[0]*q[0] + q[1]*q[1] - q[2]*q[2] - q[3]*q[3]) * 180.0f / 3.14159f;
return yaw;
}

float quaternionToPitch(){
float pitch = -asinf(2.0f*(q[1]*q[3] - q[0]*q[2])) * 180.0f / 3.14159f;
return pitch;
}

float quaternionToRoll(){
float roll = atan2f(2.0f*(q[0]*q[1] + q[2]*q[3]), q[0]*q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3]) * 180.0f / 3.14159f;
return roll;
}

void getInitialQuaternion() {

    double norm = sqrt(BMI_sensor.datas.acc_z * BMI_sensor.datas.acc_z + BMI_sensor.datas.acc_x * BMI_sensor.datas.acc_x + BMI_sensor.datas.acc_y * BMI_sensor.datas.acc_y);
    double accel_temp[3];

    accel_temp[0] = (double)BMI_sensor.datas.acc_y;
    accel_temp[1] = (double)-BMI_sensor.datas.acc_z;
    accel_temp[2] = (double)BMI_sensor.datas.acc_x;

    accel_temp[0] /= norm;
    accel_temp[1] /= norm;
    accel_temp[2] /= norm;

    double q_temp[4];

    q_temp[0] = sqrt(1.0 -accel_temp[1]) * 0.5;
    double k = 0.5 / q_temp[0];
    q_temp[1] = accel_temp[0] * k * 0.5;
    q_temp[2] = accel_temp[2] * k * 0.5;
    q_temp[3] = 0.0;

    norm = sqrt(q_temp[0] * q_temp[0] + q_temp[1] * q_temp[1] + q_temp[2] * q_temp[2] + q_temp[3] * q_temp[3]);

    q[0] = q_temp[0] / norm;
    q[1] = q_temp[1] / norm;
    q[2] = q_temp[2] / norm;
    q[3] = 0.0f;
}

float invSqrt(float x) {
	float halfx = 0.5f * x;
	float y = x;
	long i = *(long*)&y;
	i = 0x5f3759df - (i>>1);
	y = *(float*)&i;
	y = y * (1.5f - (halfx * y * y));
	return y;
}

float* getQuaternion() {
    return q;
}
