/*
 * bmi088_regs.h — BMI088 IMU register map & bitfield constants.
 * Datasheet: BST-BMI088-DS001 Rev 1.4 (Feb 2018)
 *
 * 7-bit I2C addresses (SDO tied low):
 *   ACC  = 0x18
 *   GYRO = 0x68
 * Helpers below shift left by 1 to produce the 8-bit framing
 * STM32 peripherals expect.
 */
#ifndef BMI088_REGS_H
#define BMI088_REGS_H

#include <stdint.h>

/* 8-bit framed I2C device addresses */
#define BMI088_ACC_I2C_ADDR    ((uint8_t)(0x18U << 1))   /* 0x30 */
#define BMI088_GYRO_I2C_ADDR   ((uint8_t)(0x68U << 1))   /* 0xD0 */

/* ---- Accelerometer registers ------------------------------------------- */
#define BMI088_ACC_CHIP_ID            0x00U  /* expect 0x1E */
#define BMI088_ACC_ERR_REG            0x02U
#define BMI088_ACC_STATUS             0x03U
#define BMI088_ACC_X_LSB              0x12U  /* 6 data bytes + 3 sensortime */
#define BMI088_ACC_X_MSB              0x13U
#define BMI088_ACC_Y_LSB              0x14U
#define BMI088_ACC_Y_MSB              0x15U
#define BMI088_ACC_Z_LSB              0x16U
#define BMI088_ACC_Z_MSB              0x17U
#define BMI088_ACC_SENSORTIME_0       0x18U
#define BMI088_ACC_SENSORTIME_1       0x19U
#define BMI088_ACC_SENSORTIME_2       0x1AU
#define BMI088_ACC_INT_STAT_1         0x1DU
#define BMI088_ACC_TEMP_MSB           0x22U
#define BMI088_ACC_TEMP_LSB           0x23U
#define BMI088_ACC_CONF               0x40U
#define BMI088_ACC_RANGE              0x41U
#define BMI088_ACC_INT1_IO_CTRL       0x53U
#define BMI088_ACC_INT2_IO_CTRL       0x54U
#define BMI088_ACC_INT_MAP_DATA       0x58U
#define BMI088_ACC_SELF_TEST          0x6DU
#define BMI088_ACC_PWR_CONF           0x7CU
#define BMI088_ACC_PWR_CTRL           0x7DU
#define BMI088_ACC_SOFTRESET          0x7EU

#define BMI088_ACC_CHIP_ID_VAL        0x1EU
#define BMI088_ACC_RESET_CMD          0xB6U
#define BMI088_ACC_FIFO_RESET_CMD     0xB0U

#define BMI088_ACC_PWR_SAVE_ACTIVE    0x00U
#define BMI088_ACC_PWR_SAVE_ULTRA     0x01U  /* suspend mode */
#define BMI088_ACC_PWR_SAVE_OFF       0x03U

#define BMI088_ACC_DISABLE            0x00U
#define BMI088_ACC_ENABLE             0x04U

#define BMI088_ACC_BWP_OSR4           0x08U
#define BMI088_ACC_BWP_OSR2           0x09U
#define BMI088_ACC_BWP_NORMAL         0x0AU

#define BMI088_ACC_ODR_12_5           0x05U
#define BMI088_ACC_ODR_25             0x06U
#define BMI088_ACC_ODR_50             0x07U
#define BMI088_ACC_ODR_100            0x08U
#define BMI088_ACC_ODR_200            0x09U
#define BMI088_ACC_ODR_400            0x0AU
#define BMI088_ACC_ODR_800            0x0BU
#define BMI088_ACC_ODR_1600           0x0CU

#define BMI088_ACC_RANGE_3G           0x00U
#define BMI088_ACC_RANGE_6G           0x01U
#define BMI088_ACC_RANGE_12G          0x02U
#define BMI088_ACC_RANGE_24G          0x03U

