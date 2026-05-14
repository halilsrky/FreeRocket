#include "telemetry_task.h"
#include "imu_snapshot.h"
#include "baro_snapshot.h"
#include "alt_snapshot.h"
#include "gnss_snapshot.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <stdint.h>

/*
 * Binary telemetry frame — 52 bytes.
 * addDataPacketSit formatıyla birebir uyumlu (packet.c, old_project).
 *
 * Tüm floatlar big-endian.
 *
 * Python decoder:
 *   import struct
 *   magic, alt, press, ax, ay, az, pitch, roll, yaw, gps_alt, lat, lon, vel = \
 *       struct.unpack('>Bffffffffff', data[:49])
 *   csum, cr, lf = data[49], data[50], data[51]
 *
 * Offset map:
 *   [0]      0xAB magic
 *   [1-4]    baro altitude (m,   float BE)
 *   [5-8]    pressure     (hPa, float BE)
 *   [9-12]   accel X      (m/s², float BE)
 *   [13-16]  accel Y      (m/s², float BE)
 *   [17-20]  -accel Z     (m/s², float BE, negated)
 *   [21-24]  pitch        (°,   float BE)
 *   [25-28]  roll         (°,   float BE)
 *   [29-32]  yaw          (°,   float BE)
 *   [33-36]  GPS altitude (m,   float LE)
 *   [37-40]  GPS latitude (°,   float LE)
 *   [41-44]  GPS longitude(°,   float LE)
 *   [45-48]  velocity     (m/s, float LE, Kalman)
 *   [49]     checksum     (additive sum of [0..48] mod 256)
 *   [50]     0x0D
 *   [51]     0x0A
 */

#define TELEM_RATE_HZ   50U
#define TELEM_PERIOD_MS (1000U / TELEM_RATE_HZ)

#define FRAME_LEN       52U
#define CHKSUM_SPAN     49U   /* bytes [0..48] dahil */

typedef union {
    float   f;
    uint8_t b[4];
} f32_u8_t;

static TaskHandle_t s_handle;
static uint8_t      s_frame[FRAME_LEN];  /* static: DMA frame gönderilirken yaşıyor */

/* Big-endian: array[3], [2], [1], [0] — addDataPacketSit ile aynı */
static void put_float_be(uint8_t *dst, float v)
{
    f32_u8_t u = { .f = v };
    dst[0] = u.b[3];
    dst[1] = u.b[2];
    dst[2] = u.b[1];
    dst[3] = u.b[0];
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
        gnss_snapshot_t gnss = {0};

        bool have_imu  = imu_snapshot_peek(&imu);
        bool have_baro = baro_snapshot_peek(&baro);
        bool have_alt  = alt_snapshot_peek(&alt);
        bool have_gnss = gnss_snapshot_peek(&gnss) && gnss.is_valid;

        if (!have_baro) continue;

        s_frame[0] = 0xAB;

        /* [1-32] big-endian floats */
        put_float_be(&s_frame[1],  baro.altitude);
        put_float_be(&s_frame[5],  baro.pressure);
        put_float_be(&s_frame[9],  have_imu ?  imu.accel.x    : 0.0f);
        put_float_be(&s_frame[13], have_imu ?  imu.accel.y    : 0.0f);
        put_float_be(&s_frame[17], have_imu ? -imu.accel.z    : 0.0f);  /* negated */
        put_float_be(&s_frame[21], have_imu ?  imu.euler.pitch : 0.0f);
        put_float_be(&s_frame[25], have_imu ?  imu.euler.roll  : 0.0f);
        put_float_be(&s_frame[29], have_imu ?  imu.euler.yaw   : 0.0f);

        /* [33-48] big-endian floats */
        put_float_be(&s_frame[33], have_gnss ? gnss.altitude  : 0.0f);
        put_float_be(&s_frame[37], have_gnss ? gnss.latitude  : 0.0f);
        put_float_be(&s_frame[41], have_gnss ? gnss.longitude : 0.0f);
        put_float_be(&s_frame[45], have_alt  ? alt.velocity   : 0.0f);

        s_frame[49] = checksum(s_frame, CHKSUM_SPAN);
        s_frame[50] = 0x0D;
        s_frame[51] = 0x0A;

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
