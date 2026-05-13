#include "bme280.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include <stdint.h>

HAL_StatusTypeDef bme280_init(I2C_HandleTypeDef *hi2c, bme280_calib_t *calib)
{
    uint8_t id;
    HAL_StatusTypeDef ret;

    ret = HAL_I2C_Mem_Read(hi2c, BME280_I2C_ADDR, BME280_REG_ID,
                           I2C_MEMADD_SIZE_8BIT, &id, 1, 50);
    if (ret != HAL_OK || id != BME280_CHIP_ID) return HAL_ERROR;

    uint8_t rst = BME280_SOFT_RESET;
    HAL_I2C_Mem_Write(hi2c, BME280_I2C_ADDR, BME280_REG_RESET,
                      I2C_MEMADD_SIZE_8BIT, &rst, 1, 50);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* T/P calibration: 24 bytes at 0x88 */
    uint8_t buf[24];
    ret = HAL_I2C_Mem_Read(hi2c, BME280_I2C_ADDR, BME280_REG_CALIB_TP,
                           I2C_MEMADD_SIZE_8BIT, buf, 24, 100);
    if (ret != HAL_OK) return ret;

    calib->dig_T1 = (uint16_t)(buf[0]  | ((uint16_t)buf[1]  << 8));
    calib->dig_T2 = (int16_t) (buf[2]  | ((int16_t) buf[3]  << 8));
    calib->dig_T3 = (int16_t) (buf[4]  | ((int16_t) buf[5]  << 8));
    calib->dig_P1 = (uint16_t)(buf[6]  | ((uint16_t)buf[7]  << 8));
    calib->dig_P2 = (int16_t) (buf[8]  | ((int16_t) buf[9]  << 8));
    calib->dig_P3 = (int16_t) (buf[10] | ((int16_t) buf[11] << 8));
    calib->dig_P4 = (int16_t) (buf[12] | ((int16_t) buf[13] << 8));
    calib->dig_P5 = (int16_t) (buf[14] | ((int16_t) buf[15] << 8));
    calib->dig_P6 = (int16_t) (buf[16] | ((int16_t) buf[17] << 8));
    calib->dig_P7 = (int16_t) (buf[18] | ((int16_t) buf[19] << 8));
    calib->dig_P8 = (int16_t) (buf[20] | ((int16_t) buf[21] << 8));
    calib->dig_P9 = (int16_t) (buf[22] | ((int16_t) buf[23] << 8));

    /* H1: 1 byte at 0xA1 */
    ret = HAL_I2C_Mem_Read(hi2c, BME280_I2C_ADDR, BME280_REG_CALIB_H1,
                           I2C_MEMADD_SIZE_8BIT, &calib->dig_H1, 1, 50);
    if (ret != HAL_OK) return ret;

    /* H2-H6: 7 bytes at 0xE1 */
    uint8_t hbuf[7];
    ret = HAL_I2C_Mem_Read(hi2c, BME280_I2C_ADDR, BME280_REG_CALIB_H2,
                           I2C_MEMADD_SIZE_8BIT, hbuf, 7, 50);
    if (ret != HAL_OK) return ret;

    calib->dig_H2 = (int16_t)(hbuf[0] | ((int16_t)hbuf[1] << 8));
    calib->dig_H3 = hbuf[2];
    calib->dig_H4 = (int16_t)((int16_t)hbuf[3] << 4) | (hbuf[4] & 0x0F);
    calib->dig_H5 = (int16_t)((int16_t)hbuf[5] << 4) | (hbuf[4] >> 4);
    calib->dig_H6 = (int8_t)hbuf[6];

    return HAL_OK;
}