/* INT1_IO_CTRL: bit3=INT1 output enable, bit2=OD, bit1=active level */
#define BMI088_ACC_INT1_OUT_EN        (1U << 3)
#define BMI088_ACC_INT1_PUSH_PULL     (0U << 2)
#define BMI088_ACC_INT1_OPEN_DRAIN    (1U << 2)
#define BMI088_ACC_INT1_ACTIVE_HIGH   (1U << 1)
#define BMI088_ACC_INT1_ACTIVE_LOW    (0U << 1)

/* INT_MAP_DATA: bit2 = data ready -> INT1 */
#define BMI088_ACC_INT_MAP_DRDY_INT1  (1U << 2)

/* ---- Gyroscope registers ----------------------------------------------- */
#define BMI088_GYRO_CHIP_ID           0x00U  /* expect 0x0F */
#define BMI088_GYRO_RATE_X_LSB        0x02U  /* 6 data bytes */
#define BMI088_GYRO_RATE_X_MSB        0x03U
#define BMI088_GYRO_RATE_Y_LSB        0x04U
#define BMI088_GYRO_RATE_Y_MSB        0x05U
#define BMI088_GYRO_RATE_Z_LSB        0x06U
#define BMI088_GYRO_RATE_Z_MSB        0x07U
#define BMI088_GYRO_INT_STAT_1        0x0AU
#define BMI088_GYRO_RANGE             0x0FU
#define BMI088_GYRO_BANDWIDTH         0x10U
#define BMI088_GYRO_LPM1              0x11U  /* power mode */
#define BMI088_GYRO_SOFT_RESET        0x14U
#define BMI088_GYRO_INT_CTRL          0x15U
#define BMI088_GYRO_INT3_4_IO_CONF    0x16U
#define BMI088_GYRO_INT3_4_IO_MAP     0x18U
#define BMI088_GYRO_SELF_TEST         0x3CU

#define BMI088_GYRO_CHIP_ID_VAL       0x0FU
#define BMI088_GYRO_RESET_CMD         0xB6U

#define BMI088_GYRO_RANGE_2000_DPS    0x00U
#define BMI088_GYRO_RANGE_1000_DPS    0x01U
#define BMI088_GYRO_RANGE_500_DPS     0x02U
#define BMI088_GYRO_RANGE_250_DPS     0x03U
#define BMI088_GYRO_RANGE_125_DPS     0x04U

#define BMI088_GYRO_BW_532            0x00U
#define BMI088_GYRO_BW_230            0x01U
#define BMI088_GYRO_BW_116            0x02U
#define BMI088_GYRO_BW_47             0x03U
#define BMI088_GYRO_BW_23             0x04U
#define BMI088_GYRO_BW_12             0x05U
#define BMI088_GYRO_BW_64             0x06U
#define BMI088_GYRO_BW_32             0x07U

#define BMI088_GYRO_LPM_NORMAL        0x00U
#define BMI088_GYRO_LPM_SUSPEND       0x80U
#define BMI088_GYRO_LPM_DEEPSUSPEND   0x20U

#define BMI088_GYRO_INT_ENABLE        0x80U
#define BMI088_GYRO_INT_DISABLE       0x00U

/* INT3_4 IO config: bit2=INT4 active level, bit1=INT3 push-pull, bit0=INT3 active level */
#define BMI088_GYRO_INT3_PUSH_PULL    (0U << 1)
#define BMI088_GYRO_INT3_OPEN_DRAIN   (1U << 1)
#define BMI088_GYRO_INT3_ACTIVE_HIGH  (1U << 0)
#define BMI088_GYRO_INT4_ACTIVE_HIGH  (1U << 2)

/* INT3_4 IO map */
#define BMI088_GYRO_INT_MAP_DRDY_INT3 (0x01U)
#define BMI088_GYRO_INT_MAP_DRDY_INT4 (0x80U)
#define BMI088_GYRO_INT_MAP_DRDY_BOTH (0x81U)

#endif /* BMI088_REGS_H */
