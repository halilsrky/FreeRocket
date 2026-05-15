/*
 * sut_task.c — SUT (Software Under Test) tek-task pipeline.
 *
 * Sorumluluk:
 *   cmd_task'tan gelen SUT_COMBINED paketini alır,
 *   Mahony (gyro-only) → Kalman (baro) → flight_sm zincirini çalıştırır,
 *   26-byte SUT_RESPONSE paketini blocking TX ile gönderir.
 *
 * MODE_NORMAL/SIT: task 200 ms uyur, reset bekler.
 * MODE_SUT: paket gelene kadar notify bekler, gelince pipeline çalışır.
 *
 * TX: HAL_UART_Transmit (blocking) — 26 byte @ 230400 baud ≈ 1.1 ms.
 *     Telemetry task SUT modunda TX yapmadığından çakışma yok.
 */

#include "sut_task.h"
#include "sys_mode.h"
#include "mahony.h"
#include "alt_kalman.h"
#include "alt_snapshot.h"
#include "imu_snapshot.h"
#include "flight_sm.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Sabitler ── */
#define RESP_HEADER  0xAEu
#define RESP_SIZE    26u
#define NOTIFY_PKT   (1U << 0)

/* Kalman parametreleri — SUT modunda ivme kanalı susturulmuş */
#define KF_R_ALT  5.0f
#define KF_R_ACC  5000.0f
#define KF_Q      0.01f

/* ── Modül state ── */
static TaskHandle_t  s_handle;
static QueueHandle_t s_q;

/* ── Yardımcı: float → big-endian ── */
static void put_be_f(uint8_t *dst, float v)
{
    union { float f; uint8_t b[4]; } u = { .f = v };
    dst[0] = u.b[3]; dst[1] = u.b[2]; dst[2] = u.b[1]; dst[3] = u.b[0];
}

/* ── SUT_RESPONSE gönder ── */
static void send_response(float sim_time, float alt,
                           float roll, float pitch, float yaw,
                           uint16_t status)
{
    uint8_t frame[RESP_SIZE];
    frame[0] = RESP_HEADER;
    put_be_f(&frame[1],  sim_time);
    put_be_f(&frame[5],  alt);
    put_be_f(&frame[9],  roll);
    put_be_f(&frame[13], pitch);
    put_be_f(&frame[17], yaw);
    frame[21] = (uint8_t)((status >> 8) & 0xFFu);
    frame[22] = (uint8_t)(status & 0xFFu);

    uint8_t chk = 0u;
    for (uint8_t i = 0u; i < 23u; i++) chk += frame[i];
    frame[23] = chk;
    frame[24] = 0x0Du;
    frame[25] = 0x0Au;

    HAL_UART_Transmit(&huart2, frame, RESP_SIZE, 10u);
}

