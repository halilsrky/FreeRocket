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
        .stack_size = 512 * 4,
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
 */
static float accel_vertical(const imu_snapshot_t *s)
{
    float qw = s->q.w, qx = s->q.x, qy = s->q.y, qz = s->q.z;
    float ax = s->accel.x, ay = s->accel.y, az = s->accel.z;

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

    float    alt_ref   = 0.0f;
    bool     ref_set   = false;
    uint32_t last_tick = 0U;

    TickType_t wake_tick = xTaskGetTickCount();

    for (;;) {
        /* SUT modunda baro okuma yapılmaz — sut_task kendi Kalman'ını çalıştırır */
        if (sys_mode_get() == MODE_SUT) {
            wake_tick = xTaskGetTickCount();  /* catch-up engellemek için sıfırla */
            vTaskDelay(pdMS_TO_TICKS(200U));
            continue;
        }

        vTaskDelayUntil(&wake_tick, pdMS_TO_TICKS(100));

        uint8_t raw[8];
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

        float avert    = imu_ptr ? accel_vertical(imu_ptr) : 0.0f;
        float alt_rel  = data.altitude - alt_ref;
        float filtered = alt_kalman_update(&kf, alt_rel, avert, dt);

        alt_snapshot_t alt_snap = {
            .ts_ms        = now,
            .altitude_rel = filtered,
            .velocity     = alt_kalman_velocity(&kf),
            .accel_vert   = alt_kalman_accel(&kf),
        };
        xQueueOverwrite(s_alt_q, &alt_snap);

        flight_sm_update(&alt_snap, imu_ptr);
    }
}
