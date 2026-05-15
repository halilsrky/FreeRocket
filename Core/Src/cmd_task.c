/*
 * cmd_task.c — USART2 circular DMA RX, 230400 baud.
 *
 * Paket tipleri:
 *   CMD         ( 5 byte): 0xAA | cmd | chk | 0x0D | 0x0A
 *   SUT_COMBINED (variable): 0xAD | count(1) | count×[sim_t+gx+gy+gz](16)
 *                                 | ax+ay+az(12) | alt+press+baro_t(12)
 *                                 | chk | 0x0D | 0x0A
 *                            count ≤ SUT_BATCH_MAX → max ~349 byte
 *
 * Checksum: paketin ilk (size-3) byte'ının toplamı mod 256.
 *
 * DMA: circular mod, IDLE interrupt ile paket tespiti.
 * IDLE ISR → cmd_task_uart_idle_isr() → task notify → parse_packets()
 */

#include "cmd_task.h"
#include "sut_task.h"
#include "sys_mode.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <string.h>

extern DMA_HandleTypeDef hdma_usart2_rx;

/* ── Protokol sabitleri ── */
#define CMD_HEADER          0xAAu
#define SUT_COMBINED_HEADER 0xADu
#define FOOTER1             0x0Du
#define FOOTER2             0x0Au

#define CMD_SIT  0x20u
#define CMD_SUT  0x22u
#define CMD_STOP 0x24u

/*
 * SUT_COMBINED paket boyutu:
 *   2 (header+count) + count×16 (IMU) + 12 (accel) + 12 (baro) + 3 (chk+footer)
 *   = count×16 + 29
 *   count=20 → 349 byte (< 512 buffer)
 */
#define SUT_PKT_FIXED  29u   /* count=0 varsayılan minimum */
#define SUT_PKT_MIN    (SUT_PKT_FIXED + 16u)   /* en az 1 IMU örneği */

/* ── Circular DMA buffer ── */
#define RX_BUF_LEN  512u
#define TMP_BUF_LEN 512u

static uint8_t      s_rx_buf[RX_BUF_LEN];
static uint16_t     s_last_pos;
static TaskHandle_t s_handle;

/* PC big-endian (MSB önce) → ARM little-endian float */
static float b2f(const uint8_t *b)
{
    union { float f; uint8_t b[4]; } u;
    u.b[3] = b[0]; u.b[2] = b[1]; u.b[1] = b[2]; u.b[0] = b[3];
    return u.f;
}

static void parse_packets(const uint8_t *buf, uint16_t len)
{
    uint16_t pos = 0u;

    while (pos < len) {
        const uint8_t  hdr       = buf[pos];
        const uint16_t remaining = len - pos;

        /* ── CMD (5 byte) ── */
        if (hdr == CMD_HEADER && remaining >= 5u) {
            if (buf[pos + 3u] == FOOTER1 && buf[pos + 4u] == FOOTER2) {
                uint8_t csum = (uint8_t)((hdr + buf[pos + 1u]) % 256u);
                if (csum == buf[pos + 2u]) {
                    switch (buf[pos + 1u]) {
                        case CMD_SIT:  sys_mode_set(MODE_SIT);    break;
                        case CMD_SUT:  sys_mode_set(MODE_SUT);    break;
                        case CMD_STOP: sys_mode_set(MODE_NORMAL); break;
                        default: break;
                    }
                }
                pos += 5u;
                continue;
            }
        }

        /* ── SUT_COMBINED (variable) ────────────────────────────────────
         * Format: 0xAD | count(1) | count×16 IMU | 12 accel | 12 baro
         *              | chk(1) | 0x0D | 0x0A
         * Toplam: count×16 + 29
         */
        if (hdr == SUT_COMBINED_HEADER && remaining >= SUT_PKT_MIN) {
            uint8_t cnt = buf[pos + 1u];
            if (cnt == 0u || cnt > SUT_BATCH_MAX) { pos++; continue; }

            uint16_t pkt_size = (uint16_t)cnt * 16u + SUT_PKT_FIXED;
            if (remaining < pkt_size) break;  /* parçalı paket — bekle */

            if (buf[pos + pkt_size - 2u] != FOOTER1 ||
                buf[pos + pkt_size - 1u] != FOOTER2) {
                pos++; continue;
            }

            /* Checksum: byte[0..pkt_size-4] */
            uint8_t csum = 0u;
            for (uint16_t i = 0u; i < pkt_size - 3u; i++) csum += buf[pos + i];
            if (csum != buf[pos + pkt_size - 3u]) { pos++; continue; }

            /* ── Parse ── */
            sut_packet_t pkt;
            pkt.count = cnt;

            /* IMU batch */
            for (uint8_t i = 0u; i < cnt; i++) {
                uint16_t off     = pos + 2u + (uint16_t)i * 16u;
                pkt.imu[i].sim_time = b2f(&buf[off]);
                pkt.imu[i].gx       = b2f(&buf[off + 4u]);
                pkt.imu[i].gy       = b2f(&buf[off + 8u]);
                pkt.imu[i].gz       = b2f(&buf[off + 12u]);
            }

            /* Son örnek ivmesi (flight_sm için) */
            uint16_t accel_off = pos + 2u + (uint16_t)cnt * 16u;
            pkt.last_ax = b2f(&buf[accel_off]);
            pkt.last_ay = b2f(&buf[accel_off + 4u]);
            pkt.last_az = b2f(&buf[accel_off + 8u]);

            /* Baro */
            uint16_t baro_off = accel_off + 12u;
            pkt.altitude      = b2f(&buf[baro_off]);
            pkt.pressure      = b2f(&buf[baro_off + 4u]);
            pkt.baro_sim_time = b2f(&buf[baro_off + 8u]);

            sut_task_notify_packet(&pkt);

            pos += pkt_size;
            continue;
        }

        pos++;  /* geçersiz byte — atla */
    }
}

static void cmd_task(void *arg)
{
    (void)arg;
    s_handle   = xTaskGetCurrentTaskHandle();
    s_last_pos = 0u;

    HAL_UART_Receive_DMA(&huart2, s_rx_buf, RX_BUF_LEN);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
    __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);

    static uint8_t tmp[TMP_BUF_LEN];

    for (;;) {
        uint32_t notif = 0u;
        xTaskNotifyWait(0u, UINT32_MAX, &notif, portMAX_DELAY);

        uint16_t new_pos = (uint16_t)(notif & 0xFFFFu);

        uint16_t bytes;
        if (new_pos >= s_last_pos) {
            bytes = new_pos - s_last_pos;
        } else {
            bytes = (uint16_t)(RX_BUF_LEN - s_last_pos + new_pos);
        }

        if (bytes == 0u || bytes > TMP_BUF_LEN) {
            s_last_pos = new_pos;
            continue;
        }

        for (uint16_t i = 0u; i < bytes; i++) {
            tmp[i] = s_rx_buf[(s_last_pos + i) % RX_BUF_LEN];
        }
        s_last_pos = new_pos;

        parse_packets(tmp, bytes);
    }
}

void cmd_task_uart_idle_isr(void)
{
    if (s_handle == NULL) return;

    uint16_t pos = (uint16_t)(RX_BUF_LEN -
                               (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_usart2_rx));

    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_handle, (uint32_t)pos, eSetValueWithOverwrite, &woken);
    portYIELD_FROM_ISR(woken);
}

void cmd_task_create(void)
{
    static const osThreadAttr_t attr = {
        .name       = "CMD",
        .stack_size = 256u * 4u,
        .priority   = osPriorityNormal,
    };
    osThreadNew(cmd_task, NULL, &attr);
}
