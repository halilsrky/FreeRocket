#include "mahony.h"
#include <math.h>

/* Fast inverse square root (Quake III) */
static float inv_sqrt(float x)
{
    float halfx = 0.5f * x;
    float y = x;
    long  i = *(long *)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(float *)&i;
    y = y * (1.5f - halfx * y * y);
    return y;
}

void mahony_init(mahony_t *m)
{
    m->q[0] = 1.0f; m->q[1] = 0.0f; m->q[2] = 0.0f; m->q[3] = 0.0f;
    m->twoKp    = MAHONY_KP_DEFAULT * 2.0f;
    m->twoKi    = MAHONY_KI_DEFAULT * 2.0f;
    m->integral[0] = 0.0f;
    m->integral[1] = 0.0f;
    m->integral[2] = 0.0f;
}

void mahony_update(mahony_t *m,
                   float gx, float gy, float gz,
                   float ax, float ay, float az,
                   float dt)
{
    float *q = m->q;
    float recip_norm;
    float halfvx, halfvy, halfvz;
    float halfex, halfey, halfez;
    float qa, qb, qc;

    /* Accel geçerliyse düzeltme uygula */
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
        recip_norm = inv_sqrt(ax * ax + ay * ay + az * az);
        ax *= recip_norm;
        ay *= recip_norm;
        az *= recip_norm;

        /* Mevcut quaternion'dan yerçekimi tahmini */
        halfvx = q[1] * q[3] - q[0] * q[2];
        halfvy = q[0] * q[1] + q[2] * q[3];
        halfvz = q[0] * q[0] - 0.5f + q[3] * q[3];

        /* Ölçüm ile tahmin arasındaki cross-product hatası */
        halfex = ay * halfvz - az * halfvy;
        halfey = az * halfvx - ax * halfvz;
        halfez = ax * halfvy - ay * halfvx;

        /* İntegral geri besleme */
        if (m->twoKi > 0.0f) {
            m->integral[0] += m->twoKi * halfex * dt;
            m->integral[1] += m->twoKi * halfey * dt;
            m->integral[2] += m->twoKi * halfez * dt;
            gx += m->integral[0];
            gy += m->integral[1];
            gz += m->integral[2];
        } else {
            m->integral[0] = 0.0f;
            m->integral[1] = 0.0f;
            m->integral[2] = 0.0f;
        }

        /* Oransal geri besleme */
        gx += m->twoKp * halfex;
        gy += m->twoKp * halfey;
        gz += m->twoKp * halfez;
    }

    /* Quaternion entegrasyonu */
    gx *= 0.5f * dt;
    gy *= 0.5f * dt;
    gz *= 0.5f * dt;

    qa = q[0]; qb = q[1]; qc = q[2];
    q[0] += (-qb * gx - qc * gy - q[3] * gz);
    q[1] += ( qa * gx + qc * gz - q[3] * gy);
    q[2] += ( qa * gy - qb * gz + q[3] * gx);
    q[3] += ( qa * gz + qb * gy - qc * gx);

    /* Normalize et */
    recip_norm = inv_sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    q[0] *= recip_norm;
    q[1] *= recip_norm;
    q[2] *= recip_norm;
    q[3] *= recip_norm;
}

float mahony_get_theta(const mahony_t *m)
{
    const float *q = m->q;
    /* Rotasyon matrisinin 3. sütunu (body Z ekseninin world frame karşılığı) */
    float r13 = 2.0f * (q[1]*q[3] + q[2]*q[0]);
    float r23 = 2.0f * (q[2]*q[3] - q[1]*q[0]);
    float r33 = 1.0f - 2.0f * (q[1]*q[1] + q[2]*q[2]);

    float mag  = sqrtf(r13*r13 + r23*r23 + r33*r33);
    float safe = (mag > 1e-6f) ? (r33 / mag) : 1.0f;
    safe = fmaxf(-1.0f, fminf(1.0f, safe));
    return acosf(safe) * 57.295779513f;
}

void mahony_get_euler(const mahony_t *m,
                      float *roll, float *pitch, float *yaw)
{
    const float *q = m->q;
    const float rad2deg = 57.295779513f;

    *roll  = atan2f(2.0f*(q[0]*q[1] + q[2]*q[3]),
                    q[0]*q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3]) * rad2deg;

    *pitch = -asinf(2.0f*(q[1]*q[3] - q[0]*q[2])) * rad2deg;

    *yaw   =  atan2f(2.0f*(q[1]*q[2] + q[0]*q[3]),
                     q[0]*q[0] + q[1]*q[1] - q[2]*q[2] - q[3]*q[3]) * rad2deg;
}
