/**
 * @file uart_handler.c
 * @brief UART communication handler for rocket flight control system
 * @date 2025-07-03
 * @author halilsrky
 */

#include "uart_handler.h"
#include "packet.h"
#include "sensor_fusion.h"
#include <string.h>

// Private variables
static SystemMode_t current_mode = MODE_NORMAL;
static volatile uint8_t command_packet_ready = 0;
static volatile uint8_t sut_packet_ready = 0;
static sut_data_t latest_sut_data = {0};

extern UART_HandleTypeDef huart2;

// Status packet DMA buffer (DMA safety requires static/global buffer)
static uint8_t status_packet_dma[6];

/**
 * @brief Initialize UART handler
 */
void uart_handler_init(void)
{
    current_mode = MODE_NORMAL;
    command_packet_ready = 0;
    sut_packet_ready = 0;
    memset(&latest_sut_data, 0, sizeof(latest_sut_data));
}

/**
 * @brief Process incoming UART packets (call from main loop)
 */
void uart_handler_process_packets(void)
{
    if (usart2_packet_ready) {
        usart2_packet_ready = 0; // Clear flag immediately

        // Command packet (5 bytes with header 0xAA)
        if (usart2_rx_buffer[0] == CMD_HEADER && usart2_packet_size == 5) {
            command_packet_ready = 1;
            process_command_packet(usart2_rx_buffer);
        }
        // SUT data packet (36 bytes with header 0xAB)
        else if (usart2_rx_buffer[0] == PACKET_HEADER && usart2_packet_size == 36) {
            sut_packet_ready = 1;
            process_sut_packet(usart2_rx_buffer);
        }
    }
}

/**
 * @brief Process command packet from buffer
 * @param buffer Pointer to received buffer
 */
void process_command_packet(uint8_t* buffer)
{
    if (buffer[0] == CMD_HEADER) {
        uint8_t command = buffer[1];
        //uint8_t checksum = buffer[2];
        uint8_t footer1 = buffer[3];
        uint8_t footer2 = buffer[4];

        if (footer1 == CMD_FOOTER1 && footer2 == CMD_FOOTER2) {
            switch (command) {
                case CMD_SIT:
                    current_mode = MODE_SIT;
                    break;
                case CMD_SUT:
                    current_mode = MODE_SUT;
                    break;
                case CMD_STOP:
                    current_mode = MODE_NORMAL;
                    break;
            }
        }
    }
}

/**
 * @brief Process SUT data packet from buffer
 * @param buffer Pointer to received buffer
 */
void process_sut_packet(uint8_t* buffer)
{
    // Header, footer and checksum verification
    if (buffer[0] != PACKET_HEADER) return;
    if (buffer[34] != CMD_FOOTER1 || buffer[35] != CMD_FOOTER2) return;

    uint8_t calculated_checksum = 0;
    for (int i = 0; i < 33; i++) {
        calculated_checksum += buffer[i];
    }
    calculated_checksum %= 256;
    if (calculated_checksum != buffer[33]) return;

    // Parse data
    latest_sut_data.altitude = uint8_arrayi_float32_ye_donustur(&buffer[1]);
    latest_sut_data.pressure = uint8_arrayi_float32_ye_donustur(&buffer[5]);
    latest_sut_data.accel_x = uint8_arrayi_float32_ye_donustur(&buffer[9]);
    latest_sut_data.accel_y = uint8_arrayi_float32_ye_donustur(&buffer[13]);
    latest_sut_data.accel_z = uint8_arrayi_float32_ye_donustur(&buffer[17]);
    latest_sut_data.gyro_x = uint8_arrayi_float32_ye_donustur(&buffer[21]);
    latest_sut_data.gyro_y = uint8_arrayi_float32_ye_donustur(&buffer[25]);
    latest_sut_data.gyro_z = uint8_arrayi_float32_ye_donustur(&buffer[29]);
}

/**
 * @brief Get current system mode
 */
SystemMode_t uart_handler_get_mode(void)
{
    return current_mode;
}

/**
 * @brief Check if command packet is ready
 */
uint8_t uart_handler_command_ready(void)
{
    return command_packet_ready;
}

/**
 * @brief Check if SUT data packet is ready
 */
uint8_t uart_handler_sut_data_ready(void)
{
    return sut_packet_ready;
}

/**
 * @brief Get the latest SUT data
 */
uint8_t uart_handler_get_sut_data(sut_data_t* data)
{
    if (data == NULL) return 0;

    memcpy(data, &latest_sut_data, sizeof(sut_data_t));
    return 1;
}

/**
 * @brief Clear command ready flag
 */
void uart_handler_clear_command_flag(void)
{
    command_packet_ready = 0;
}

/**
 * @brief Clear SUT data ready flag
 */
void uart_handler_clear_sut_flag(void)
{
    sut_packet_ready = 0;
}

/**
 * @brief Send status packet (for SUT mode)
 */
void uart_handler_send_status(uint16_t status_bits)
{
    if (!usart2_tx_busy) {
        // Use static buffer for DMA safety
        status_packet_dma[0] = 0xAA;
        status_packet_dma[1] = status_bits & 0xFF;         // Low byte of status
        status_packet_dma[2] = (status_bits >> 8) & 0xFF;  // High byte of status

        // Calculate checksum (only for header and status bytes)
        status_packet_dma[3] = (status_packet_dma[0] + status_packet_dma[1] + status_packet_dma[2]) % 256;

        status_packet_dma[4] = 0x0D;
        status_packet_dma[5] = 0x0A;

        uart2_send_packet_dma(status_packet_dma, 6);
    }
}

void set_current_mode(SystemMode_t test_mode){
	current_mode = test_mode;
}
