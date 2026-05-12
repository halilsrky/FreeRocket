#ifndef BMI088_DEFS_H
#define BMI088_DEFS_H

/* ── I2C addresses (8-bit shifted) ── */
#define BMI088_ACC_I2C_ADDR     ((uint8_t)(0x18 << 1))
#define BMI088_GYRO_I2C_ADDR    ((uint8_t)(0x68 << 1))

/* ── Accelerometer registers ── */
#define ACC_CHIP_ID             0x00
#define ACC_ERR_REG             0x02
#define ACC_STATUS              0x03
#define ACC_X_LSB               0x12
#define ACC_INT_STAT_1          0x1D
#define ACC_CONF                0x40
#define ACC_RANGE               0x41
#define ACC_INT1_IO_CTRL        0x53
#define ACC_INT2_IO_CTRL        0x54
#define ACC_INT_MAP_DATA        0x58
#define ACC_SELF_TEST           0x6D
#define ACC_PWR_CONF            0x7C
#define ACC_PWR_CTRL            0x7D
#define ACC_SOFTRESET           0x7E

/* ACC_CONF field values */
#define ACC_BWP_OSR4            0x08
#define ACC_BWP_OSR2            0x09
#define ACC_BWP_NORMAL          0x0A

#define ACC_ODR_50              0x07
#define ACC_ODR_100             0x08
#define ACC_ODR_200             0x09
#define ACC_ODR_400             0x0A
#define ACC_ODR_800             0x0B
#define ACC_ODR_1600            0x0C

/* ACC_RANGE values */
#define ACC_RANGE_3G            0x00
#define ACC_RANGE_6G            0x01
#define ACC_RANGE_12G           0x02
#define ACC_RANGE_24G           0x03

/* ACC_PWR_CONF values */
#define ACC_PWR_ACTIVE          0x00
#define ACC_PWR_SUSPEND         0x03

/* ACC_PWR_CTRL values */
#define ACC_ENABLE              0x04
#define ACC_DISABLE             0x00

/* ACC_INT1_IO_CTRL: output enable | push-pull | active-high */
#define ACC_INT1_CFG            ((1u << 3) | (0u << 2) | (1u << 1))

/* ACC_INT_MAP_DATA: DRDY → INT1 */
#define ACC_INT_DRDY_INT1       (1u << 2)

/* ACC reset command */
#define ACC_RESET_CMD           0xB6

/* ── Gyroscope registers ── */
#define GYRO_CHIP_ID            0x00
#define GYRO_RATE_X_LSB         0x02
#define GYRO_RANGE              0x0F
#define GYRO_BANDWIDTH          0x10
#define GYRO_LPM1               0x11
#define GYRO_SOFT_RESET         0x14
#define GYRO_INT_CTRL           0x15
#define GYRO_INT3_INT4_IO_CONF  0x16
#define GYRO_INT3_INT4_IO_MAP   0x18

/* GYRO_RANGE values */
#define GYRO_RANGE_2000DPS      0x00
#define GYRO_RANGE_1000DPS      0x01
#define GYRO_RANGE_500DPS       0x02
#define GYRO_RANGE_250DPS       0x03
#define GYRO_RANGE_125DPS       0x04

/* GYRO_BANDWIDTH (filter BW + ODR pairs) */
#define GYRO_BW_532HZ_ODR2000   0x00
#define GYRO_BW_230HZ_ODR2000   0x01
#define GYRO_BW_116HZ_ODR1000   0x02
#define GYRO_BW_47HZ_ODR400     0x03
#define GYRO_BW_23HZ_ODR200     0x04
#define GYRO_BW_12HZ_ODR100     0x05

/* GYRO_LPM1 power modes */
#define GYRO_LPM_NORMAL         0x00
#define GYRO_LPM_SUSPEND        0x80
#define GYRO_LPM_DEEP_SUSPEND   0x20

/* GYRO_INT_CTRL: data-ready interrupt enable */
#define GYRO_INT_ENABLE         0x80

/* GYRO_INT3_INT4_IO_CONF: INT3 push-pull, active-high */
#define GYRO_INT3_CFG           ((0u << 1) | (1u << 0) | (1u << 2))

/* GYRO_INT3_INT4_IO_MAP: DRDY → INT3 and INT4 */
#define GYRO_INT_MAP_BOTH       0x81

/* Gyro reset command */
#define GYRO_RESET_CMD          0xB6

/* ── Expected chip IDs ── */
#define ACC_CHIP_ID_VAL         0x1E
#define GYRO_CHIP_ID_VAL        0x0F

/* ── Physical constants ── */
#define BMI088_DEG_TO_RAD       0.017453292519943f   /* PI / 180 */
#define BMI088_GRAVITY          9.81f

#endif /* BMI088_DEFS_H */
