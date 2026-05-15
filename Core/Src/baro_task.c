#include "baro_task.h"
#include "baro_snapshot.h"
#include "alt_snapshot.h"
#include "alt_kalman.h"
#include "imu_snapshot.h"
#include "flight_sm.h"
#include "sys_mode.h"
#include "bme280.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdbool.h>
#include <math.h>

static QueueHandle_t s_baro_q;
static QueueHandle_t s_alt_q;

static void baro_task(void *arg);

void baro_task_create(void)
{
    s_baro_q = xQueueCreate(1, sizeof(baro_snapshot_t));
    s_alt_q  = xQueueCreate(1, sizeof(alt_snapshot_t));

    flight_sm_init();

    static const osThreadAttr_t attr = {
        .name       = "BARO",
        .stack_size = 512 * 4,   /* Kalman matris işlemleri için 256'dan 512'ye çıkarıldı */
        .priority   = osPriorityBelowNormal,
    };
    osThreadNew(baro_task, NULL, &attr);
}

bool baro_snapshot_peek(baro_snapshot_t *out)
{
    return xQueuePeek(s_baro_q, out, 0) == pdTRUE;
}

bool alt_snapshot_peek(alt_snapshot_t *out)
{
    return xQueuePeek(s_alt_q, out, 0) == pdTRUE;
}

/*
 * IMU quaternion ile gövde-frame ivmesini dünya-frame dikey bileşenine çevirir.
 * ENU: dünya Z yukarı, durağanda az_world ≈ +9.81 m/s².
 * Yerçekimi çıkarılınca: durağanda ≈ 0, fırlatmada > 0.
 */
static float accel_vertical(const imu_snapshot_t *s)
{
    float qw = s->q.w, qx = s->q.x, qy = s->q.y, qz = s->q.z;
    float ax = s->accel.x, ay = s->accel.y, az = s->accel.z;

    /* Dünya-frame Z satırı: R_body_to_world[2] · a_body */
    float az_world = 2.0f*(qx*qz - qw*qy)*ax
                   + 2.0f*(qy*qz + qw*qx)*ay
                   + (1.0f - 2.0f*(qx*qx + qy*qy))*az;

    return az_world - 9.81f;
}

static void baro_task(void *arg)
{
    (void)arg;

    bme280_calib_t calib;

    if (bme280_init(&hi2c3, &calib) != HAL_OK) {
        vTaskDelete(NULL);
        return;
    }
    bme280_config(&hi2c3);
    vTaskDelay(pdMS_TO_TICKS(8));

    alt_kalman_t kf;
    alt_kalman_init(&kf);

    float    alt_ref      = 0.0f;
    bool     ref_set      = false;
    uint32_t last_tick    = 0U;
    float    last_sim_time = -1.0f;  /* SUT: önceki baro paketinin sim zamanı */

    SystemMode_t prev_mode = MODE_NORMAL;

    uint8_t    raw[8];
    TickType_t wake_tick = xTaskGetTickCount();

    for (;;) {
        SystemMode_t mode = sys_mode_get();

        /* Mod geçişinde tüm state'i sıfırla */
        if (mode != prev_mode) {
            ref_set       = false;
            last_sim_time = -1.0f;
            alt_kalman_init(&kf);
            if (mode == MODE_SUT) {
                kf.r_acc = 5000.0f;  /* ivme kanalı susturulmuş */
                kf.r_alt = 1.0f;     /* baro varyansı ~0.25 m² → 1.0 temkinli ama gerçekçi */
                kf.q     = 1.0f;     /* hız durumu hızlı değişebilsin (yanma fazı için) */
            }
            else wake_tick = xTaskGetTickCount(); /* NORMAL/SIT'e dönüşte timer sıfırla */
            flight_sm_reset();
            prev_mode = mode;
        }

        /* ── SUT: yeni paket gelene kadar engelle (sabit 100 ms bekleme yok) ── */
        if (mode == MODE_SUT) {
            sut_baro_t sut_baro;
            if (!sys_mode_sut_baro_receive(&sut_baro, 200U)) continue;

            uint32_t now = HAL_GetTick();

            baro_snapshot_t baro_snap = {
                .ts_ms    = now,
                .pressure = sut_baro.pressure,
                .altitude = sut_baro.altitude,
            };
            xQueueOverwrite(s_baro_q, &baro_snap);

            if (!ref_set) {
                alt_ref       = sut_baro.altitude;
                ref_set       = true;
                last_sim_time = sut_baro.sim_time;
                continue;
            }

            float dt = (last_sim_time < 0.0f) ? 0.1f
                                               : (sut_baro.sim_time - last_sim_time);
            if (dt <= 0.0f || dt > 1.0f) dt = 0.1f;  /* sıfır veya anormal aralık */
            last_sim_time = sut_baro.sim_time;

            float alt_rel = sut_baro.altitude - alt_ref;

            /* SUT: IMU snapshot anlık değer taşır, 100 ms penceredeki son sample
             * motor ateşlenmesinde 87 m/s² görebilir. Kalman yalnızca baro
             * yüksekliği ile beslenir; ivme kanalı r_acc=5000 ile susturulmuş. */
            float avert = 0.0f;

            float filtered_alt = alt_kalman_update(&kf, alt_rel, avert, dt);

            alt_snapshot_t alt_snap = {
                .ts_ms        = now,
                .altitude_rel = filtered_alt,
                .velocity     = alt_kalman_velocity(&kf),
                .accel_vert   = alt_kalman_accel(&kf),
            };
            xQueueOverwrite(s_alt_q, &alt_snap);

            imu_snapshot_t imu_snap;
            const imu_snapshot_t *imu_ptr = imu_snapshot_peek(&imu_snap) ? &imu_snap : NULL;
            flight_sm_update(&alt_snap, imu_ptr);
            continue;
        }

        /* ── NORMAL / SIT: sabit 10 Hz, gerçek sensör yolu ─────────────── */
        vTaskDelayUntil(&wake_tick, pdMS_TO_TICKS(100));

        if (bme280_read(&hi2c3, raw) != HAL_OK) continue;

        bme280_data_t data;
        bme280_parse(raw, &calib, &data);

        uint32_t now = HAL_GetTick();

        baro_snapshot_t baro_snap = {
            .ts_ms       = now,
            .temperature = data.temperature,
            .pressure    = data.pressure,
            .humidity    = data.humidity,
            .altitude    = data.altitude,
        };
        xQueueOverwrite(s_baro_q, &baro_snap);

        if (!ref_set) {
            alt_ref   = data.altitude;
            ref_set   = true;
            last_tick = now;
            continue;
        }

        float dt = (float)(now - last_tick) * 0.001f;
        last_tick = now;

        imu_snapshot_t        imu_snap;
        const imu_snapshot_t *imu_ptr = imu_snapshot_peek(&imu_snap) ? &imu_snap : NULL;

        float avert = imu_ptr ? accel_vertical(imu_ptr) : 0.0f;

        float alt_rel      = data.altitude - alt_ref;
        float filtered_alt = alt_kalman_update(&kf, alt_rel, avert, dt);

        alt_snapshot_t alt_snap = {
            .ts_ms        = now,
            .altitude_rel = filtered_alt,
            .velocity     = alt_kalman_velocity(&kf),
            .accel_vert   = alt_kalman_accel(&kf),
        };
        xQueueOverwrite(s_alt_q, &alt_snap);

        flight_sm_update(&alt_snap, imu_ptr);
    }
}
