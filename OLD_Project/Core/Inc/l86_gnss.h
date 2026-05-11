/*
 * l86_gnss.h
 *
 *  Created on: Jul 11, 2025
 *      Author: bilal
 */

#ifndef INC_L86_GNSS_H_
#define INC_L86_GNSS_H_

#include <stdint.h>
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define BUFFER_SIZE 1200
#define MSG_SIZE 400
#define DATA_SIZE 120
#define COMMEND_BUFFER_SIZE 50

#define VALID 'A'
#define INVALID 'V'

typedef struct gps_data
{
	float non_fixed_time;
	char is_valid;
	float non_fixed_latitude;
	char N_S;
	float non_fixed_longitude;
	char E_W;
	float speed_over_ground;	// in knots
	float course_over_ground;	// in degree
	uint32_t non_fixed_date;
	char positioning_mode;
	uint8_t fix_status;
	uint8_t satellites_in_use;
	float HDOP;			// Horizontal Dilution of Precision
	float altitude;		// Altitude in meters according to WGS84 ellipsoid
	float geoid_height;	// Height of GeoID (mean sea level) above WGS84 ellipsoid, meter

	float latitude;
	float longitude;
	uint8_t time[3];	// h - m - s
	uint8_t date[3];	// d - m - y
	float orthometric_height; // Orthometric height (deniz seviyesine göre yükseklik) = Altitude (WGS84) − Geoid height

}gps_data_t;

typedef enum
{
	BAUD_RATE_4800 = (uint32_t)4800,
	BAUD_RATE_9600 = (uint32_t)9600,
	BAUD_RATE_14400 = (uint32_t)14400,
	BAUD_RATE_19200 = (uint32_t)19200,
	BAUD_RATE_38400 = (uint32_t)38400,
	BAUD_RATE_57600 = (uint32_t)57600,
	BAUD_RATE_115200 = (uint32_t)115200,
}L86_GNSS_BAUD_RATE;

void L86_GNSS_Init(UART_HandleTypeDef *huart);

void L86_GNSS_Update(gps_data_t *gps_data_);

void L86_GNSS_Print_Info(gps_data_t *gps_data_, UART_HandleTypeDef *huart_Seri_Port);

#endif /* INC_L86_GNSS_H_ */
