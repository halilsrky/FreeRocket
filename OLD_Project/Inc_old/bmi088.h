/*
 * bmi088.h
 *
 *  Created on: May 6, 2024
 *      Author: yahya
 */

#ifndef INC_BMI088_H_
#define INC_BMI088_H_
#include <stdint.h>
#include "main.h"
#include <stdio.h>
#include <string.h>


extern float roll, pitch, yaw;

//Offset Values
#ifdef ROCKET_CARD
#define ACCEL_Z_OFFSET 			(float)4.0
#define ACCEL_Y_OFFSET 			(float)-15.0
#define ACCEL_X_OFFSET 			(float)-4.0
#else
#define ACCEL_Z_OFFSET 			(float)0.0
#define ACCEL_Y_OFFSET 			(float)0.0
#define ACCEL_X_OFFSET 			(float)0.0
#endif

//Accelerometer register address
#define ACC_I2C_ADD				((uint8_t)0x18 << 1)	//0x18

#define ACC_SOFTRESET			0x7E
#define ACC_PWR_CTRL			0x7D
#define ACC_PWR_CONF			0x7C
#define ACC_SELF_TEST			0x6D
#define ACC_INT_MAP_DATA		0x58
#define ACC_INT2_IO_CTRL		0x54
#define ACC_INT1_IO_CTRL		0x53
#define ACC_RANGE				0x41
#define ACC_CONF				0x40
#define ACC_TEMP_LSB			0x23
#define ACC_TEMP_MSB			0x22
#define ACC_INT_STAT_1			0x1D

#define ACC_SENSORTIME_2		0x1A
#define ACC_SENSORTIME_1		0x19
#define ACC_SENSORTIME_0		0x18
#define ACC_Z_MSB				0x17
#define ACC_Z_LSB				0x16
#define ACC_Y_MSB				0x15
#define ACC_Y_LSB				0x14
#define ACC_X_MSB				0x13
#define ACC_X_LSB				0x12

#define ACC_STATUS				0x03
#define ACC_ERR_REG				0x02
#define ACC_CHIP_ID				0x00

//ACC register values
#define ACC_BWP_OSR4			0x08
#define ACC_BWP_OSR2			0x09
#define ACC_BWP_NORMAL			0x0A

#define ACC_ODR_12_5			0x05
#define ACC_ODR_25				0x06
#define ACC_ODR_50				0x07
#define ACC_ODR_100				0x08
#define ACC_ODR_200				0x09
#define ACC_ODR_400				0x0A
#define ACC_ODR_800				0x0B
#define ACC_ODR_1600			0x0C

#define ACC_RANGE_3G			0x00
#define ACC_RANGE_6G			0x01
#define ACC_RANGE_12G			0x02
#define ACC_RANGE_24G			0x03

#define ACC_INT1_IN_ENABLE		0x01
#define ACC_INT1_OUT_ENABLE		0x01

#define ACC_INT1_OD_PP			0x00		//Output push-pull
#define ACC_INT1_OD_OD			0x01		//Output open drain

#define ACC_INT1_LVL_ACT_LOW	0x00		//GPIO level active LOW
#define ACC_INT1_LVL_ACT_HIGH	0x01		//GPIO level active HIGH

#define ACC_INT2_IN_ENABLE		0x01
#define ACC_INT2_OUT_ENABLE		0x01

#define ACC_INT2_OD_PP			0x00		//Output push-pull
#define ACC_INT2_OD_OD			0x01		//Output open drain

#define ACC_INT2_LVL_ACT_LOW	0x00		//GPIO level active LOW
#define ACC_INT2_LVL_ACT_HIGH	0x01		//GPIO level active HIGH

#define ACC_SELF_TEST_OFF		0x00
#define ACC_SELF_TEST_POSITIVE	0x0D
#define ACC_SELF_TEST_NEGATIVE	0x09

#define ACC_PWR_SAVE_OFF		0x03
#define ACC_PWR_SAVE_ACTIVE		0x00
#define ACC_PWR_SAVE_ULTRA		0x01

#define ACC_ENABLE				0x04
#define ACC_DISABLE				0x00

#define ACC_RESET				0xB6
#define FIFO_RESET				0xB0

//Gyroscope register address
#define GYRO_I2C_ADD			((uint8_t)0x68 << 1)	//0x68

#define GYRO_SELF_TEST			0x3C
#define GYRO_INT_3_4_IO_MAP		0x18
#define GYRO_INT_3_4_IO_CONF	0x16
#define GYRO_INT_CTRL			0x15
#define GYRO_SOFT_RESET			0x14
#define GYRO_LPM1				0x11
#define GYRO_BANDWITH			0x10
#define GYRO_RANGE				0x0F
#define GYRO_INT_STAT_1			0x0A

