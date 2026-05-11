/*
 * bmi088.c — BMI088 IMU driver implementation, HAL transport.
 *
 * Polled HAL_I2C_Mem_* for boot-time configuration; HAL_I2C_Mem_Read_DMA
 * for the hot path. OS_delay is used for settle times so the rest of
 * the system keeps scheduling while we wait.
 */
#include "bmi088.h"
#include "miros.h"

/* Datasheet-defined settle times (ms). OS_delay quantum depends on
 * APP_TICKS_PER_SEC (currently 100 Hz / 10 ms). Round up to 1 tick min. */
#define MS_TO_TICKS(ms)  (((ms) + 9U) / 10U)

#define POLLED_TIMEOUT_MS  100U

static I2C_HandleTypeDef *s_hi2c;
static bmi088_config_t    s_cfg;

void bmi088_bind(I2C_HandleTypeDef *hi2c)
{
    s_hi2c = hi2c;
}

/* ---- Tiny HAL wrappers, mapping status → bmi088_status_t ---------------- */

static bmi088_status_t acc_write(uint8_t reg, uint8_t val)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(s_hi2c, BMI088_ACC_I2C_ADDR,
                                             reg, I2C_MEMADD_SIZE_8BIT,
                                             &val, 1U, POLLED_TIMEOUT_MS);
    return (st == HAL_OK) ? BMI088_OK : BMI088_ERR_I2C;
}

static bmi088_status_t gyro_write(uint8_t reg, uint8_t val)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(s_hi2c, BMI088_GYRO_I2C_ADDR,
                                             reg, I2C_MEMADD_SIZE_8BIT,
                                             &val, 1U, POLLED_TIMEOUT_MS);
    return (st == HAL_OK) ? BMI088_OK : BMI088_ERR_I2C;
}

static bmi088_status_t acc_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(s_hi2c, BMI088_ACC_I2C_ADDR,
                                            reg, I2C_MEMADD_SIZE_8BIT,
                                            buf, len, POLLED_TIMEOUT_MS);
    return (st == HAL_OK) ? BMI088_OK : BMI088_ERR_I2C;
}

static bmi088_status_t gyro_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(s_hi2c, BMI088_GYRO_I2C_ADDR,
                                            reg, I2C_MEMADD_SIZE_8BIT,
                                            buf, len, POLLED_TIMEOUT_MS);
    return (st == HAL_OK) ? BMI088_OK : BMI088_ERR_I2C;
}

/* ---- Probe -------------------------------------------------------------- */

bmi088_status_t bmi088_probe(uint8_t *out_acc_id, uint8_t *out_gyro_id)
{
    uint8_t id;
    bmi088_status_t st;

    /* Datasheet quirk: first accel read after power-on can be garbage. */
    (void)acc_read(BMI088_ACC_CHIP_ID, &id, 1U);

    st = acc_read(BMI088_ACC_CHIP_ID, &id, 1U);
    if (st != BMI088_OK) return st;
    if (out_acc_id) *out_acc_id = id;
    if (id != BMI088_ACC_CHIP_ID_VAL) return BMI088_ERR_ACC_ID;

    st = gyro_read(BMI088_GYRO_CHIP_ID, &id, 1U);
    if (st != BMI088_OK) return st;
    if (out_gyro_id) *out_gyro_id = id;
    if (id != BMI088_GYRO_CHIP_ID_VAL) return BMI088_ERR_GYRO_ID;

    return BMI088_OK;
}

/* ---- Init --------------------------------------------------------------- */

