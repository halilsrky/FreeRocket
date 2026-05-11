/**
 * @file uart_handler.h
 * @brief UART communication handler for rocket flight control system
 * @date 2025-07-03
 * @author halilsrky
 */

#ifndef INC_UART_HANDLER_H_
#define INC_UART_HANDLER_H_

#include <stdint.h>

// Command definitions
#define CMD_HEADER               0xAA
#define CMD_SIT                  0x20
#define CMD_SUT                  0x22
#define CMD_STOP                 0x24
#define PACKET_HEADER            0xAB
#define CMD_FOOTER1              0x0D
#define CMD_FOOTER2              0x0A

// System operation modes
typedef enum {
    MODE_NORMAL,  // Normal flight mode
    MODE_SIT,     // Sensor Interface Test mode
    MODE_SUT      // System Under Test mode
} SystemMode_t;

// SUT data structure for parsed synthetic data
typedef struct {
    float altitude;
    float pressure;
    float accel_x, accel_y, accel_z; // m/s²
    float gyro_x, gyro_y, gyro_z;    // rad/s (or deg/s depending on config)
    float theta;                      // Angle (degrees)
} sut_data_t;

/**
 * @brief Initialize UART handler
 */
void uart_handler_init(void);

/**
 * @brief Process incoming UART packets (call from main loop)
 */
void uart_handler_process_packets(void);

/**
 * @brief Get current system mode
 * @return Current system mode
 */
SystemMode_t uart_handler_get_mode(void);
/**
 * @brief Send status packet (for SUT mode)
 */
void uart_handler_send_status(uint16_t status_bits);

/**
 * @brief Check if command packet is ready
 * @return 1 if command packet ready, 0 otherwise
 */
uint8_t uart_handler_command_ready(void);

/**
 * @brief Process command packet from buffer
 * @param buffer Pointer to received buffer
 */
void process_command_packet(uint8_t* buffer);

/**
 * @brief Process SUT data packet from buffer
 * @param buffer Pointer to received buffer
 */
void process_sut_packet(uint8_t* buffer);

/**
 * @brief Check if SUT data packet is ready
 * @return 1 if SUT data ready, 0 otherwise
 */
uint8_t uart_handler_sut_data_ready(void);

/**
 * @brief Get the latest SUT data
 * @param data Pointer to structure to fill with SUT data
 * @return 1 if valid data available, 0 otherwise
 */
uint8_t uart_handler_get_sut_data(sut_data_t* data);

/**
 * @brief Clear command ready flag
 */
void uart_handler_clear_command_flag(void);

/**
 * @brief Clear SUT data ready flag
 */
void uart_handler_clear_sut_flag(void);

void set_current_mode(SystemMode_t test_mode);

// External dependencies from main.c
extern volatile uint8_t usart2_packet_ready;
extern volatile uint16_t usart2_packet_size;
extern uint8_t usart2_rx_buffer[36];
extern volatile uint8_t usart2_tx_busy;
extern void uart2_send_packet_dma(uint8_t *data, uint16_t size);

#endif /* INC_UART_HANDLER_H_ */