#define GYRO_RATE_Z_MSB			0x07
#define GYRO_RATE_Z_LSB			0x06
#define GYRO_RATE_Y_MSB			0x05
#define GYRO_RATE_Y_LSB			0x04
#define GYRO_RATE_X_MSB			0x03
#define GYRO_RATE_X_LSB			0x02

#define GYRO_CHIP_ID			0x00

//GYRO register values
#define GYRO_RANGE_2000			0x00
#define GYRO_RANGE_1000			0x01
#define GYRO_RANGE_500			0x02
#define GYRO_RANGE_250			0x03
#define GYRO_RANGE_125			0x04

#define GYRO_BW_532				0x00
#define GYRO_BW_230				0x01
#define GYRO_BW_116				0x02
#define GYRO_BW_47				0x03
#define GYRO_BW_23				0x04
#define GYRO_BW_12				0x05
#define GYRO_BW_64				0x06
#define GYRO_BW_32				0x07

#define GYRO_LPM_NORMAL			0x00
#define GYRO_LPM_SUSPEND		0x80
#define GYRO_LPM_DEEPSUSPEND	0x20

#define GYRO_RESET				0xB6

#define GYRO_INT_ENABLE			0x80
#define GYRO_INT_DISABLE		0x00

#define GYRO_INT_IO_PP			0x00
#define GYRO_INT_IO_OD			0x01

#define GYRO_INT_ACT_LOW		0x00
#define GYRO_INT_ACT_HIGH		0x01

#define GYRO_INT_MAP_OFF		0x00
#define GYRO_INT_MAP_3			0x01
#define GYRO_INT_MAP_4			0x80
#define GYRO_INT_MAP_BOTH		0x81

#define DEG_TO_RAD (3.14159265359f / 180.0f)


//Struct typedefs
typedef struct bmi088_flag
{
	uint8_t isGyroUpdated, isAccelUpdated;
	uint8_t isAccelDmaComplete, isGyroDmaComplete;
	uint8_t isDmaTransferActive;
	uint8_t isAccelPending, isGyroPending; // Pending requests when DMA is busy
}bmi088_flag_t;

typedef struct bmi088_offsets
{
	float gyro_offset[3];
	float acc_offset[3];
}bmi088_offsets_t;

typedef struct bmi088_conf
{
	uint8_t acc_powerMode;
	uint8_t acc_range;
	uint8_t acc_bandwith;
	uint8_t acc_outputDateRate;
	uint8_t gyro_powerMode;
	uint8_t gyro_bandWidth;
	uint8_t gyro_range;
	I2C_HandleTypeDef	*BMI_I2c;
	IRQn_Type acc_IRQ, gyro_IRQ;
	bmi088_offsets_t	*offsets;
}bmi088_conf_t;

typedef struct bmi088_datas
{
	float gyro_x, gyro_y, gyro_z;
	float angle_x, angle_y, angle_z;
	float theta;
	float delta_angle_x, delta_angle_y, delta_angle_z;
	float yaw, pitch, roll;
	float yaw1, pitch1, roll1;
	float acc_x, acc_y, acc_z;
	float temp;
	float current_time, last_time, delta_time;
	float vel_x, vel_y, vel_z;
	uint8_t raw_accel_data[9];
	uint8_t raw_gyro_data[6];
	uint8_t raw_temp_data[2];
}bmi088_datas_t;

typedef struct bmi088_struct
{
	bmi088_flag_t 		flags;
	bmi088_conf_t 		device_config;
	bmi088_datas_t		datas;
}bmi088_struct_t;

uint8_t bmi088_init(bmi088_struct_t* BMI);
void bmi088_config(bmi088_struct_t* BMI);
void bmi088_process_accel_data(bmi088_struct_t* BMI);
void bmi088_process_gyro_data(bmi088_struct_t* BMI);
HAL_StatusTypeDef bmi088_start_accel_dma(bmi088_struct_t* BMI);
HAL_StatusTypeDef bmi088_start_gyro_dma(bmi088_struct_t* BMI);
void bmi088_set_gyro_INT(bmi088_struct_t* BMI);
void bmi088_set_accel_INT(bmi088_struct_t* BMI);
void get_offset(bmi088_struct_t* BMI);
uint8_t bmi088_getGyroChipId(bmi088_struct_t* BMI);
void bmi088_accel_dma_complete_callback(bmi088_struct_t* BMI);
void bmi088_gyro_dma_complete_callback(bmi088_struct_t* BMI);
void bmi088_temp_dma_complete_callback(bmi088_struct_t* BMI);

// Debug: Frekans hesaplama fonksiyonları
float bmi088_get_accel_frequency(void);
float bmi088_get_gyro_frequency(void);

#endif /* INC_BMI088_H_ */
