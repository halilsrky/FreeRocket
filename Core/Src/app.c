#include "app.h"
#include "main.h"
#include "imu_task.h"
#include "baro_task.h"
#include "telemetry_task.h"
#include "gnss_task.h"
#include "cmd_task.h"
#include "sut_task.h"
#include "sys_mode.h"
#include "cmsis_os.h"
#include "SEGGER_SYSVIEW.h"

void Application_Start(void)
{
    SEGGER_SYSVIEW_Conf();

    sys_mode_init();

    sut_task_create();      /* queue oluşturulur — cmd_task'tan önce */
    imu_task_create();
    baro_task_create();
    gnss_task_create();
    telemetry_task_create();
    cmd_task_create();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    NVIC_SystemReset();
}
