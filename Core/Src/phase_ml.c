/*
 * phase_ml.c — Roket faz dedektörü ML çıkarım modülü.
 *
 * Model: Dense NN  14 giriş → 64 → 32 → 5 sınıf (int8 TFLite, ST Edge AI v4)
 * Pencere: 200 IMU örneği, 10 baro okuması, her 100 örnekte çıkarım.
 *
 * Sınıflar: 0 PAD  1 BOOST  2 COAST  3 APOGEE  4 DESCENT
 */

#include "phase_ml.h"
#include "network.h"
#include "network_data.h"
#include "stai.h"
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Pencere parametreleri ── */
#define IMU_WINDOW    200u
#define IMU_STEP      100u
#define BARO_WINDOW   10u
#define N_FEATURES    14u
#define N_CLASSES     5u

/* ── Quantization (stedgeai analyze çıktısı) ── */
#define INPUT_SCALE       0.19289371371269226f
#define INPUT_ZERO_POINT  (-8)

/* ── Z-score normalizasyon sabitleri (scaler_params.json) ── */
static const float FEAT_MEAN[N_FEATURES] = {
     6.1332f,    /* accel_z_mean     */
     0.8215f,    /* accel_z_std      */
     7.5851f,    /* accel_z_max      */
     0.0011f,    /* accel_x_mean     */
     0.0918f,    /* accel_x_std      */
    -0.2282f,    /* accel_y_mean     */
     0.0949f,    /* accel_y_std      */
     0.3392f,    /* accel_lateral_rms*/
     0.0568f,    /* gyro_mag_mean    */
     0.0120f,    /* gyro_mag_std     */
    13.3005f,    /* jerk_z_mean      */
  1365.6988f,    /* baro_mean        */
     1.6260f,    /* baro_slope       */
    23.9758f,    /* baro_std         */
};

static const float FEAT_SCALE[N_FEATURES] = {
    16.2021f,
     3.4651f,
    19.3601f,
     0.0803f,
     0.1710f,
     0.4100f,
     0.1874f,
     0.4175f,
     0.1026f,
     0.0391f,
    11.0951f,
  1114.5356f,
   110.8266f,
    20.9574f,
};

/* ── Ring buffer yapısı (statik — stack'e koyulmaz) ── */
typedef struct {
    float    az[IMU_WINDOW], ax[IMU_WINDOW], ay[IMU_WINDOW];
    float    gx[IMU_WINDOW], gy[IMU_WINDOW], gz[IMU_WINDOW];
    float    ts[IMU_WINDOW];
    float    baro[BARO_WINDOW];
    float    baro_t[BARO_WINDOW];
    uint16_t head;        /* sonraki yazma pozisyonu */
    uint16_t count;       /* dolu örnek sayısı (maks IMU_WINDOW) */
    uint8_t  baro_head;
    uint8_t  baro_count;
    uint32_t total;       /* toplam eklenen IMU örneği */
    uint32_t last_infer;  /* son çıkarım tetik indeksi (total/IMU_STEP) */
} PhaseBuffer;

static PhaseBuffer s_buf;
static uint8_t     s_phase;

/* ── ST Edge AI bağlamı ── */
STAI_NETWORK_CONTEXT_DECLARE(s_net_ctx, STAI_NETWORK_CONTEXT_SIZE)
static uint8_t  s_act_buf[STAI_NETWORK_ACTIVATIONS_SIZE] STAI_ALIGNED(4);
static stai_ptr s_in[STAI_NETWORK_IN_NUM];
static stai_ptr s_out[STAI_NETWORK_OUT_NUM];
static bool     s_net_ready = false;

/* ── Float → int8 quantize ── */
static int8_t quantize(float x)
{
    int32_t q = (int32_t)roundf(x / INPUT_SCALE) + INPUT_ZERO_POINT;
    if (q >  127) q =  127;
    if (q < -128) q = -128;
    return (int8_t)q;
}

