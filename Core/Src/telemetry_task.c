#include "telemetry_task.h"
#include "imu_snapshot.h"
#include "baro_snapshot.h"
#include "alt_snapshot.h"
#include "gnss_snapshot.h"
#include "flight_sm.h"
#include "sys_mode.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <stdint.h>

/*
 * Binary telemetry frame — 54 bytes.
 *
 * Tüm floatlar big-endian.
 *
 * Python decoder:
 *   import struct
 *   magic, alt, press, ax, ay, az, pitch, roll, yaw, gps_alt, lat, lon, vel = \
 *       struct.unpack('>Bffffffffff', data[:49])
 *   status = (data[49] << 8) | data[50]
 *   csum, cr, lf = data[51], data[52], data[53]
 *
 * Offset map:
 *   [0]      0xAB magic
 *   [1-4]    Kalman altitude AGL (m,   float BE)
 *   [5-8]    pressure          (hPa, float BE)
 *   [9-12]   accel X      (m/s², float BE)
 *   [13-16]  accel Y      (m/s², float BE)
 *   [17-20]  -accel Z     (m/s², float BE, negated)
 *   [21-24]  pitch        (°,   float BE)
 *   [25-28]  roll         (°,   float BE)
 *   [29-32]  yaw          (°,   float BE)
 *   [33-36]  GPS altitude (m,   float BE)
 *   [37-40]  GPS latitude (°,   float BE)
 *   [41-44]  GPS longitude(°,   float BE)
 *   [45-48]  velocity     (m/s, float BE, Kalman)
 *   [49]     status_bits  high byte (FSM_BIT_* flags)
 *   [50]     status_bits  low byte
 *   [51]     checksum     (additive sum of [0..50] mod 256)
 *   [52]     0x0D
 *   [53]     0x0A
 */

#define TELEM_RATE_HZ   50U
#define TELEM_PERIOD_MS (1000U / TELEM_RATE_HZ)

#define FRAME_LEN       54U
#define CHKSUM_SPAN     51U   /* bytes [0..50] dahil */

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

        imu_snapshot_t    imu    = {0};
        baro_snapshot_t   baro   = {0};
        alt_snapshot_t    alt    = {0};
        gnss_snapshot_t   gnss   = {0};
        flight_snapshot_t flight = {0};
        sut_data_t        sut    = {0};

        bool have_imu    = imu_snapshot_peek(&imu);
        bool have_baro   = baro_snapshot_peek(&baro);
        bool have_alt    = alt_snapshot_peek(&alt);
        bool have_gnss   = gnss_snapshot_peek(&gnss) && gnss.is_valid;
        flight_snapshot_peek(&flight);  /* yoksa status=0 kalır */

        bool is_sut = (sys_mode_get() == MODE_SUT) && sys_mode_sut_peek(&sut);

        if (!have_baro || !have_alt) continue;

        s_frame[0] = 0xAB;

        /* [1-32] big-endian floats */
        put_float_be(&s_frame[1],  alt.altitude_rel);
        put_float_be(&s_frame[5],  baro.pressure);
        put_float_be(&s_frame[9],  is_sut ?  sut.accel_x : (have_imu ?  imu.accel.x     : 0.0f));
        put_float_be(&s_frame[13], is_sut ?  sut.accel_y : (have_imu ?  imu.accel.y     : 0.0f));
        put_float_be(&s_frame[17], is_sut ? -sut.accel_z : (have_imu ? -imu.accel.z     : 0.0f));
        put_float_be(&s_frame[21], is_sut ?  0.0f        : (have_imu ?  imu.euler.pitch : 0.0f));
        put_float_be(&s_frame[25], is_sut ?  0.0f        : (have_imu ?  imu.euler.roll  : 0.0f));
        put_float_be(&s_frame[29], is_sut ?  0.0f        : (have_imu ?  imu.euler.yaw   : 0.0f));

        /* [33-48] big-endian floats */
        put_float_be(&s_frame[33], have_gnss ? gnss.altitude  : 0.0f);
        put_float_be(&s_frame[37], have_gnss ? gnss.latitude  : 0.0f);
        put_float_be(&s_frame[41], have_gnss ? gnss.longitude : 0.0f);
        put_float_be(&s_frame[45], have_alt  ? alt.velocity   : 0.0f);

        s_frame[49] = (flight.status >> 8) & 0xFF;
        s_frame[50] =  flight.status       & 0xFF;

        s_frame[51] = checksum(s_frame, CHKSUM_SPAN);
        s_frame[52] = 0x0D;
        s_frame[53] = 0x0A;

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
