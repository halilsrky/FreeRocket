/*
 * bmi088.h — BMI088 IMU driver (mini RTOS port).
 *
 * Phase 1 scope:
 *   - Chip ID probe + reset + config (polled I2C, boot-time only)
 *   - GPIO + EXTI setup for the accel & gyro DRDY pins (PB3, PB4)
 *   - EXTI ISRs that POST events to a registered mini RTOS thread
 *
 * Phase 2 (future): DMA-driven raw read (DMA1 Stream0 Ch1), Mahony
 * orientation update, sample timestamping.
 *
 * Wiring assumptions (matches SKYRTOS):
 *   I2C: PB8/PB9 via i2c1.c
 *   ACC INT1  -> PB3 -> EXTI3 (rising edge, no pull)
 *   GYRO INT3 -> PB4 -> EXTI4 (rising edge, no pull)
 */
#ifndef BMI088_H
#define BMI088_H

#include <stdint.h>
#include "miros.h"
#include "bmi088_regs.h"

typedef enum {
    BMI088_OK            = 0,
    BMI088_ERR_ACC_ID    = 1,  /* accel chip ID mismatch */
    BMI088_ERR_GYRO_ID   = 2,  /* gyro chip ID mismatch */
    BMI088_ERR_I2C       = 3,  /* underlying I2C transfer failed */
} bmi088_status_t;

typedef struct {
    uint8_t acc_range;       /* BMI088_ACC_RANGE_* */
    uint8_t acc_bwp;         /* BMI088_ACC_BWP_*   */
    uint8_t acc_odr;         /* BMI088_ACC_ODR_*   */
    uint8_t gyro_range;      /* BMI088_GYRO_RANGE_*_DPS */
    uint8_t gyro_bandwidth;  /* BMI088_GYRO_BW_*   */
} bmi088_config_t;

/* Bind a thread + event mask pair so the EXTI ISRs know what to POST.
 * Both DRDY events go to the same thread (typically imuTask) but get
 * distinct mask bits so the thread can tell which sensor fired.
 *
 * Must be called before enabling EXTI interrupts (i.e. before bmi088_init).
 */
void            bmi088_attach_thread(OSThread *t,
                                     uint32_t accel_drdy_evt,
                                     uint32_t gyro_drdy_evt);

/* Probe both chip IDs over I2C. Useful for board bring-up before
 * committing to a full init. Does not modify peripheral state.
 */
bmi088_status_t bmi088_probe(uint8_t *out_acc_id, uint8_t *out_gyro_id);

/* Full initialization sequence: soft reset, range/ODR/bandwidth config,
 * INT pin routing, EXTI configuration. After this call returns OK the
 * sensor will start raising DRDY interrupts at the configured ODR.
 */
bmi088_status_t bmi088_init(const bmi088_config_t *cfg);

/* ISR entry points — invoked by the EXTI3 / EXTI4 vectors in bsp.c.
 * Each clears the corresponding EXTI pending bit and POSTs the
 * registered event to the attached thread.
 */
void            bmi088_exti_accel_isr(void);
void            bmi088_exti_gyro_isr(void);

#endif /* BMI088_H */
