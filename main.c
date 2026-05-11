#include <stdint.h>
#include "miros.h"
#include "bsp.h"
#include "i2c1.h"
#include "bmi088.h"

/* ---- Event masks for imuTask ------------------------------------------- */
#define EVT_BMI_ACCEL_DRDY  (1U << 0)
#define EVT_BMI_GYRO_DRDY   (1U << 1)

/* ---- imuTask ----------------------------------------------------------- */

uint32_t stack_imu[80];
OSThread imuThread;

static void main_imuThread(void)
{
    /* I2C + BMI088 init runs in task context so it executes only after
     * the scheduler is up — keeps boot-time blocking out of main(). */
    i2c1_init();

    bmi088_config_t cfg = {
        .acc_range      = BMI088_ACC_RANGE_24G,
        .acc_bwp        = BMI088_ACC_BWP_NORMAL,
        .acc_odr        = BMI088_ACC_ODR_400,
        .gyro_range     = BMI088_GYRO_RANGE_2000_DPS,
        .gyro_bandwidth = BMI088_GYRO_BW_47,
    };

    if (bmi088_init(&cfg) != BMI088_OK) {
        /* Init failed — park the thread; LED stays whatever the lower-prio
         * blinky leaves it. Replace with proper error handling later. */
        while (1) { OS_delay(BSP_TICKS_PER_SEC); }
    }

    /* Phase 1: react to DRDY events by toggling the on-board LED.
     * Phase 2 will replace this with: start I2C DMA read -> wait DMA TC
     * event -> parse raw bytes -> Mahony update.
     */
    while (1) {
        uint32_t evt = OS_evtWait(EVT_BMI_ACCEL_DRDY | EVT_BMI_GYRO_DRDY);
        if (evt == EVT_BMI_ACCEL_DRDY) {
            BSP_ledGreenOn();
        } else {
            BSP_ledGreenOff();
        }
    }
}

/* ---- Existing blinky kept as a low-priority sanity load --------------- */

uint32_t stack_blinky1[40];
OSThread blinky1;
static void main_blinky1(void) {
    while (1) {
        BSP_ledGreenOn();
        OS_delay(BSP_TICKS_PER_SEC / 4U);
        BSP_ledGreenOff();
        OS_delay(BSP_TICKS_PER_SEC * 3U / 4U);
    }
}

uint32_t stack_idleThread[40];

int main(void) {
    SystemClock_Config();
    BSP_init();
    OS_init(stack_idleThread, sizeof(stack_idleThread));

    /* Bind imuTask to the BMI088 driver before starting it, so any DRDY
     * spike that races init has a valid POST target. */
    bmi088_attach_thread(&imuThread, EVT_BMI_ACCEL_DRDY, EVT_BMI_GYRO_DRDY);

    /* imuTask: highest priority of the user threads */
    OSThread_start(&imuThread, 5U, &main_imuThread,
                   stack_imu, sizeof(stack_imu));

    /* Low-priority blinky as a visual scheduler heartbeat */
    OSThread_start(&blinky1, 1U, &main_blinky1,
                   stack_blinky1, sizeof(stack_blinky1));

    OS_run();
}