HAL_StatusTypeDef bme280_config(I2C_HandleTypeDef *hi2c)
{
    uint8_t v;
    HAL_StatusTypeDef ret;

    /* ctrl_hum = osrs_h x1 — must write BEFORE ctrl_meas (datasheet req.) */
    v = 0x01;
    ret = HAL_I2C_Mem_Write(hi2c, BME280_I2C_ADDR, BME280_REG_CTRL_HUM,
                            I2C_MEMADD_SIZE_8BIT, &v, 1, 50);
    if (ret != HAL_OK) return ret;

    /* config: t_standby=0.5ms, IIR filter=off */
    v = 0x00;
    ret = HAL_I2C_Mem_Write(hi2c, BME280_I2C_ADDR, BME280_REG_CONFIG,
                            I2C_MEMADD_SIZE_8BIT, &v, 1, 50);
    if (ret != HAL_OK) return ret;

    /* ctrl_meas: osrs_t=x1, osrs_p=x2, mode=normal
     *   bits[7:5]=001, bits[4:2]=010, bits[1:0]=11 → 0x2B */
    v = 0x2B;
    ret = HAL_I2C_Mem_Write(hi2c, BME280_I2C_ADDR, BME280_REG_CTRL_MEAS,
                            I2C_MEMADD_SIZE_8BIT, &v, 1, 50);
    return ret;
}

HAL_StatusTypeDef bme280_read(I2C_HandleTypeDef *hi2c, uint8_t *buf)
{
    return HAL_I2C_Mem_Read(hi2c, BME280_I2C_ADDR, BME280_REG_PRESS_MSB,
                            I2C_MEMADD_SIZE_8BIT, buf, 8, 20);
}

void bme280_parse(const uint8_t *buf, const bme280_calib_t *calib, bme280_data_t *out)
{
    int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
    int32_t adc_H = ((int32_t)buf[6] << 8)  |  buf[7];

    /* Temperature — Bosch 64-bit integer compensation (datasheet 4.2.3) */
    int32_t v1 = (((adc_T >> 3) - ((int32_t)calib->dig_T1 << 1)) *
                   (int32_t)calib->dig_T2) >> 11;
    int32_t v2 = (((((adc_T >> 4) - (int32_t)calib->dig_T1) *
                    ((adc_T >> 4) - (int32_t)calib->dig_T1)) >> 12) *
                   (int32_t)calib->dig_T3) >> 14;
    int32_t t_fine = v1 + v2;
    out->temperature = (float)((t_fine * 5 + 128) >> 8) / 100.0f;

    /* Pressure */
    int64_t p1 = (int64_t)t_fine - 128000;
    int64_t p2 = p1 * p1 * (int64_t)calib->dig_P6;
    p2 += (p1 * (int64_t)calib->dig_P5) << 17;
    p2 += (int64_t)calib->dig_P4 << 35;
    p1  = ((p1 * p1 * (int64_t)calib->dig_P3) >> 8) +
          ((p1 * (int64_t)calib->dig_P2) << 12);
    p1  = ((INT64_C(1) << 47) + p1) * (int64_t)calib->dig_P1 >> 33;

    if (p1 == 0) {
        out->pressure = 0.0f;
    } else {
        int64_t p = 1048576 - adc_P;
        p = (((p << 31) - p2) * 3125) / p1;
        p1 = ((int64_t)calib->dig_P9 * (p >> 13) * (p >> 13)) >> 25;
        p2 = ((int64_t)calib->dig_P8 * p) >> 19;
        p  = ((p + p1 + p2) >> 8) + ((int64_t)calib->dig_P7 << 4);
        /* p is in Q24.8 Pa units → divide by 256 for Pa, /100 for hPa */
        out->pressure = (float)p / 25600.0f;
    }

    /* Humidity */
    int32_t h = t_fine - 76800;
    h = (((((adc_H << 14) - ((int32_t)calib->dig_H4 << 20) -
             ((int32_t)calib->dig_H5 * h)) + 16384) >> 15) *
         (((((((h * (int32_t)calib->dig_H6) >> 10) *
              (((h * (int32_t)calib->dig_H3) >> 11) + 32768)) >> 10) +
            2097152) * (int32_t)calib->dig_H2 + 8192) >> 14));
    h -= (((((h >> 15) * (h >> 15)) >> 7) * (int32_t)calib->dig_H1) >> 4);
    h  = (h < 0) ? 0 : h;
    h  = (h > 419430400) ? 419430400 : h;
    out->humidity = (float)(h >> 12) / 1024.0f;

    /* Altitude — standard atmosphere (ISA), MSL reference */
    out->altitude = 44330.0f * (1.0f - powf(out->pressure / 1013.25f,
                                             1.0f / 5.255f));
}
