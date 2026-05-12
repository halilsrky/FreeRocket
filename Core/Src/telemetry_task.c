#include "telemetry_task.h"
#include "imu_snapshot.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

/*
 * Binary telemetry frame — 44 bytes.
 *
 * Python decoder:
 *   import struct
 *   hdr, ts, ax,ay,az, gx,gy,gz, roll,pitch,yaw, crc = \
 *       struct.unpack('<2sI 3f 3f 3f H', data)
 */

#define TELEM_RATE_HZ    50U
#define TELEM_PERIOD_MS  (1000U / TELEM_RATE_HZ)   /* 20 ms */

#define FRAME_MAGIC_0    0xAAU
#define FRAME_MAGIC_1    0x55U

#define NOTIFY_TX_DONE   (1U << 0)

typedef struct __attribute__((packed)) {
    uint8_t  magic[2];
    uint32_t ts_ms;
    float    ax, ay, az;
    float    gx, gy, gz;
    float    roll, pitch, yaw;
    uint16_t crc;           /* 16-bit additive sum: bytes [2 .. 41] */
} telem_frame_t;            /* sizeof = 44 bytes */

static TaskHandle_t  s_handle;
static telem_frame_t s_frame;   /* static: DMA accesses it after function return */

static uint16_t frame_crc(const telem_frame_t *f)
{
    uint16_t        sum = 0;
    const uint8_t  *p   = (const uint8_t *)f + sizeof(f->magic);
    const uint8_t  *end = (const uint8_t *)&f->crc;
    while (p < end) sum += *p++;
    return sum;
}

static void telem_task(void *arg)
{
    (void)arg;
    s_handle = xTaskGetCurrentTaskHandle();

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TELEM_PERIOD_MS));

        imu_snapshot_t snap;
        if (!imu_snapshot_peek(&snap)) continue;   /* veri henüz yok */

        /* Frame doldur */
        s_frame.magic[0] = FRAME_MAGIC_0;
        s_frame.magic[1] = FRAME_MAGIC_1;
        s_frame.ts_ms    = snap.ts_ms;
        s_frame.ax       = snap.accel.x;
        s_frame.ay       = snap.accel.y;
        s_frame.az       = snap.accel.z;
        s_frame.gx       = snap.gyro.x;
        s_frame.gy       = snap.gyro.y;
        s_frame.gz       = snap.gyro.z;
        s_frame.roll     = snap.euler.roll;
        s_frame.pitch    = snap.euler.pitch;
        s_frame.yaw      = snap.euler.yaw;
        s_frame.crc      = frame_crc(&s_frame);

        /* DMA TX başlat — blocking değil */
        if (HAL_UART_Transmit_DMA(&huart2, (uint8_t *)&s_frame,
                                  sizeof(s_frame)) != HAL_OK) {
            continue;   /* önceki TX henüz bitmemiş, bu döngüyü atla */
        }

        /*
         * TX tamamlanmasını bekle. Timeout = bir tam period:
         * eğer bitmezse bir sonraki döngüde HAL_BUSY döner ve atlanır.
         * Bu bekleyiş IMU task'ını hiç etkilemez.
         */
        uint32_t bits = 0;
        xTaskNotifyWait(0U, UINT32_MAX, &bits, pdMS_TO_TICKS(TELEM_PERIOD_MS));
    }
}

/* ── HAL callback (ISR context) ── */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2 || s_handle == NULL) return;

    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_handle, NOTIFY_TX_DONE, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

/* ── Public ── */

void telemetry_task_create(void)
{
    static const osThreadAttr_t attr = {
        .name       = "Telem",
        .stack_size = 256 * 4,
        .priority   = osPriorityBelowNormal,
    };
    osThreadNew(telem_task, NULL, &attr);
}
