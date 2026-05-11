/*
 * i2c1.h — Bare-metal I2C1 driver for STM32F446.
 *
 * Pin map (matches SKYRTOS):
 *   PB8 = I2C1_SCL (AF4, open-drain, pull-up off — board needs external pull-ups)
 *   PB9 = I2C1_SDA (AF4, open-drain)
 *
 * Phase 1 scope: blocking polled register/memory read & write.
 * DMA-driven read path (DMA1 Stream0 Channel1) will be added in phase 2.
 *
 * All routines are intended for the boot/config phase or for low-rate
 * housekeeping reads. Do NOT call from hot paths once the scheduler is
 * running — the polling loops are CPU bound.
 */
#ifndef I2C1_H
#define I2C1_H

#include <stdint.h>
#include "miros.h"

typedef enum {
    I2C1_OK            = 0,
    I2C1_ERR_TIMEOUT   = 1,
    I2C1_ERR_NACK      = 2,
    I2C1_ERR_BUS       = 3,
    I2C1_ERR_ARG       = 4,
    I2C1_ERR_BUSY      = 5,  /* a DMA transfer is already in flight */
    I2C1_ERR_DMA       = 6,  /* DMA reported a transfer error */
} i2c1_status_t;

/* Bring-up: configure GPIO PB8/PB9, RCC, peripheral timing.
 * Idempotent — safe to call multiple times.
 * Bus speed is fixed at 100 kHz (standard mode); APB1 clock is read at
 * runtime so 16 MHz HSI default and 42 MHz HSE+PLL both work without
 * recompilation.
 */
void          i2c1_init(void);

/* Write a single byte to a device register.
 * dev_addr_8bit: pre-shifted 7-bit address (e.g. 0x18 << 1).
 */
i2c1_status_t i2c1_mem_write_byte(uint8_t dev_addr_8bit, uint8_t reg, uint8_t value);

/* Read N bytes starting from a device register into `buf`.
 * Supports len >= 1. Implements the STM32F4 I2C V1 master-receiver
 * sequence (1-byte and >=2-byte have different STOP/NACK timing per
 * RM0390 §27.3.3).
 */
i2c1_status_t i2c1_mem_read(uint8_t dev_addr_8bit, uint8_t reg,
                            uint8_t *buf, uint16_t len);

/* ---- DMA-driven read path -----------------------------------------------
 *
 * Bind a thread + event mask pair so the DMA1 Stream0 TC ISR knows where
 * to POST when the transfer finishes. Call once during init.
 */
void          i2c1_attach_rx_thread(OSThread *t, uint32_t done_evt);

/* Kick off a non-blocking memory read. The register-pointer write phase
 * is done synchronously (very short, ~50 us at 100 kHz), then DMA1
 * Stream0 takes over for the RX phase. On completion the registered
 * event is POSTed; the caller fetches the final status with
 * i2c1_dma_rx_result().
 *
 * Constraints:
 *   - len must be >= 3 (V1 master-receiver errata for N=1/2 needs
 *     special POS/ACK timing that doesn't combine cleanly with DMA;
 *     use the polled i2c1_mem_read() for short transfers).
 *   - Only one DMA RX may be in flight at a time. Returns I2C1_ERR_BUSY
 *     if called while a previous transfer is still running.
 *   - `buf` must remain valid until the done event arrives.
 */
i2c1_status_t i2c1_mem_read_dma_start(uint8_t dev_addr_8bit, uint8_t reg,
                                      uint8_t *buf, uint16_t len);

/* Final status of the most recently completed DMA RX. Valid after the
 * registered done event is observed by the waiting thread.
 */
i2c1_status_t i2c1_dma_rx_result(void);

/* DMA1 Stream0 ISR entry — invoked by DMA1_Stream0_IRQHandler in bsp.c.
 * Sends STOP, tears down the stream, captures status, and POSTs the
 * registered event.
 */
void          i2c1_dma_rx_isr(void);

#endif /* I2C1_H */
