/*
 * app.h — Application layer entry point.
 *
 * Cube-generated main.c handles HAL/peripheral init, then calls
 * Application_Start() (added in USER CODE 2 block) which spins up MiROS
 * and never returns. All task definitions, event masks, and HAL
 * callback overrides live in app.c — main.c stays untouched so CubeMX
 * regeneration is safe.
 */
#ifndef APP_H
#define APP_H

#include "miros.h"

#define APP_TICKS_PER_SEC   100U

/* Event bits for imuThread */
#define EVT_ACCEL_DRDY      (1U << 0)
#define EVT_GYRO_DRDY       (1U << 1)
#define EVT_DMA_DONE        (1U << 2)
#define EVT_I2C_ERROR       (1U << 3)

void Application_Start(void);

#endif /* APP_H */
