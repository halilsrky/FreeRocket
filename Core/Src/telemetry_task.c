#include "telemetry_task.h"
#include "imu_snapshot.h"
#include "baro_snapshot.h"
#include "alt_snapshot.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <stdint.h>

/*
 * Binary telemetry frame — 50 bytes.
 * addDataPacketNormal formatıyla birebir uyumlu (packet.c, old_project).
 *
 * Python decoder:
 *   import struct
 *   magic, alt, gps_alt, lat, lon, theta, az, gz, temp, press, mag, vel, hum, \
 *       reserved, csum, cr, lf = struct.unpack('<B 11f BB BBB', data)
 *
 * Offset map:
 *   [0]      0xFF  magic
 *   [1-4]    baro altitude MSL (m)
 *   [5-8]    GPS altitude      → 0
 *   [9-12]   GPS latitude      → 0
 *   [13-16]  GPS longitude     → 0
 *   [17-20]  pitch (°)
 *   [21-24]  accel Z (m/s²)
 *   [25-28]  gyro Z (rad/s)
 *   [29-32]  temperature (°C)
 *   [33-36]  pressure (hPa)
 *   [37-40]  magnetic field    → 0
 *   [41-44]  vertical velocity (m/s, Kalman)
 *   [45]     humidity (%RH, uint8)
 *   [46]     0x00 reserved
 *   [47]     checksum (additive sum of [0..46] mod 256)
 *   [48]     0x0D
 *   [49]     0x0A
 */

#define TELEM_RATE_HZ   50U
#define TELEM_PERIOD_MS (1000U / TELEM_RATE_HZ)

#define FRAME_LEN       50U
#define CHKSUM_SPAN     47U   /* bytes [0..46] dahil */

typedef union {
    float   f;
    uint8_t b[4];
} f32_u8_t;

static TaskHandle_t s_handle;
static uint8_t      s_frame[FRAME_LEN];  /* static: DMA frame gönderilirken yaşıyor */

static void put_float(uint8_t *dst, float v)
{
    f32_u8_t u = { .f = v };
    dst[0] = u.b[0];
    dst[1] = u.b[1];
    dst[2] = u.b[2];
    dst[3] = u.b[3];
}

static uint8_t checksum(const uint8_t *buf, uint16_t len)
{
    uint16_t sum = 0;
    for (uint16_t i = 0; i < len; i++) sum += buf[i];
    return (uint8_t)(sum % 256U);
}

static void telem_task(void *arg)
{
    (void)arg;
    s_handle = xTaskGetCurrentTaskHandle();

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TELEM_PERIOD_MS));

        imu_snapshot_t  imu  = {0};
        baro_snapshot_t baro = {0};
        alt_snapshot_t  alt  = {0};

        bool have_imu  = imu_snapshot_peek(&imu);
        bool have_baro = baro_snapshot_peek(&baro);
        bool have_alt  = alt_snapshot_peek(&alt);

        /* En az baro olmadan frame göndermek anlamsız */
        if (!have_baro) continue;

        s_frame[0] = 0xFF;

        put_float(&s_frame[1],  baro.altitude);
        put_float(&s_frame[5],  0.0f);                              /* GPS alt  */
        put_float(&s_frame[9],  0.0f);                              /* lat      */
        put_float(&s_frame[13], 0.0f);                              /* lon      */
        put_float(&s_frame[17], have_imu ? imu.euler.pitch : 0.0f);
        put_float(&s_frame[21], have_imu ? imu.accel.z     : 0.0f);
        put_float(&s_frame[25], have_imu ? imu.gyro.z      : 0.0f);
        put_float(&s_frame[29], baro.temperature);
        put_float(&s_frame[33], baro.pressure);
        put_float(&s_frame[37], 0.0f);                              /* magnetic */
        put_float(&s_frame[41], have_alt ? alt.velocity     : 0.0f);

        s_frame[45] = (uint8_t)baro.humidity;
        s_frame[46] = 0x00;
        s_frame[47] = checksum(s_frame, CHKSUM_SPAN);
        s_frame[48] = 0x0D;
        s_frame[49] = 0x0A;

        HAL_UART_Transmit_DMA(&huart2, s_frame, FRAME_LEN);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2 || s_handle == NULL) return;

    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_handle, 1U, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

void telemetry_task_create(void)
{
    static const osThreadAttr_t attr = {
        .name       = "Telem",
        .stack_size = 256 * 4,
        .priority   = osPriorityBelowNormal,
    };
    osThreadNew(telem_task, NULL, &attr);
}
