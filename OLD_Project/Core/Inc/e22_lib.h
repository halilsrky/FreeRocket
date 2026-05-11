/*
 * e22_lib.h
 *
 *  Created on: Dec 20, 2024
 *      Author: yahya
 */

#ifndef INC_E22_LIB_H_
#define INC_E22_LIB_H_
#include <stdint.h>
#include <string.h>
#include "main.h"

#define E22_BAUD_RATE_1200 					0x00
#define E22_BAUD_RATE_2400 					0x01
#define E22_BAUD_RATE_4800 					0x02
#define E22_BAUD_RATE_9600 					0X03
#define E22_BAUD_RATE_19200 				0X04
#define E22_BAUD_RATE_38400 				0X05
#define E22_BAUD_RATE_57600 				0X06
#define E22_BAUD_RATE_115200 				0X07

#define E22_PARITY_8N1						0x00
#define E22_PARITY_8O1						0x01
#define E22_PARITY_8E1 						0X02

#define E22_AIR_DATA_RATE_2400 				0x02
#define E22_AIR_DATA_RATE_4800 				0x03
#define E22_AIR_DATA_RATE_9600 				0x04
#define E22_AIR_DATA_RATE_19200 			0x05
#define E22_AIR_DATA_RATE_38400 			0x06
#define E22_AIR_DATA_RATE_62500 			0x07

#define E22_PACKET_SIZE_200 				0x00
#define E22_PACKET_SIZE_128 				0x01
#define E22_PACKET_SIZE_64 					0x02
#define E22_PACKET_SIZE_32 					0x03

#define E22_RSSI_NOISE_DISABLE 				0x00
#define E22_RSSI_NOISE_ENABLE 				0x01

#define E22_TRANSMITTING_POWER_22 			0x00
#define E22_TRANSMITTING_POWER_17 			0x01
#define E22_TRANSMITTING_POWER_13 			0x02
#define E22_TRANSMITTING_POWER_10 			0x03

#define E22_ENABLE_RSSI_DISABLE 			0x00
#define E22_ENABLE_RSSI_ENABLE 				0x01

#define E22_TRANSMISSION_MODE_TRANSPARENT 	0x00
#define E22_TRANSMISSION_MODE_FIXED 		0x01

#define E22_REPEATER_FUNC_DISABLE			0x00
#define E22_REPEATER_FUNC_ENABLE			0x01

#define E22_LBT_DISABLE 					0x00
#define E22_LBT_ENABLE 						0x01

#define E22_WOR_RECEIVER					0x00
#define E22_WOR_TRANSMITTER					0x01

#define E22_WOR_CYCLE_500 					0x00
#define E22_WOR_CYCLE_1000 					0x01
#define E22_WOR_CYCLE_1500 					0x02
#define E22_WOR_CYCLE_2000 					0x03
#define E22_WOR_CYCLE_2500 					0x04
#define E22_WOR_CYCLE_3000 					0x05
#define E22_WOR_CYCLE_3500 					0x06
#define E22_WOR_CYCLE_4000 					0x07

struct e22_mode_pins
{
	GPIO_TypeDef*		m0_pin_port;
	GPIO_TypeDef*		m1_pin_port;
	uint16_t			m0_pin;
	uint16_t			m1_pin;
};

//degisiklik
typedef struct e22ConfStruct
{
	struct e22_mode_pins	pins;
	uint16_t 				address;
	uint8_t 				net_id;
	uint8_t					baud_rate;
	uint8_t					parity_bit;
	uint8_t					air_rate;
	uint8_t					packet_size;
	uint8_t					rssi_noise;
	uint8_t					power;
	uint8_t					channel;
	uint8_t					rssi_enable;
	uint8_t					mode;
	uint8_t					repeater_func;
	uint8_t					lbt;
	uint8_t					wor;
	uint8_t					wor_cycle;
	uint16_t				key;

}e22_conf_struct_t;

void e22_init(e22_conf_struct_t *lora_conf_struct, UART_HandleTypeDef* huart);
void e22_transmit(e22_conf_struct_t *lora_conf_struct, uint8_t* data, UART_HandleTypeDef* huart, uint16_t len);
void e22_sleep_mode(e22_conf_struct_t *lora_conf_struct);
void e22_config_mode(e22_conf_struct_t *lora_conf_struct);
void e22_transmit_mode(e22_conf_struct_t *lora_conf_struct);
#endif /* INC_E22_LIB_H_ */
