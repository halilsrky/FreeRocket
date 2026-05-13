#include "baro_task.h"
#include "baro_snapshot.h"
#include "bme280.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdbool.h>

static QueueHandle_t s_snapshot_q;

static void baro_task(void *arg);

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

static void baro_task(void *arg)
{
    (void)arg;

    bme280_calib_t calib;

    if (bme280_init(&hi2c3, &calib) != HAL_OK) {
        vTaskDelete(NULL);
        return;
    }
    bme280_config(&hi2c3);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t raw[8];
    TickType_t wake_tick = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&wake_tick, pdMS_TO_TICKS(100));

        if (bme280_read(&hi2c3, raw) != HAL_OK) continue;

        bme280_data_t data;
        bme280_parse(raw, &calib, &data);

        baro_snapshot_t snap = {
            .ts_ms       = HAL_GetTick(),
            .temperature = data.temperature,
            .pressure    = data.pressure,
            .humidity    = data.humidity,
            .altitude    = data.altitude,
        };
        xQueueOverwrite(s_snapshot_q, &snap);
    }
}
