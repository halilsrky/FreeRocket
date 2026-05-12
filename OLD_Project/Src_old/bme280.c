/*
 * bme280.c
 *
 *  Created on: May 1, 2024
 *      Author: yahya
 *  Modified on: 2025-06-24
 *      Author: halilsrky
 */

#include "bme280.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

extern BME_parameters_t bme_params;
extern int is_BME_ok;
extern UART_HandleTypeDef huart2;

static I2C_HandleTypeDef* I2C_;
static BME_280_t*         BME;
static uint8_t bme_started_flag = 0;
static uint8_t last_measuring = 1;

// Frequency measurement variables
static uint32_t execution_count = 0;
static uint32_t last_freq_check_time = 0;
static float execution_frequency = 0.0f;


void bme280_getVals()
{
	uint8_t status;
    HAL_StatusTypeDef retVal = HAL_I2C_Mem_Read(I2C_, BME280_ADD, BME280_STATUS, I2C_MEMADD_SIZE_8BIT, &status, 1, 100);
    BME->isUpdated = 0;
    uint8_t current_measuring = ((status & (0x01 << 3)) == 0);

    if ((last_measuring == 1) && (current_measuring == 0)) {
        uint8_t data[8];
        retVal = HAL_I2C_Mem_Read(I2C_, BME280_ADD, BME280_P_MSB_ADD, I2C_MEMADD_SIZE_8BIT, data, 8, 20);
        if (retVal == HAL_OK && memcmp(data, BME->lastReadings, 8) != 0) {
            memcpy(BME->lastReadings, data, 8);
            BME->isUpdated = 1;

/*            execution_count++;

            uint32_t current_time = HAL_GetTick();
            if (current_time - last_freq_check_time >= 1000) {
                execution_frequency = (float)execution_count / ((current_time - last_freq_check_time) / 1000.0f);
                execution_count = 0;
                last_freq_check_time = current_time;

                // Optional: Print frequency to debug UART
                char freq_msg[50];
                sprintf(freq_msg, "BME280 Read Freq: %.2f Hz\r\n", execution_frequency);
                HAL_UART_Transmit(&huart2, (uint8_t*)freq_msg, strlen(freq_msg), 100);
            }

*/

            BME->adcVals.ut = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | ((int32_t)data[5] >> 4);
            BME->adcVals.up = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | ((int32_t)data[2] >> 4);
            BME->adcVals.uh = ((int32_t)data[6] << 8) | ((int32_t)data[7]);
        }
    }
    last_measuring = current_measuring;
    UNUSED(retVal);
}



void bme280_calculate_altitude() {
    // Standard sea level pressure in hPa
    float p_seaLevel = 1013.25;

    // Calculate altitude from pressure using barometric formula
    float rawAltitude = 44330.0 * (1.0 - pow((BME->pressure / p_seaLevel), (1.0 / 5.255)));

    // Apply base altitude correction
    BME->altitude = rawAltitude - BME->base_altitude;
}

