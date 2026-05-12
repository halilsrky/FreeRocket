#ifndef BMI088_H
#define BMI088_H

#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "bmi088_defs.h"

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t acc_range;   /* ACC_RANGE_3G .. ACC_RANGE_24G */
    uint8_t acc_odr;     /* ACC_ODR_50 .. ACC_ODR_1600    */
    uint8_t gyro_range;  /* GYRO_RANGE_2000DPS .. GYRO_RANGE_125DPS */
    uint8_t gyro_bw;     /* GYRO_BW_* — controls both filter BW and ODR */
} bmi088_config_t;

/*
 * Chip ID'leri doğrula. 0 = OK, bit0 = acc fail, bit1 = gyro fail.
 * Scheduler başladıktan sonra (task içinden) çağrılacak.
 */
uint8_t bmi088_init(const bmi088_config_t *cfg);

/*
 * Reset + ODR/range/interrupt ayarları.
 * HAL_Delay yerine vTaskDelay kullanır — sadece task context'ten çağır.
 */
void bmi088_config(const bmi088_config_t *cfg);

/* DMA burst başlat — callback: HAL_I2C_MemRxCpltCallback */
HAL_StatusTypeDef bmi088_start_accel_dma(I2C_HandleTypeDef *hi2c, uint8_t *buf6);
HAL_StatusTypeDef bmi088_start_gyro_dma(I2C_HandleTypeDef *hi2c, uint8_t *buf6);

/* Ham 6 byte → float SI birimleri (m/s², rad/s) */
void bmi088_parse_accel(const uint8_t *raw6, uint8_t range,
                        float *ax, float *ay, float *az);
void bmi088_parse_gyro(const uint8_t *raw6, uint8_t range,
                       float *gx, float *gy, float *gz);

#endif /* BMI088_H */
