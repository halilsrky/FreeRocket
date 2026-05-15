#include "alt_kalman.h"
#include <math.h>

/*
 * State transition matrix F (constant-acceleration model):
 *
 *   F = | 1  dt  dt²/2 |
 *       | 0   1  dt    |
 *       | 0   0   1    |
 *
 * Measurement matrix H (observe altitude and acceleration):
 *
 *   H = | 1  0  0 |
 *       | 0  0  1 |
 *
 * Process noise Q derived from a single scalar q:
 *
 *   Q = q * | dt⁴/4  dt³/2  dt²/2 |
 *           | dt³/2  dt²    dt    |
 *           | dt²/2  dt     1     |
 */

void alt_kalman_init(alt_kalman_t *kf)
{
    kf->x[0] = 0.0f;
    kf->x[1] = 0.0f;
    kf->x[2] = 0.0f;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            kf->P[i][j] = 0.0f;

    kf->P[0][0] = 1.0f;
    kf->P[1][1] = 0.1f;
    kf->P[2][2] = 1.0f;

    kf->q     = 0.01f;   /* process noise — increase if filter lags */
    kf->r_alt = 50.0f;   /* baro noise variance   (~1 m std dev)    */
    kf->r_acc = 0.1f;  /* accel noise variance — daha büyük = baroya daha çok güven */
}

float alt_kalman_update(alt_kalman_t *kf, float alt_rel, float accel_vert, float dt)
{
    if (dt <= 0.0f) return kf->x[0];

    const float dt2 = dt  * dt;
    const float dt3 = dt2 * dt;
    const float dt4 = dt2 * dt2;

    /* ── 1. Time update (predict) ──────────────────────────────── */

    float x0 = kf->x[0] + kf->x[1]*dt + kf->x[2]*dt2*0.5f;
    float x1 = kf->x[1] + kf->x[2]*dt;
    float x2 = kf->x[2];
    kf->x[0] = x0;
    kf->x[1] = x1;
    kf->x[2] = x2;

    float F[3][3] = {
        { 1.0f, dt,   dt2*0.5f },
        { 0.0f, 1.0f, dt       },
        { 0.0f, 0.0f, 1.0f     },
    };

    float Q[3][3] = {
        { kf->q*dt4*0.25f, kf->q*dt3*0.5f, kf->q*dt2*0.5f },
        { kf->q*dt3*0.5f,  kf->q*dt2,      kf->q*dt        },
        { kf->q*dt2*0.5f,  kf->q*dt,       kf->q           },
    };

    /* P = F*P*F' + Q */
    float FP[3][3] = {0};
    float FPFT[3][3] = {0};

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
                FP[i][j] += F[i][k] * kf->P[k][j];

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++)
                FPFT[i][j] += FP[i][k] * F[j][k];  /* F'[k][j] == F[j][k] */

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            kf->P[i][j] = FPFT[i][j] + Q[i][j];

    /* ── 2. Measurement update (correct) ───────────────────────── */

    /* H = [1 0 0; 0 0 1],  y = z - H*x */
    float y0 = alt_rel    - kf->x[0];
    float y1 = accel_vert - kf->x[2];

    /* HP = H*P: row 0 of H picks P row 0, row 1 picks P row 2 */
    float HP0[3], HP1[3];
    for (int j = 0; j < 3; j++) {
        HP0[j] = kf->P[0][j];
        HP1[j] = kf->P[2][j];
    }

    /* S = HP*H' + R  (2×2).  H' cols: col0=[1,0,0]', col1=[0,0,1]' */
    float S00 = HP0[0] + kf->r_alt;
    float S01 = HP0[2];
    float S10 = HP1[0];
    float S11 = HP1[2] + kf->r_acc;

    float det = S00*S11 - S01*S10;
    if (fabsf(det) < 1e-9f) return kf->x[0];

    float Si00 =  S11 / det;
    float Si01 = -S01 / det;
    float Si10 = -S10 / det;
    float Si11 =  S00 / det;

    /* PHt = P*H'  (3×2): col0 = P col0, col1 = P col2 */
    float PHt0[3], PHt1[3];
    for (int i = 0; i < 3; i++) {
        PHt0[i] = kf->P[i][0];
        PHt1[i] = kf->P[i][2];
    }

    /* K = PHt * S_inv  (3×2) */
    float K0[3], K1[3];  /* K col0, K col1 */
    for (int i = 0; i < 3; i++) {
        K0[i] = PHt0[i]*Si00 + PHt1[i]*Si10;
        K1[i] = PHt0[i]*Si01 + PHt1[i]*Si11;
    }

    /* x = x + K*y */
    for (int i = 0; i < 3; i++)
        kf->x[i] += K0[i]*y0 + K1[i]*y1;

    /* P = (I - K*H)*P */
    /* KH[i][j] = K col0[i]*H row0[j] + K col1[i]*H row1[j]
     * H row0 = [1,0,0], H row1 = [0,0,1]
     * => KH[i][0] = K0[i], KH[i][2] = K1[i], rest zero */
    float KH[3][3] = {0};
    for (int i = 0; i < 3; i++) {
        KH[i][0] = K0[i];
        KH[i][2] = K1[i];
    }

    float Pnew[3][3] = {0};
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 3; k++) {
                float IKH_ik = (i == k ? 1.0f : 0.0f) - KH[i][k];
                Pnew[i][j] += IKH_ik * kf->P[k][j];
            }

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            kf->P[i][j] = Pnew[i][j];

    return kf->x[0];
}
