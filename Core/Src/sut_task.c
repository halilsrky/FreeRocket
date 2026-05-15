/*
 * sut_task.c — SUT pipeline (ML faz dedektörü ile).
 *
 * Sorumluluk:
 *   cmd_task'tan gelen SUT_COMBINED paketini alır,
 *   IMU/baro verilerini phase_ml ring buffer'ına atar,
 *   ML modelinden faz bilgisi alır ve 26-byte SUT_RESPONSE gönderir.
 *
 * Filtreleme yok. Raw SUT verisi direkt modele gider.
 * Response: sim_time | raw_altitude | 0 | 0 | 0 | faz(0-4)
 */

#include "sut_task.h"
#include "sys_mode.h"
#include "phase_ml.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdint.h>

/* ── Sabitler ── */
#define RESP_HEADER  0xAEu
#define RESP_SIZE    26u
#define NOTIFY_PKT   (1U << 0)

/*
 * ML faz (0-4) → flight_sm bit flag dönüşüm tablosu.
 * PC, status alanını kümülatif FSM_BIT_* olarak yorumlar.
 *
 *  0 PAD     → 0x0000
 *  1 BOOST   → 0x0001  LAUNCHED
 *  2 COAST   → 0x0007  LAUNCHED|BURNOUT|ARMED
 *  3 APOGEE  → 0x0217  +APOGEE|VEL_APOGEE
 *  4 DESCENT → 0x0237  +DROGUE
 */
static const uint16_t PHASE_TO_STATUS[5] = {
    0x0000u,  /* PAD     */
    0x0001u,  /* BOOST   */
    0x0007u,  /* COAST   */
    0x0217u,  /* APOGEE  */
    0x0237u,  /* DESCENT */
};

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
static void send_response(float sim_time, float alt, uint16_t status)
{
    uint8_t frame[RESP_SIZE];
    frame[0] = RESP_HEADER;
    put_be_f(&frame[1],  sim_time);
    put_be_f(&frame[5],  alt);
    put_be_f(&frame[9],  0.0f);   /* roll  = 0 */
    put_be_f(&frame[13], 0.0f);   /* pitch = 0 */
    put_be_f(&frame[17], 0.0f);   /* yaw   = 0 */
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

    SystemMode_t prev_mode = MODE_NORMAL;

    for (;;) {
        SystemMode_t mode = sys_mode_get();

        /* Mod geçişinde ML buffer'ı sıfırla */
        if (mode != prev_mode) {
            if (mode == MODE_SUT) {
                phase_ml_init();
                sut_packet_t dummy;
                while (xQueueReceive(s_q, &dummy, 0) == pdTRUE) {}
            }
            prev_mode = mode;
        }

        if (mode != MODE_SUT) {
            vTaskDelay(pdMS_TO_TICKS(200u));
            continue;
        }

        /* Paket bekle */
        uint32_t bits = 0u;
        xTaskNotifyWait(0u, UINT32_MAX, &bits, pdMS_TO_TICKS(500u));

        if (!(bits & NOTIFY_PKT)) continue;

        sut_packet_t pkt;
        if (xQueueReceive(s_q, &pkt, 0u) != pdTRUE) continue;

        /* IMU örneklerini ring buffer'a at */
        for (uint8_t i = 0u; i < pkt.count; i++) {
            phase_ml_push(pkt.last_az, pkt.last_ax, pkt.last_ay,
                          pkt.imu[i].gx, pkt.imu[i].gy, pkt.imu[i].gz,
                          pkt.imu[i].sim_time);
        }

        /* Baro okumasını at */
        phase_ml_push_baro(pkt.altitude, pkt.baro_sim_time);

        /* Faz → bit flag dönüşümü ve response */
        uint8_t  phase  = phase_ml_get_phase();
        uint16_t status = PHASE_TO_STATUS[phase];
        send_response(pkt.baro_sim_time, pkt.altitude, status);
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
