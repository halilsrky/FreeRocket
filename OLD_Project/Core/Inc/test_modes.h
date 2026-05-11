/**
 * @file test_modes.h
 * @brief Test mode handlers for rocket flight control system
 * @date 2025-07-03
 * @author halilsrky
 */

#ifndef INC_TEST_MODES_H_
#define INC_TEST_MODES_H_

#include "uart_handler.h"
#include "sensor_fusion.h"
#include <stdint.h>
#include "sensor_mailbox.h"

/**
 * @brief Initialize test modes module
 */
void test_modes_init(void);

/**
 * @brief Handle SIT (Sensor Interface Test) mode
 * @param bme Pointer to BME280 data structure
 * @param bmi Pointer to BMI088 data structure
 */
void test_modes_handle_sit(bme_sample_t* bme, bmi_sample_t* bmi);
void algorithm_update_sut(void);
/**
 * @brief Handle SUT (System Under Test) mode
 * @param sut_data Pointer to synthetic data from UART
 * @param sensor_output Pointer to sensor fusion output
 * @return Status bits from flight algorithm processing
 */
uint16_t test_modes_handle_sut(sut_data_t* sut_data, fused_sample_t* sensor_output);

// External dependencies
extern volatile uint8_t tx_timer_flag;
extern volatile uint8_t usart1_tx_busy;
extern void uart1_send_packet_dma(uint8_t *data, uint16_t size);
extern unsigned char sit_paket[36];  // ADDED: External declaration for packet buffer
extern void addDataPacketSit(bme_sample_t* bme, bmi_sample_t* bmi);  // ADDED: Function declaration

#endif /* INC_TEST_MODES_H_ */
