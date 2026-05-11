/*
 * packet.h
 *
 *  Created on: May 7, 2025
 *      Author: Halil
 */

#ifndef INC_PACKET_H_
#define INC_PACKET_H_

#include "bmi088.h"
#include "bme280.h"
#include "l86_gnss.h"
#include "flight_algorithm.h"
#include "sensor_fusion.h"
#include "sensor_mailbox.h"


typedef union {
    float sayi;
    uint8_t array[4];
} FLOAT32_UINT8_DONUSTURUCU;

typedef union {
    uint16_t sayi;
    uint8_t array[2];
} UINT16_UINT8_DONUSTURUCU;

unsigned char check_sum_hesapla_normal(int a);
unsigned char check_sum_hesapla_sit(int a);

void addDataPacketNormal(bme_sample_t* BME, bmi_sample_t* BMI, fused_sample_t* sensor, gnss_sample_t* GNSS, float hmc1021_gauss, float voltage, float current);
void addDataPacketSD(bme_sample_t* BME, bmi_sample_t* BMI, fused_sample_t* sensor, gnss_sample_t* GNSS, float hmc1021_gauss, float voltage, float current);
float uint8_arrayi_float32_ye_donustur(uint8_t byte_array_u8[4]);
#endif /* INC_PACKET_H_ */
