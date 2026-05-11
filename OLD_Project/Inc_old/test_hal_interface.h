/**
 * @file test_hal_interface.h
 * @brief HAL abstraction layer for unit testing
 * 
 * AMAÇ:
 * Production kodu bu interface'i kullanır.
 * Unit test'te mock implementation sağlanır.
 * Production build'de gerçek HAL çağrılır.
 * 
 * KULLANIM:
 * #ifdef UNIT_TEST_MODE
 *     uint32_t hal_get_tick(void) { return mock_tick; }
 * #else
 *     uint32_t hal_get_tick(void) { return HAL_GetTick(); }
 * #endif
 */

#ifndef TEST_HAL_INTERFACE_H
#define TEST_HAL_INTERFACE_H

#include <stdint.h>

/* ============================================================================
 *                         HAL ABSTRACTION INTERFACE
 * ============================================================================ */

#ifdef UNIT_TEST_MODE

/* ============================================================================
 *                         MOCK IMPLEMENTATION (Test Mode)
 * ============================================================================ */

/* Mock tick counter - test code controls this */
extern uint32_t g_mock_tick;

static inline uint32_t hal_get_tick(void) {
    return g_mock_tick;
}

static inline void mock_set_tick(uint32_t tick) {
    g_mock_tick = tick;
}

static inline void mock_advance_tick(uint32_t delta) {
    g_mock_tick += delta;
}

static inline void mock_reset_tick(void) {
    g_mock_tick = 0;
}

/* GPIO Mock */
static inline void hal_gpio_write(void* port, uint16_t pin, uint8_t state) {
    (void)port; (void)pin; (void)state;
    /* Test'te GPIO yazmak no-op */
}

#else

/* ============================================================================
 *                         REAL IMPLEMENTATION (Production Mode)
 * ============================================================================ */

#include "main.h"  /* HAL_GetTick için */

static inline uint32_t hal_get_tick(void) {
    return HAL_GetTick();
}

static inline void hal_gpio_write(void* port, uint16_t pin, uint8_t state) {
    HAL_GPIO_WritePin((GPIO_TypeDef*)port, pin, (GPIO_PinState)state);
}

#endif /* UNIT_TEST_MODE */

#endif /* TEST_HAL_INTERFACE_H */
