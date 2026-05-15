#include "sys_mode.h"
#include "FreeRTOS.h"
#include "queue.h"

static volatile SystemMode_t s_mode       = MODE_NORMAL;
static QueueHandle_t         s_imu_batch_q;
static QueueHandle_t         s_baro_q;

void sys_mode_init(void)
{
    s_mode        = MODE_NORMAL;
    s_imu_batch_q = xQueueCreate(1, sizeof(sut_imu_batch_t));
    s_baro_q      = xQueueCreate(1, sizeof(sut_baro_t));
}

SystemMode_t sys_mode_get(void)
{
    return s_mode;
}

void sys_mode_set(SystemMode_t mode)
{
    s_mode = mode;
}

void sys_mode_sut_imu_batch_put(const sut_imu_batch_t *batch)
{
    xQueueOverwrite(s_imu_batch_q, batch);
}

bool sys_mode_sut_imu_batch_receive(sut_imu_batch_t *out, uint32_t timeout_ms)
{
    return xQueueReceive(s_imu_batch_q, out, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void sys_mode_sut_baro_put(const sut_baro_t *data)
{
    xQueueOverwrite(s_baro_q, data);
}

bool sys_mode_sut_baro_receive(sut_baro_t *out, uint32_t timeout_ms)
{
    return xQueueReceive(s_baro_q, out, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}
