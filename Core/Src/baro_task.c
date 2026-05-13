#include "baro_task.h"
#include "baro_snapshot.h"
#include "bme280.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdbool.h>

/* ── Notification bit masks ── */
#define NOTIFY_DMA_DONE  (1U << 0)
#define NOTIFY_I2C_ERROR (1U << 1)

/* ── Module-private state ── */
static TaskHandle_t  s_handle;
static QueueHandle_t s_snapshot_q;
static uint8_t       s_raw_buf[8];
static bme280_calib_t s_calib;

/* ── Forward declaration ── */
static void baro_task(void *arg);

/* ── Public API ── */

void baro_task_create(void)
{
    s_snapshot_q = xQueueCreate(1, sizeof(baro_snapshot_t));

    static const osThreadAttr_t attr = {
        .name       = "BARO",
        .stack_size = 256 * 4,
        .priority   = osPriorityBelowNormal,
    };
    osThreadNew(baro_task, NULL, &attr);
}

bool baro_snapshot_peek(baro_snapshot_t *out)
{
    return xQueuePeek(s_snapshot_q, out, 0) == pdTRUE;
}

/* ── ISR entry points (called from imu_task.c HAL callbacks) ── */

void baro_i2c_rx_done_from_isr(void)
{
    if (s_handle == NULL) return;

    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_handle, NOTIFY_DMA_DONE, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

void baro_i2c_error_from_isr(void)
{
    if (s_handle == NULL) return;

    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_handle, NOTIFY_I2C_ERROR, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

/* ── Task implementation ── */

static void baro_task(void *arg)
{
    (void)arg;

    s_handle = xTaskGetCurrentTaskHandle();

    /* Blocking init: chip ID check + calibration reads */
    HAL_StatusTypeDef ret = bme280_init(&hi2c3, &s_calib);
    if (ret != HAL_OK) {
        /* Sensör bulunamadı — görevi sonlandır, hata LED/UART ile işaretlenebilir */
        vTaskDelete(NULL);
        return;
    }

    bme280_config(&hi2c3);

    /* Normal mode ilk ölçümü tamamlasın */
    vTaskDelay(pdMS_TO_TICKS(100));

    TickType_t wake_tick = xTaskGetTickCount();

    for (;;) {
        /* 10 Hz — 100 ms döngü */
        vTaskDelayUntil(&wake_tick, pdMS_TO_TICKS(100));

        ret = bme280_start_read_dma(&hi2c3, s_raw_buf);
        if (ret != HAL_OK) continue;   /* bus meşgulse bir sonraki döngüde tekrar dene */

        uint32_t bits = 0;
        xTaskNotifyWait(0U, UINT32_MAX, &bits, pdMS_TO_TICKS(50));

        if (bits & NOTIFY_I2C_ERROR) continue;

        if (bits & NOTIFY_DMA_DONE) {
            bme280_data_t data;
            bme280_parse(s_raw_buf, &s_calib, &data);

            baro_snapshot_t snap = {
                .ts_ms       = HAL_GetTick(),
                .temperature = data.temperature,
                .pressure    = data.pressure,
                .humidity    = data.humidity,
                .altitude    = data.altitude,
            };
            xQueueOverwrite(s_snapshot_q, &snap);
        }
        /* timeout (bits == 0): DMA başlamış ama tamamlanmamış — bir sonraki döngüde devam */
    }
}
