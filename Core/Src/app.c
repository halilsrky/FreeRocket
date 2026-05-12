#include "app.h"
#include "main.h"
#include "imu_task.h"
#include "telemetry_task.h"
#include "cmsis_os.h"

void Application_Start(void)
{
    imu_task_create();
    telemetry_task_create();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    NVIC_SystemReset();
}
