#include "bmi088.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>

/* ── Full-scale lookup tables ── */

/* Accel: milli-g full-scale for range 0..3 (3G, 6G, 12G, 24G) */
static const float k_acc_fs_mg[4] = { 3000.0f, 6000.0f, 12000.0f, 24000.0f };

/* Gyro: deg/s full-scale for range 0..4 (2000, 1000, 500, 250, 125) */
static const float k_gyro_fs_dps[5] = { 2000.0f, 1000.0f, 500.0f, 250.0f, 125.0f };

/* ── Helpers ── */

static HAL_StatusTypeDef write_reg(I2C_HandleTypeDef *hi2c,
                                   uint16_t dev_addr, uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(hi2c, dev_addr, reg,
                             I2C_MEMADD_SIZE_8BIT, &val, 1, 20);
}

static HAL_StatusTypeDef read_reg(I2C_HandleTypeDef *hi2c,
                                  uint16_t dev_addr, uint8_t reg, uint8_t *out)
{
    return HAL_I2C_Mem_Read(hi2c, dev_addr, reg,
                            I2C_MEMADD_SIZE_8BIT, out, 1, 50);
}

/* ── Public API ── */

uint8_t bmi088_init(const bmi088_config_t *cfg)
{
    uint8_t result = 0;
    uint8_t id = 0;

    /* ACC: dummy read required on first I2C access (SPI→I2C mode switch) */
    read_reg(cfg->hi2c, BMI088_ACC_I2C_ADDR, ACC_CHIP_ID, &id);
    read_reg(cfg->hi2c, BMI088_ACC_I2C_ADDR, ACC_CHIP_ID, &id);
    if (id != ACC_CHIP_ID_VAL) result |= 0x01;

    read_reg(cfg->hi2c, BMI088_GYRO_I2C_ADDR, GYRO_CHIP_ID, &id);
    if (id != GYRO_CHIP_ID_VAL) result |= 0x02;

    return result;
}

void bmi088_config(const bmi088_config_t *cfg)
{
    I2C_HandleTypeDef *hi2c = cfg->hi2c;

    /* ── Gyro configuration ── */
    write_reg(hi2c, BMI088_GYRO_I2C_ADDR, GYRO_SOFT_RESET, GYRO_RESET_CMD);
    vTaskDelay(pdMS_TO_TICKS(30));

    write_reg(hi2c, BMI088_GYRO_I2C_ADDR, GYRO_RANGE,     cfg->gyro_range);
    write_reg(hi2c, BMI088_GYRO_I2C_ADDR, GYRO_BANDWIDTH,  cfg->gyro_bw);
    write_reg(hi2c, BMI088_GYRO_I2C_ADDR, GYRO_LPM1,       GYRO_LPM_NORMAL);
    vTaskDelay(pdMS_TO_TICKS(20));

    write_reg(hi2c, BMI088_GYRO_I2C_ADDR, GYRO_INT_CTRL,           GYRO_INT_ENABLE);
    write_reg(hi2c, BMI088_GYRO_I2C_ADDR, GYRO_INT3_INT4_IO_CONF,  GYRO_INT3_CFG);
    write_reg(hi2c, BMI088_GYRO_I2C_ADDR, GYRO_INT3_INT4_IO_MAP,   GYRO_INT_MAP_BOTH);

    /* ── Accelerometer configuration ── */
    write_reg(hi2c, BMI088_ACC_I2C_ADDR, ACC_SOFTRESET, ACC_RESET_CMD);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Dummy read after reset to re-enter I2C mode */
    uint8_t dummy;
    read_reg(hi2c, BMI088_ACC_I2C_ADDR, ACC_CHIP_ID, &dummy);

    write_reg(hi2c, BMI088_ACC_I2C_ADDR, ACC_PWR_CTRL, ACC_ENABLE);
    vTaskDelay(pdMS_TO_TICKS(10));

    write_reg(hi2c, BMI088_ACC_I2C_ADDR, ACC_PWR_CONF, ACC_PWR_ACTIVE);
    vTaskDelay(pdMS_TO_TICKS(5));

    write_reg(hi2c, BMI088_ACC_I2C_ADDR, ACC_CONF,
              (uint8_t)((ACC_BWP_NORMAL << 4) | cfg->acc_odr));
    write_reg(hi2c, BMI088_ACC_I2C_ADDR, ACC_RANGE, cfg->acc_range);

    write_reg(hi2c, BMI088_ACC_I2C_ADDR, ACC_INT1_IO_CTRL, ACC_INT1_CFG);
    write_reg(hi2c, BMI088_ACC_I2C_ADDR, ACC_INT_MAP_DATA,  ACC_INT_DRDY_INT1);

    vTaskDelay(pdMS_TO_TICKS(5));
}

HAL_StatusTypeDef bmi088_start_accel_dma(I2C_HandleTypeDef *hi2c, uint8_t *buf6)
{
    return HAL_I2C_Mem_Read_DMA(hi2c, BMI088_ACC_I2C_ADDR,
                                ACC_X_LSB, I2C_MEMADD_SIZE_8BIT, buf6, 6);
}

HAL_StatusTypeDef bmi088_start_gyro_dma(I2C_HandleTypeDef *hi2c, uint8_t *buf6)
{
    return HAL_I2C_Mem_Read_DMA(hi2c, BMI088_GYRO_I2C_ADDR,
                                GYRO_RATE_X_LSB, I2C_MEMADD_SIZE_8BIT, buf6, 6);
}

void bmi088_parse_accel(const uint8_t *raw6, uint8_t range,
                        float *ax, float *ay, float *az)
{
    int16_t rx = (int16_t)((raw6[1] << 8) | raw6[0]);
    int16_t ry = (int16_t)((raw6[3] << 8) | raw6[2]);
    int16_t rz = (int16_t)((raw6[5] << 8) | raw6[4]);

    float fs = k_acc_fs_mg[range & 0x03];
    float scale = fs / 32768.0f * BMI088_GRAVITY / 1000.0f;

    *ax = (float)rx * scale;
    *ay = (float)ry * scale;
    *az = (float)rz * scale;
}

void bmi088_parse_gyro(const uint8_t *raw6, uint8_t range,
                       float *gx, float *gy, float *gz)
{
    int16_t rx = (int16_t)((raw6[1] << 8) | raw6[0]);
    int16_t ry = (int16_t)((raw6[3] << 8) | raw6[2]);
    int16_t rz = (int16_t)((raw6[5] << 8) | raw6[4]);

    float fs = k_gyro_fs_dps[range > 4 ? 0 : range];
    float scale = fs / 32767.0f * BMI088_DEG_TO_RAD;

    *gx = (float)rx * scale;
    *gy = (float)ry * scale;
    *gz = (float)rz * scale;
}
