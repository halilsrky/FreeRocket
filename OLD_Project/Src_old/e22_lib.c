/*
 * e22_lib.c
 *
 *  Created on: Dec 20, 2024
 *      Author: yahya
 */
#include "e22_lib.h"

uint8_t data_packet[9] ={0};

static void view_e22_response(UART_HandleTypeDef* lora_uart_hndl, UART_HandleTypeDef* serial_uart_hndl)
{
	while (1)
	{
	  if (__HAL_UART_GET_FLAG(lora_uart_hndl, UART_FLAG_RXNE) == SET)
	  {
	    uint8_t dat[1];
	    HAL_UART_Receive(lora_uart_hndl, (uint8_t *)dat, 1, 100);
	    HAL_UART_Transmit(serial_uart_hndl, (uint8_t *)dat, 1, 100);
	  }
	  if (__HAL_UART_GET_FLAG(serial_uart_hndl, UART_FLAG_RXNE) == SET)
	  {
	    uint8_t dat[1];
	    HAL_UART_Receive(serial_uart_hndl, (uint8_t *)dat, 1, 100);
	    HAL_UART_Transmit(lora_uart_hndl, (uint8_t *)dat, 1, 100);
	  }
	}
}

/**
  * @brief  Initializes the lora module by configuration struct.
  * @param  lora_conf_struct: pointer to the lora configuration struct.
  * @param  huart: pointer to the uart handler typedef.
  * @retval None
  */
void e22_init(e22_conf_struct_t *lora_conf_struct, UART_HandleTypeDef* huart)
{
	data_packet[0] = 0xC0;	//Set register command.
	data_packet[1] = 0x03;	//Starting from byte 0x03
	data_packet[2] = 0x04;	//6 bytes will be configured.
	data_packet[3] = lora_conf_struct->air_rate | (lora_conf_struct->parity_bit << 3) | (lora_conf_struct->baud_rate << 5);																									//Wireless air data rate（bps）, Serial parity bit, UART Serial port rate（bps).
	data_packet[4] = lora_conf_struct->power | (lora_conf_struct->rssi_noise << 5) | (lora_conf_struct->packet_size << 6);																									//Transmitting power, RSSI anbient noise enable, Sub packet settings.
	data_packet[5] = lora_conf_struct->channel;																																											//channel 0-83 (410.125 + CH *1M)
	data_packet[6] = lora_conf_struct->wor_cycle | (lora_conf_struct->wor << 3) | (lora_conf_struct->lbt << 4) | (lora_conf_struct->repeater_func << 5) | (lora_conf_struct->mode << 6) | (lora_conf_struct->rssi_enable << 7);	//WOR cycle time, WOR transceiver control, LBT enable, Repeater function, Transmission mode, Enable RSSI.
	data_packet[7] = (uint8_t)(lora_conf_struct->key >> 8);																																								//high byte of key
	data_packet[8] = (uint8_t)(lora_conf_struct->key);																																									//low byte of key


	//UART transmits the configuration datas.
	uint8_t response[7] = {0};
	for(int i = 0; i < 10; i++)
	{
		HAL_UART_Transmit(huart, data_packet, 7, 50);
		HAL_UART_Receive(huart, response, 7, 50);
		if(memcmp(&response[1], &data_packet[1], 6) == 0)
			break;
		HAL_Delay(10);
	}


	/*
	while (1)
	{
	  if (__HAL_UART_GET_FLAG(&TELEM_UART_HNDLR, UART_FLAG_RXNE) == SET)
	  {
	    uint8_t dat[1];
	    HAL_UART_Receive(&TELEM_UART_HNDLR, (uint8_t *)dat, 1, 100);
	    HAL_UART_Transmit(&TTL_HNDLR, (uint8_t *)dat, 1, 100);
	  }
	  if (__HAL_UART_GET_FLAG(&TTL_HNDLR, UART_FLAG_RXNE) == SET)
	  {
	    uint8_t dat[1];
	    HAL_UART_Receive(&TTL_HNDLR, (uint8_t *)dat, 1, 100);
	    HAL_UART_Transmit(&TELEM_UART_HNDLR, (uint8_t *)dat, 1, 100);
	  }
	}
	*/


}

/**
  * @brief  Transmits the data.
  * @param  lora_conf_struct: Pointer to the lora configuration struct.
  * @param  data: Pointer to the data string.
  * @param  huart: Pointer to the uart handler typedef.
  * @retval None
  */
void e22_transmit(e22_conf_struct_t *lora_conf_struct, uint8_t* data, UART_HandleTypeDef* huart, uint16_t len)
{
	//For normal working mode M0 -> 0    M1 -> 0
	HAL_UART_Transmit(huart, data, len, 50);
}

/**
  * @brief  Makes the module asleep.
  * @param  lora_conf_struct: Pointer to the lora configuration struct.
  * @retval None
  */
void e22_sleep_mode(e22_conf_struct_t *lora_conf_struct)
{
	//For sleep mode M0 -> 1    M1 -> 1
	HAL_GPIO_WritePin(RF_M0_GPIO_Port, RF_M0_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(RF_M1_GPIO_Port, RF_M1_Pin, GPIO_PIN_SET);
}


void e22_config_mode(e22_conf_struct_t *lora_conf_struct)
{
	//For config mode M0 -> 0    M1 -> 1
	HAL_GPIO_WritePin(RF_M0_GPIO_Port, RF_M0_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(RF_M1_GPIO_Port, RF_M1_Pin, GPIO_PIN_SET);
}

void e22_transmit_mode(e22_conf_struct_t *lora_conf_struct)
{
	//For transmission mode M0 -> 0    M1 -> 0
	HAL_GPIO_WritePin(RF_M0_GPIO_Port, RF_M0_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(RF_M1_GPIO_Port, RF_M1_Pin, GPIO_PIN_RESET);
}
