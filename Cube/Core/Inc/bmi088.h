/*
 * bmi088.h — BMI088 IMU driver, HAL + MiROS port.
 *
 * Transport: STM32 HAL I2C (polled for init, DMA for hot path).
 * Threading: this driver doesn't post events itself — the app layer
 *   owns HAL_GPIO_EXTI_Callback / HAL_I2C_MemRxCpltCallback and dispatches
 *   events to the imuThread. The driver just provides:
 *     - bmi088_init(): boot-time polled setup
 *     - bmi088_read_accel_dma() / bmi088_read_gyro_dma(): non-blocking
 *       6-byte reads launched via HAL_I2C_Mem_Read_DMA
 *     - bmi088_parse_*(): raw bytes -> physical units
 *
 * Wiring assumptions (.ioc):
 *   I2C1: PB8/PB9, DMA1 Stream0 Ch1 for RX
 *   ACC INT1  -> PB3 (EXTI3 rising)
 *   GYRO INT3 -> PB4 (EXTI4 rising)
 */
#ifndef BMI088_H
#define BMI088_H

#include "stm32f4xx_hal.h"
#include "bmi088_regs.h"

typedef enum {
    BMI088_OK            = 0,
    BMI088_ERR_ACC_ID    = 1,
    BMI088_ERR_GYRO_ID   = 2,
    BMI088_ERR_I2C       = 3,
    BMI088_ERR_ARG       = 4,
} bmi088_status_t;

typedef struct {
    uint8_t acc_range;       /* BMI088_ACC_RANGE_* */
    uint8_t acc_bwp;         /* BMI088_ACC_BWP_*   */
    uint8_t acc_odr;         /* BMI088_ACC_ODR_*   */
    uint8_t gyro_range;      /* BMI088_GYRO_RANGE_*_DPS */
    uint8_t gyro_bandwidth;  /* BMI088_GYRO_BW_*   */
} bmi088_config_t;

typedef struct {
    float x;
    float y;
    float z;
} bmi088_vec3_t;

/* Bind the HAL I2C handle. Must be called before any other driver call. */
void              bmi088_bind(I2C_HandleTypeDef *hi2c);

/* Read both chip IDs over I2C (polled). For board bring-up. */
bmi088_status_t   bmi088_probe(uint8_t *out_acc_id, uint8_t *out_gyro_id);

/* Polled init: soft-reset, range/ODR/bandwidth, INT pin routing.
 * Uses OS_delay for the datasheet-mandated settle times — must be
 * called from a task context (not from main() before OS_run). */
bmi088_status_t   bmi088_init(const bmi088_config_t *cfg);

/* Launch a 6-byte DMA read of the accel data registers (X_LSB..Z_MSB).
 * Returns HAL_OK if the transfer was kicked off. Completion fires
 * HAL_I2C_MemRxCpltCallback in the I2C IRQ context.
 */
HAL_StatusTypeDef bmi088_read_accel_dma(uint8_t *raw6);

/* Same for gyro (RATE_X_LSB..RATE_Z_MSB). */
HAL_StatusTypeDef bmi088_read_gyro_dma(uint8_t *raw6);

/* Decode raw bytes into physical units using the ranges captured by
 * the last bmi088_init() call.
 *   accel: g  (1 g = 9.80665 m/s^2)
 *   gyro : deg/s
 */
void              bmi088_parse_accel(const uint8_t *raw6, bmi088_vec3_t *out_g);
void              bmi088_parse_gyro (const uint8_t *raw6, bmi088_vec3_t *out_dps);

#endif /* BMI088_H */
