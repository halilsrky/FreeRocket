#include "app.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

static void heartbeat_task(void *arg);

/*
 * Tek giriş noktası. defaultTask buraya gelir, gerçek task'ları kurar.
 * Sensör driver'ları sonraki adımda buradan oluşturulacak.
 */
void Application_Start(void)
{
    static const osThreadAttr_t hb_attr = {
        .name       = "Heartbeat",
        .stack_size = 128 * 4,
        .priority   = osPriorityLow,
    };
    osThreadNew(heartbeat_task, NULL, &hb_attr);
}

/* İskelet doğrulama task'ı — sistem çökmüyor mu diye izler.
   IWDG eklenince beslemesi buraya gelecek. */
static void heartbeat_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        osDelay(500);
    }
}

/* Stack overflow tespit hook'u — sessiz çöküş yerine kontrollü reset. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    NVIC_SystemReset();
}