void bme280_config()
{
    uint8_t params[25];
    HAL_StatusTypeDef retVal;

    BME->base_altitude = 0.0;

    // Reset and initialize I2C
    HAL_I2C_DeInit(I2C_);
    HAL_Delay(5);
    HAL_I2C_Init(I2C_);
    HAL_Delay(5);

    // Read calibration parameters
    retVal = HAL_I2C_Mem_Read(I2C_, BME280_ADD, BME280_PARAM1_START, I2C_MEMADD_SIZE_8BIT, params, 25, 200);

    BME->parameters->dig_T1 = params[0] | (uint16_t)(params[1] << 8);
    BME->parameters->dig_T2 = params[2] | ((int16_t)params[3] << 8);
    BME->parameters->dig_T3 = params[4] | ((int16_t)params[5] << 8);
    BME->parameters->dig_P1 = params[6] | ((uint16_t)params[7] << 8);
    BME->parameters->dig_P2 = params[8] | ((int16_t)params[9] << 8);
    BME->parameters->dig_P3 = params[10] | ((int16_t)params[11] << 8);
    BME->parameters->dig_P4 = params[12] | ((int16_t)params[13] << 8);
    BME->parameters->dig_P5 = params[14] | ((int16_t)params[15] << 8);
    BME->parameters->dig_P6 = params[16] | ((int16_t)params[17] << 8);
    BME->parameters->dig_P7 = params[18] | ((int16_t)params[19] << 8);
    BME->parameters->dig_P8 = params[20] | ((int16_t)params[21] << 8);
    BME->parameters->dig_P9 = params[22] | ((int16_t)params[23] << 8);
    BME->parameters->dig_H1 = params[24];

    retVal = HAL_I2C_Mem_Read(I2C_, BME280_ADD, BME280_PARAM2_START, I2C_MEMADD_SIZE_8BIT, params, 7, 50);
    BME->parameters->dig_H2 = params[0] | ((int16_t)params[1] << 8);
    BME->parameters->dig_H3 = params[2];
    BME->parameters->dig_H4 = (params[4] & 0xF) | ((int16_t)params[3] << 4);
    BME->parameters->dig_H5 = ((params[4] & 0xF0) >> 4) | ((int16_t)params[5] << 4);
    BME->parameters->dig_H6 = params[6];

    uint8_t data_ctrl = 0;
    data_ctrl = BME->device_config.bme280_output_speed;
    retVal = HAL_I2C_Mem_Write(I2C_, BME280_ADD, BME280_CTRL_HUM, I2C_MEMADD_SIZE_8BIT, &data_ctrl, 1, 50);
    data_ctrl = 0;
    data_ctrl = BME->device_config.bme280_mode | (BME->device_config.bme280_output_speed << 2) | (BME->device_config.bme280_output_speed << 5);
    retVal = HAL_I2C_Mem_Write(I2C_, BME280_ADD, BME280_CTRL_MEAS, I2C_MEMADD_SIZE_8BIT, &data_ctrl, 1, 50);
    data_ctrl = 0;
    data_ctrl = (BME->device_config.bme280_standby_time << 5) | (BME->device_config.bme280_filter << 2);
    retVal = HAL_I2C_Mem_Write(I2C_, BME280_ADD, BME280_CONFIG, I2C_MEMADD_SIZE_8BIT, &data_ctrl, 1, 50);


    HAL_Delay(100);

        int valid_readings = 0;
        float readings_sum = 0.0f;

        for(int i = 0; i < 100; i++) {  // Daha fazla okuma
            bme280_update();

            // Geçerli basınç okuması kontrolü
            if (BME->pressure > 300.0f && BME->pressure < 1200.0f) {  // Makul basınç aralığı
                float temp_altitude = 44330.0 * (1.0 - pow((BME->pressure / 1013.25), (1.0 / 5.255)));

                // Aşırı değerleri filtrele
                if (temp_altitude > -1000.0f && temp_altitude < 10000.0f) {
                    readings_sum += temp_altitude;
                    valid_readings++;
                }
            }
            HAL_Delay(20);  // Kısa bekleme
        }

        // Yeterli geçerli okuma varsa ortalama al
        if (valid_readings > 50) {
            BME->base_altitude = readings_sum / valid_readings;
        } else {
            BME->base_altitude = 0.0f;  // Güvenli varsayılan değer
        }

        bme280_update();

    UNUSED(retVal);
}

void bme280_init(BME_280_t* BME_sensor, I2C_HandleTypeDef* I2C_bme)
{
    BME = BME_sensor;
    I2C_ = I2C_bme;
    BME->parameters = &bme_params;
    bme_started_flag = 0;

/*
	execution_count = 0;
    last_freq_check_time = HAL_GetTick();
    execution_frequency = 0.0f;
*/


    // Check BME280 ID
    uint8_t buf[1];
    HAL_I2C_Mem_Read(I2C_, BME280_ADD, BME280_ID, I2C_MEMADD_SIZE_8BIT, buf, 1, 50);
    if(*buf == 0x60)
        is_BME_ok = 1;
    else
        is_BME_ok = 0;
}

