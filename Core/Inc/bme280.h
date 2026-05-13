#ifndef INC_BME280_H_
#define INC_BME280_H_

#include "main.h"
#include <stdint.h>

/* ── Register addresses ── */
#define BME280_I2C_ADDR      (0x76 << 1)
#define BME280_REG_ID        0xD0
#define BME280_REG_RESET     0xE0
#define BME280_REG_CTRL_HUM  0xF2
#define BME280_REG_STATUS    0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG    0xF5
#define BME280_REG_PRESS_MSB 0xF7   /* 8 bytes: press(3)+temp(3)+hum(2) */
#define BME280_REG_CALIB_TP  0x88   /* 24 bytes T/P calibration */
#define BME280_REG_CALIB_H1  0xA1   /* 1 byte H1 */
#define BME280_REG_CALIB_H2  0xE1   /* 7 bytes H2-H6 */

#define BME280_CHIP_ID       0x60
#define BME280_SOFT_RESET    0xB6

/* ── Calibration data (read once at init) ── */
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t  dig_H1, dig_H3;
    int16_t  dig_H2, dig_H4, dig_H5;
    int8_t   dig_H6;
} bme280_calib_t;

/* ── Parsed output ── */
typedef struct {
    float temperature;  /* °C  */
    float pressure;     /* hPa */
    float humidity;     /* %RH */
    float altitude;     /* m (MSL, standard atmosphere) */
} bme280_data_t;

/*
 * Init: chip ID check + calibration register reads.
 * Blocking — call once from task context before starting the loop.
 */
HAL_StatusTypeDef bme280_init(I2C_HandleTypeDef *hi2c, bme280_calib_t *calib);

/*
 * Config: OSR settings + normal mode.
 * Blocking — call after init.
 */
HAL_StatusTypeDef bme280_config(I2C_HandleTypeDef *hi2c);

/*
 * Start non-blocking DMA read of 8 raw ADC bytes (0xF7–0xFE).
 * Completion triggers HAL_I2C_MemRxCpltCallback.
 */
HAL_StatusTypeDef bme280_start_read_dma(I2C_HandleTypeDef *hi2c, uint8_t *buf);

/*
 * Parse 8 raw bytes into physical values using calibration coefficients.
 * Pure function — no I2C, no state.
 */
void bme280_parse(const uint8_t *buf, const bme280_calib_t *calib, bme280_data_t *out);

#endif /* INC_BME280_H_ */