/* ── Task gövdesi ── */
static void sut_task_fn(void *arg)
{
    (void)arg;
    s_handle = xTaskGetCurrentTaskHandle();

    mahony_t     mah;
    alt_kalman_t kf;
    SystemMode_t prev_mode = MODE_NORMAL;

    float alt_ref    = 0.0f;
    bool  ref_set    = false;
    float prev_imu_t = -1.0f;
    float prev_baro_t= -1.0f;

    for (;;) {
        SystemMode_t mode = sys_mode_get();

        /* ── Mod geçişinde state sıfırla ── */
        if (mode != prev_mode) {
            if (mode == MODE_SUT) {
                mahony_init(&mah);
                alt_kalman_init(&kf);
                kf.r_alt = KF_R_ALT;
                kf.r_acc = KF_R_ACC;
                kf.q     = KF_Q;
                flight_sm_reset();
                alt_ref     = 0.0f;
                ref_set     = false;
                prev_imu_t  = -1.0f;
                prev_baro_t = -1.0f;
                /* Queue'yu temizle — önceki moddan kalan paket olabilir */
                sut_packet_t dummy;
                while (xQueueReceive(s_q, &dummy, 0) == pdTRUE) {}
            }
            prev_mode = mode;
        }

        /* ── SUT dışındaysa uy ── */
        if (mode != MODE_SUT) {
            vTaskDelay(pdMS_TO_TICKS(200u));
            continue;
        }

        /* ── Paket bekle ── */
        uint32_t bits = 0u;
        xTaskNotifyWait(0u, UINT32_MAX, &bits, pdMS_TO_TICKS(500u));

        if (!(bits & NOTIFY_PKT)) continue;

        sut_packet_t pkt;
        if (xQueueReceive(s_q, &pkt, 0u) != pdTRUE) continue;

        /* ── 1. Mahony: IMU batch (gyro-only) ── */
        for (uint8_t i = 0u; i < pkt.count; i++) {
            float dt = (prev_imu_t < 0.0f)
                       ? 0.005f
                       : (pkt.imu[i].sim_time - prev_imu_t);
            if (dt <= 0.0f || dt > 0.2f) dt = 0.005f;
            prev_imu_t = pkt.imu[i].sim_time;
            mahony_update(&mah,
                          pkt.imu[i].gx, pkt.imu[i].gy, pkt.imu[i].gz,
                          0.0f, 0.0f, 0.0f, dt);
        }

        float roll, pitch, yaw;
        mahony_get_euler(&mah, &roll, &pitch, &yaw);

        /* ── 2. Alt referans (ilk pakette ayarla) ── */
        if (!ref_set) {
            alt_ref    = pkt.altitude;
            ref_set    = true;
            prev_baro_t = pkt.baro_sim_time;
            /* İlk paket: AGL = 0, Kalman henüz güncellenmedi */
            send_response(pkt.baro_sim_time, 0.0f, roll, pitch, yaw, 0u);
            continue;
        }

        /* ── 3. Kalman: baro ── */
        float dt_baro = pkt.baro_sim_time - prev_baro_t;
        if (dt_baro <= 0.0f || dt_baro > 1.0f) dt_baro = 0.1f;
        prev_baro_t = pkt.baro_sim_time;

        float alt_rel  = pkt.altitude - alt_ref;
        float filtered = alt_kalman_update(&kf, alt_rel, 0.0f, dt_baro);

        /* ── 4. flight_sm ── */
        alt_snapshot_t alt_snap = {
            .ts_ms        = 0u,
            .altitude_rel = filtered,
            .velocity     = alt_kalman_velocity(&kf),
            .accel_vert   = alt_kalman_accel(&kf),
        };
        imu_snapshot_t imu_snap = { 0 };
        imu_snap.accel.x     = pkt.last_ax;
        imu_snap.accel.y     = pkt.last_ay;
        imu_snap.accel.z     = pkt.last_az;
        imu_snap.euler.roll  = roll;
        imu_snap.euler.pitch = pitch;
        imu_snap.euler.yaw   = yaw;

        flight_sm_update(&alt_snap, &imu_snap);

        flight_snapshot_t fsm = { 0 };
        flight_snapshot_peek(&fsm);

        /* ── 5. Response ── */
        send_response(pkt.baro_sim_time, filtered,
                      roll, pitch, yaw, fsm.status);
    }
}

/* ── Public API ── */

void sut_task_create(void)
{
    s_q = xQueueCreate(1u, sizeof(sut_packet_t));

    static const osThreadAttr_t attr = {
        .name       = "SUT",
        .stack_size = 512u * 4u,
        .priority   = osPriorityNormal,
    };
    osThreadNew(sut_task_fn, NULL, &attr);
}

void sut_task_notify_packet(const sut_packet_t *pkt)
{
    if (s_q == NULL || s_handle == NULL) return;
    xQueueOverwrite(s_q, pkt);
    xTaskNotify(s_handle, NOTIFY_PKT, eSetBits);
}
