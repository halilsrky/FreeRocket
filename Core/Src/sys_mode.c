#include "sys_mode.h"
#include "FreeRTOS.h"
#include "queue.h"

static volatile SystemMode_t s_mode = MODE_NORMAL;
static QueueHandle_t         s_sut_q;

void sys_mode_init(void)
{
    s_mode  = MODE_NORMAL;
    s_sut_q = xQueueCreate(1, sizeof(sut_data_t));
}

SystemMode_t sys_mode_get(void)
{
    return s_mode;
}

void sys_mode_set(SystemMode_t mode)
{
    s_mode = mode;
}

bool sys_mode_sut_peek(sut_data_t *out)
{
    return xQueuePeek(s_sut_q, out, 0) == pdTRUE;
}

void sys_mode_sut_put(const sut_data_t *data)
{
    xQueueOverwrite(s_sut_q, data);
}