bmi088_status_t bmi088_init(const bmi088_config_t *cfg)
{
    bmi088_status_t st;
    uint8_t dummy;

    if ((cfg == (const bmi088_config_t *)0) || (s_hi2c == (I2C_HandleTypeDef *)0)) {
        return BMI088_ERR_ARG;
    }
    s_cfg = *cfg;

    st = bmi088_probe((uint8_t *)0, (uint8_t *)0);
    if (st != BMI088_OK) return st;

    /* Soft-reset both halves. */
    (void)acc_write(BMI088_ACC_SOFTRESET, BMI088_ACC_RESET_CMD);
    OS_delay(MS_TO_TICKS(5U));
    (void)gyro_write(BMI088_GYRO_SOFT_RESET, BMI088_GYRO_RESET_CMD);
    OS_delay(MS_TO_TICKS(50U));

    /* Accel needs a dummy I2C read after reset before normal access. */
    (void)acc_read(BMI088_ACC_CHIP_ID, &dummy, 1U);
    OS_delay(MS_TO_TICKS(5U));

    /* ---- Accelerometer config ------------------------------------------ */
    st = acc_write(BMI088_ACC_PWR_CONF, BMI088_ACC_PWR_SAVE_ACTIVE);
    if (st != BMI088_OK) return st;
    OS_delay(MS_TO_TICKS(5U));

    st = acc_write(BMI088_ACC_PWR_CTRL, BMI088_ACC_ENABLE);
    if (st != BMI088_OK) return st;
    OS_delay(MS_TO_TICKS(50U));

    st = acc_write(BMI088_ACC_RANGE, cfg->acc_range);
    if (st != BMI088_OK) return st;

    st = acc_write(BMI088_ACC_CONF, (uint8_t)((cfg->acc_bwp << 4) | cfg->acc_odr));
    if (st != BMI088_OK) return st;

    /* INT1: push-pull, active high, output enable; map data-ready to INT1. */
    st = acc_write(BMI088_ACC_INT1_IO_CTRL,
                   BMI088_ACC_INT1_OUT_EN |
                   BMI088_ACC_INT1_PUSH_PULL |
                   BMI088_ACC_INT1_ACTIVE_HIGH);
    if (st != BMI088_OK) return st;
    st = acc_write(BMI088_ACC_INT_MAP_DATA, BMI088_ACC_INT_MAP_DRDY_INT1);
    if (st != BMI088_OK) return st;

    /* ---- Gyro config --------------------------------------------------- */
    st = gyro_write(BMI088_GYRO_RANGE, cfg->gyro_range);
    if (st != BMI088_OK) return st;

    st = gyro_write(BMI088_GYRO_BANDWIDTH, cfg->gyro_bandwidth);
    if (st != BMI088_OK) return st;

    st = gyro_write(BMI088_GYRO_LPM1, BMI088_GYRO_LPM_NORMAL);
    if (st != BMI088_OK) return st;
    OS_delay(MS_TO_TICKS(30U));

    st = gyro_write(BMI088_GYRO_INT_CTRL, BMI088_GYRO_INT_ENABLE);
    if (st != BMI088_OK) return st;

    st = gyro_write(BMI088_GYRO_INT3_4_IO_CONF,
                    BMI088_GYRO_INT3_PUSH_PULL |
                    BMI088_GYRO_INT3_ACTIVE_HIGH |
                    BMI088_GYRO_INT4_ACTIVE_HIGH);
    if (st != BMI088_OK) return st;

    /* Map DRDY to both INT3/INT4 — only one is wired to the MCU (PB4). */
    st = gyro_write(BMI088_GYRO_INT3_4_IO_MAP, BMI088_GYRO_INT_MAP_DRDY_BOTH);
    if (st != BMI088_OK) return st;

    return BMI088_OK;
}

/* ---- DMA read launchers ------------------------------------------------- */

HAL_StatusTypeDef bmi088_read_accel_dma(uint8_t *raw6)
{
    return HAL_I2C_Mem_Read_DMA(s_hi2c, BMI088_ACC_I2C_ADDR,
                                BMI088_ACC_X_LSB, I2C_MEMADD_SIZE_8BIT,
                                raw6, 6U);
}

HAL_StatusTypeDef bmi088_read_gyro_dma(uint8_t *raw6)
{
    return HAL_I2C_Mem_Read_DMA(s_hi2c, BMI088_GYRO_I2C_ADDR,
                                BMI088_GYRO_RATE_X_LSB, I2C_MEMADD_SIZE_8BIT,
                                raw6, 6U);
}

/* ---- Raw → physical decoding ------------------------------------------- */

static int16_t le16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void bmi088_parse_accel(const uint8_t *raw6, bmi088_vec3_t *out_g)
{
    /* Datasheet §5.3.4: acc_g = (raw / 32768) * 1.5 * 2^(range+1)
     * range_3g(0):  ±3 g    range_6g(1):  ±6 g
     * range_12g(2): ±12 g   range_24g(3): ±24 g
     */
    const float full_scale_g = 1.5f * (float)(1U << (s_cfg.acc_range + 1U));
    const float lsb_g = full_scale_g / 32768.0f;

    out_g->x = (float)le16(&raw6[0]) * lsb_g;
    out_g->y = (float)le16(&raw6[2]) * lsb_g;
    out_g->z = (float)le16(&raw6[4]) * lsb_g;
}

void bmi088_parse_gyro(const uint8_t *raw6, bmi088_vec3_t *out_dps)
{
    /* Datasheet §5.4.4: full-scale halves with each range step.
     * 2000_dps(0)..125_dps(4) → {2000, 1000, 500, 250, 125} dps. */
    static const float fs_dps_table[5] = { 2000.0f, 1000.0f, 500.0f,
                                            250.0f,  125.0f };
    uint8_t r = (s_cfg.gyro_range > 4U) ? 0U : s_cfg.gyro_range;
    const float lsb_dps = fs_dps_table[r] / 32768.0f;

    out_dps->x = (float)le16(&raw6[0]) * lsb_dps;
    out_dps->y = (float)le16(&raw6[2]) * lsb_dps;
    out_dps->z = (float)le16(&raw6[4]) * lsb_dps;
}
