/**
 * @file test_modes.c
 * @brief Test mode handlers for rocket flight control system
 * @date 2025-07-03
 * @author halilsrky
 */

#include "test_modes.h"
#include "packet.h"
#include "flight_algorithm.h"
#include <string.h>
#include "data_logger.h"
#include "l86_gnss.h"

// SIT packet buffer
extern unsigned char sit_paket[36];
extern unsigned char sd_paket[64];

extern UART_HandleTypeDef huart2;


/**
 * @brief Initialize test modes module
 */
void test_modes_init(void)
{
    // Nothing specific to initialize for test modes
}

/**
 * @brief Handle SIT (Sensor Interface Test) mode
 */
void test_modes_handle_sit(bme_sample_t* bme, bmi_sample_t* bmi)
{
	addDataPacketSit(bme, bmi);
    if (!usart2_tx_busy) {
        uart2_send_packet_dma((uint8_t*)sit_paket, 36);
    }
}


void algorithm_update_sut(void)
{

	fused_sample_t sensor_output;

    // Check if SUT data is ready
    if (uart_handler_sut_data_ready()) {
        uart_handler_clear_sut_flag();

        sut_data_t sut_data;
        if (uart_handler_get_sut_data(&sut_data)) {
            // Process SUT data through test modes handler
            uint16_t status_bits = test_modes_handle_sut(&sut_data, &sensor_output);
            uart_handler_send_status(status_bits);
        }
    }
}


/**
 * @brief Handle SUT (System Under Test) mode
 */
uint16_t test_modes_handle_sut(sut_data_t* sut_data, fused_sample_t* sensor_output)
{
    // Convert SUT data to BME and BMI structures
	bme_sample_t bme_sut = {0};
	bmi_sample_t bmi_sut = {0};

    // Fill BME data
    bme_sut.altitude = sut_data->altitude;
    bme_sut.pressure = sut_data->pressure;

    // Fill BMI data
    bmi_sut.accel_z = sut_data->accel_x;
    bmi_sut.accel_y = sut_data->accel_y;
    bmi_sut.accel_x = (-sut_data->accel_z);
    bmi_sut.gyro_z = sut_data->gyro_x;
    bmi_sut.theta = fabs(sut_data->gyro_y) > fabs(sut_data->gyro_x) ? sut_data->gyro_y : sut_data->gyro_x;
    bmi_sut.gyro_x = sut_data->gyro_z;

    // Process synthetic data through sensor fusion first
    sensor_fusion_update_kalman(&bme_sut, &bmi_sut, sensor_output);
	//addDataPacketSD(&bme_sut, &bmi_sut, &gnss_data, sensor_output, 0, 0);
	log_normal_packet_data(sd_paket, "sut.bin");
    // Then run flight algorithm with fused data
    flight_algorithm_update(&bme_sut, &bmi_sut, sensor_output);
    uint16_t status_bits = flight_algorithm_get_status_bits();

    return status_bits;
}
