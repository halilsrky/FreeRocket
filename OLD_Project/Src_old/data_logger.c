/*
 * data_logger.c
 *
 *  Created on: Jun 7, 2025
 *      Author: yahya
 */
#include "data_logger.h"
#include "fatfs.h"
#include "stdint.h"
#include "stdio.h"
#include "string.h"
#include "stm32f4xx_hal.h"

// Buffer için tanımlamalar
#define PACKET_SIZE 64
#define PACKETS_PER_SECTOR 8  // 8 paket = 512 byte
#define BUFFER_SIZE (PACKET_SIZE * PACKETS_PER_SECTOR)
#define FLUSH_THRESHOLD 10  // 10 kez buffer dolunca dosyayı kapat

static uint8_t packet_buffer[BUFFER_SIZE];
static uint16_t buffer_index = 0;
static uint8_t flush_counter = 0;  // Kaç kez buffer dolduğunu sayar
static uint8_t file_open = 0;  // Dosya açık mı?
static uint8_t sd_open = 1;  // Dosya açık mı?


FATFS FatFs; 	//Fatfs handle
FIL fil; 		//File handle
FRESULT fres; 	//Result after operations

//For file operation functions look at https://elm-chan.org/fsw/ff/00index_e.html

void data_logger_init()
{
	// SD kartı mount et
	fres = f_mount(&FatFs, "", 1);
	if (fres != FR_OK) {
		sd_open = 0;
		return;
	}
	
	// packets.bin dosyasını oluştur (eğer yoksa)
	fres = f_open(&fil, "gorev.bin", FA_WRITE | FA_OPEN_ALWAYS);
	if (fres == FR_OK) {
		f_close(&fil);
	}
	
	// Buffer'ı sıfırla
	buffer_index = 0;
	flush_counter = 0;
	file_open = 0;
}

void log_normal_packet_data(unsigned char* packet_data, char* name)
{

	if (sd_open == 0) {
		return;
	}

	// Paketi buffer'a kopyala
	memcpy(&packet_buffer[buffer_index], packet_data, PACKET_SIZE);
	buffer_index += PACKET_SIZE;
	
	// Buffer doldu mu kontrol et
	if (buffer_index >= BUFFER_SIZE) {
		// Dosya açık değilse aç
		if (!file_open) {
			fres = f_open(&fil, name, FA_WRITE | FA_OPEN_ALWAYS);
			if (fres == FR_OK) {
				f_lseek(&fil, f_size(&fil));  // Dosya sonuna git
				file_open = 1;
			} else {
				sd_open = 0;
				buffer_index = 0;
				return;
			}
		}

		// Buffer'ı dosyaya yaz
		unsigned int file_res = 0;
		f_write(&fil, packet_buffer, buffer_index, &file_res);

		// Buffer'ı sıfırla ve sayacı artır
		buffer_index = 0;
		flush_counter++;

		// 10 kez yazım yapıldıysa dosyayı kapat
		if (flush_counter >= FLUSH_THRESHOLD) {
			f_close(&fil);
			file_open = 0;
			flush_counter = 0;
		}
	}
}
void flush_packet_buffer(void)
{
	// Eğer buffer'da veri varsa yaz
	if (buffer_index > 0) {
		// Dosya açık değilse aç
		if (!file_open) {
			fres = f_open(&fil, "gorev.bin", FA_WRITE | FA_OPEN_ALWAYS);
			if (fres == FR_OK) {
				f_lseek(&fil, f_size(&fil));
				file_open = 1;
			}
		}

		if (file_open) {
			unsigned int file_res = 0;
			f_write(&fil, packet_buffer, buffer_index, &file_res);
			buffer_index = 0;
		}
	}

	// Dosya açıksa kapat
	if (file_open) {
		f_close(&fil);
		file_open = 0;
		flush_counter = 0;
	}
}

// Sistem kapanırken veya acil durumda buffer'ı boşalt
void force_flush_buffer(void)
{
	flush_packet_buffer();
}
