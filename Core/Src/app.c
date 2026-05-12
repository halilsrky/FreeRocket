#include "app.h"
#include "main.h"
#include "cmsis_os.h"

/*
 * Tüm task'ların oluşturulduğu tek giriş noktası.
 * IMU driver eklenince ilk osThreadNew() buraya gelecek.
 */
void Application_Start(void)
{
}