/* ── Özellik çıkarma (buffer kronolojik sırayla okunur) ── */
static void extract_features(float *f)
{
    float sum_az=0.0f, sum_az2=0.0f, max_az=-1e9f;
    float sum_ax=0.0f, sum_ax2=0.0f;
    float sum_ay=0.0f, sum_ay2=0.0f;
    float sum_lat=0.0f, sum_gm=0.0f, sum_gm2=0.0f, sum_jerk=0.0f;

    for (uint16_t i = 0u; i < IMU_WINDOW; i++) {
        /* head = en eski örneğin indeksi */
        uint16_t idx  = (s_buf.head + i) % IMU_WINDOW;
        float az = s_buf.az[idx], ax = s_buf.ax[idx], ay = s_buf.ay[idx];
        float gx = s_buf.gx[idx], gy = s_buf.gy[idx], gz = s_buf.gz[idx];

        sum_az  += az;  sum_az2 += az * az;
        if (az > max_az) max_az = az;
        sum_ax  += ax;  sum_ax2 += ax * ax;
        sum_ay  += ay;  sum_ay2 += ay * ay;
        sum_lat += sqrtf(ax * ax + ay * ay);

        float gm = sqrtf(gx * gx + gy * gy + gz * gz);
        sum_gm  += gm;  sum_gm2 += gm * gm;

        if (i > 0u) {
            uint16_t prev = (s_buf.head + i - 1u) % IMU_WINDOW;
            float dt = s_buf.ts[idx] - s_buf.ts[prev];
            if (dt < 1e-6f) dt = 1e-6f;
            sum_jerk += fabsf(az - s_buf.az[prev]) / dt;
        }
    }

    float n = (float)IMU_WINDOW;
    f[0]  = sum_az / n;
    f[1]  = sqrtf(sum_az2 / n - f[0] * f[0]);
    f[2]  = max_az;
    f[3]  = sum_ax / n;
    f[4]  = sqrtf(sum_ax2 / n - f[3] * f[3]);
    f[5]  = sum_ay / n;
    f[6]  = sqrtf(sum_ay2 / n - f[5] * f[5]);
    f[7]  = sum_lat / n;
    f[8]  = sum_gm / n;
    f[9]  = sqrtf(sum_gm2 / n - f[8] * f[8]);
    f[10] = sum_jerk / (float)(IMU_WINDOW - 1u);

    /* Baro: yeterli örnek yoksa eğitim ortalamasını kullan */
    uint8_t nb = s_buf.baro_count;
    if (nb == 0u) {
        f[11] = FEAT_MEAN[11];
        f[12] = FEAT_MEAN[12];
        f[13] = FEAT_MEAN[13];
        return;
    }

    float bsum=0.0f, btsum=0.0f, bbt=0.0f, bt2=0.0f, bvar=0.0f;
    for (uint8_t i = 0u; i < nb; i++) {
        float b  = s_buf.baro[i];
        float bt = s_buf.baro_t[i];
        bsum  += b;
        btsum += bt;
        bbt   += b * bt;
        bt2   += bt * bt;
    }
    float baro_mean = bsum / nb;
    float t_mean    = btsum / nb;
    float slope_num = bbt - nb * baro_mean * t_mean;
    float slope_den = bt2 - nb * t_mean * t_mean;
    for (uint8_t i = 0u; i < nb; i++) {
        float d = s_buf.baro[i] - baro_mean;
        bvar += d * d;
    }

    f[11] = baro_mean;
    f[12] = (fabsf(slope_den) > 1e-6f) ? slope_num / slope_den : 0.0f;
    f[13] = sqrtf(bvar / nb);
}

/* ── Çıkarım: feature → normalize → quantize → model → argmax ── */
static void run_inference(void)
{
    float features[N_FEATURES];
    extract_features(features);

    for (uint8_t i = 0u; i < N_FEATURES; i++)
        features[i] = (features[i] - FEAT_MEAN[i]) / FEAT_SCALE[i];

    int8_t *in_ptr = (int8_t *)s_in[0];
    for (uint8_t i = 0u; i < N_FEATURES; i++)
        in_ptr[i] = quantize(features[i]);

    stai_network_run(s_net_ctx, STAI_MODE_SYNC);

    int8_t *out_ptr = (int8_t *)s_out[0];
    uint8_t best = 0u;
    for (uint8_t i = 1u; i < N_CLASSES; i++)
        if (out_ptr[i] > out_ptr[best]) best = i;
    s_phase = best;
}

/* ── Public API ── */

void phase_ml_init(void)
{
    memset(&s_buf, 0, sizeof(s_buf));
    s_phase = 0u;

    /* Network bağlamı tek sefer kurulur */
    if (!s_net_ready) {
        stai_network_init(s_net_ctx);
        stai_ptr act = (stai_ptr)s_act_buf;
        stai_network_set_activations(s_net_ctx, &act, 1u);
        stai_size n;
        stai_network_get_inputs(s_net_ctx, s_in, &n);
        stai_network_get_outputs(s_net_ctx, s_out, &n);
        s_net_ready = true;
    }
}

void phase_ml_push(float az, float ax, float ay,
                   float gx, float gy, float gz, float ts)
{
    uint16_t h = s_buf.head;
    s_buf.az[h] = az;  s_buf.ax[h] = ax;  s_buf.ay[h] = ay;
    s_buf.gx[h] = gx;  s_buf.gy[h] = gy;  s_buf.gz[h] = gz;
    s_buf.ts[h] = ts;
    s_buf.head = (h + 1u) % IMU_WINDOW;
    if (s_buf.count < IMU_WINDOW) s_buf.count++;
    s_buf.total++;

    if (s_buf.count >= IMU_WINDOW) {
        uint32_t trigger = s_buf.total / IMU_STEP;
        if (trigger > s_buf.last_infer) {
            s_buf.last_infer = trigger;
            run_inference();
        }
    }
}

void phase_ml_push_baro(float alt, float t)
{
    uint8_t h = s_buf.baro_head;
    s_buf.baro[h]   = alt;
    s_buf.baro_t[h] = t;
    s_buf.baro_head = (h + 1u) % BARO_WINDOW;
    if (s_buf.baro_count < BARO_WINDOW) s_buf.baro_count++;
}

uint8_t phase_ml_get_phase(void)
{
    return s_phase;
}