void bme280_update() {
    int32_t var1_t, var2_t, T, adc_T;
    bme280_getVals();

    if(BME->isUpdated == 1){

    	if (!bme_started_flag) {
    	    bme_started_flag = 1;
    	    BME->lastTime = HAL_GetTick();
    	    BME->deltaTime1 = 0.0f;
    	} else {
    	    uint32_t now = HAL_GetTick();
    	    BME->deltaTime1 = (now > BME->lastTime) ? (now - BME->lastTime) : 0.0f;
    	    BME->deltaTime1 = BME->deltaTime1 / 1000.0f;
    	    BME->lastTime = now;
    	}

        //For temperature
        adc_T = BME->adcVals.ut;
        var1_t = ((((adc_T >> 3 ) - ((int32_t)BME->parameters->dig_T1 << 1))) * ((int32_t)BME->parameters->dig_T2)) >> 11;
        var2_t = (((((adc_T >> 4) - ((int32_t)BME->parameters->dig_T1)) * ((adc_T >> 4) - ((int32_t)BME->parameters->dig_T1))) >> 12) * ((int32_t)BME->parameters->dig_T3)) >> 14;
        int32_t t_fine = var1_t + var2_t;
        T = (t_fine * 5 + 128) >> 8;
        BME->temperature = (float)T / 100.0;

        //For pressure
        int64_t var1_p, var2_p, P, adc_P;
        adc_P = (int64_t)BME->adcVals.up;
        var1_p = ((int64_t)t_fine) - 128000;
        var2_p = var1_p * var1_p * (int64_t)BME->parameters->dig_P6;
        var2_p = var2_p + ((var1_p *(int64_t)BME->parameters->dig_P5) <<17);
        var2_p = var2_p + (((int64_t)BME->parameters->dig_P4) << 35);
        var1_p = ((var1_p * var1_p * (int64_t)BME->parameters->dig_P3) >> 8) + ((var1_p * (int64_t)BME->parameters->dig_P2) << 12);
        var1_p = (((((int64_t)1) <<47 ) + var1_p)) * ((int64_t) BME->parameters->dig_P1) >> 33;
        if(var1_p == 0)
        {
            P = 0;
        }
        else
        {
            P = 1048576 - adc_P;
            P = (((P << 31) - var2_p) * 3125) / var1_p;
            var1_p = (((int64_t) BME->parameters->dig_P9) * (P >> 13) * (P >> 13)) >> 25;
            var2_p = (((int64_t) BME->parameters->dig_P8) * P) >> 19;
            P = (( P + var1_p + var2_p) >> 8) + (((int64_t)BME->parameters->dig_P7) << 4);
        }

        BME->pressure = ((float)P / 256.0 / 100.0);

        //for humidity
        uint32_t var_h, adc_H;
        adc_H = BME->adcVals.uh;

        var_h = (t_fine - ((int32_t)76800));
        var_h = (((((adc_H << 14) - (((int32_t)BME->parameters->dig_H4) << 20) - (((int32_t)BME->parameters->dig_H5) * var_h)) + ((int32_t)16384)) >> 15) * (((((((var_h *((int32_t)BME->parameters->dig_H6)) >> 10) * (((var_h * ((int32_t)BME->parameters->dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)BME->parameters->dig_H2) + 8192) >> 14));
        var_h = (var_h - (((((var_h >> 15) * (var_h >> 15)) >> 7) * ((int32_t)BME->parameters->dig_H1)) >> 4));
        var_h = (var_h < 0 ? 0 : var_h);
        var_h = (var_h > 419430400 ? 419430400 : var_h);
        BME->humidity = ((float)(var_h >> 12)) / 1024.0;

        // Calculate raw altitude (no filtering)
        bme280_calculate_altitude();
        BME->isUpdated = 0;
    }
}

// Simple interface functions for external modules to access data
float bme280_get_altitude() {
    return BME->altitude;
}

float bme280_get_pressure() {
    return BME->pressure;
}

float bme280_get_temperature() {
    return BME->temperature;
}

float bme280_get_humidity() {
    return BME->humidity;
}

// Function to get the current execution frequency
float bme280_get_execution_frequency() {
    return execution_frequency;
}

