/* Board Support Package (BSP) for the NUCLEO-F446RE board */
#include <stdint.h>

#include "bsp.h"
#include "miros.h"
#include "qassert.h"
#include "stm32f446xx.h"
#include "bmi088.h"
#include "i2c1.h"

/* On-board user LED LD2 is on PA5.
 * The NUCLEO-F446RE has only one user LED, so we map green/blue/red
 * all to PA5. The two active blinky threads will both drive the same
 * pin, which is enough to demonstrate that the scheduler is running.
 */
#define LED_LD2_PIN  5U

static void led_on(void)  { GPIOA->BSRR = (1U << LED_LD2_PIN); }
static void led_off(void) { GPIOA->BSRR = (1U << (LED_LD2_PIN + 16U)); }

void SysTick_Handler(void) {
    OS_tick();

    __disable_irq();
    OS_sched();
    __enable_irq();
}

/* BMI088 DRDY EXTI vectors. Delegate to the driver, which clears the
 * pending bit and POSTs the right event to imuTask.
 */
void EXTI3_IRQHandler(void) { bmi088_exti_accel_isr(); }
void EXTI4_IRQHandler(void) { bmi088_exti_gyro_isr();  }

/* I2C1_RX DMA completion — drives the non-blocking sensor read path. */
void DMA1_Stream0_IRQHandler(void) { i2c1_dma_rx_isr(); }

/* Bring SYSCLK up to 168 MHz using the on-board 8 MHz HSE (NUCLEO-F446RE
 * has the ST-Link MCO routed to PH0/OSC_IN). PLL config matches the
 * SKYRTOS HAL setup: M=8, N=336, P=2, Q=2.
 *
 *   VCO_in  = HSE / M   = 8 MHz / 8   = 1 MHz   (must be 1..2 MHz)
 *   VCO_out = VCO_in*N  = 1 MHz * 336 = 336 MHz
 *   SYSCLK  = VCO_out/P = 336 MHz / 2 = 168 MHz
 *   HCLK    = SYSCLK    = 168 MHz
 *   APB1    = HCLK / 4  =  42 MHz   (max 45 MHz)
 *   APB2    = HCLK / 2  =  84 MHz   (max 90 MHz)
 *
 * RM0390 §6 (RCC) + §3.4.1 (Flash latency) cover this sequence.
 */
void SystemClock_Config(void) {
    /* PWR clock for voltage scaling register. Scale 1 is required for
     * SYSCLK > 144 MHz (RM0390 §5.1.4). */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_VOS;  /* 11b = scale 1 */

    /* Enable HSE, wait until stable. */
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0U) { }

    /* Configure PLL. PLLP=2 is encoded as 00 in PLLP bits. */
    RCC->PLLCFGR = (8U   << RCC_PLLCFGR_PLLM_Pos)
                 | (336U << RCC_PLLCFGR_PLLN_Pos)
                 | (0U   << RCC_PLLCFGR_PLLP_Pos)   /* /2 */
                 | RCC_PLLCFGR_PLLSRC_HSE
                 | (2U   << RCC_PLLCFGR_PLLQ_Pos);

    /* Enable PLL, wait lock. */
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0U) { }

    /* Bump flash latency BEFORE raising HCLK. 5 WS covers 150..168 MHz
     * at VOS scale 1 (RM0390 Table 5). Prefetch + caches on. */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN
               | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* Set bus prescalers BEFORE switching SYSCLK so APB1/APB2 never
     * see an overclocked transient. */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_HPRE_DIV1
              | RCC_CFGR_PPRE1_DIV4
              | RCC_CFGR_PPRE2_DIV2;

    /* Switch SYSCLK to PLL output, wait for status bit. */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }

    SystemCoreClockUpdate();
}

void BSP_init(void) {
    /* enable GPIOA clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA5 as general purpose output (MODER[11:10] = 01) */
    GPIOA->MODER &= ~(3U << (LED_LD2_PIN * 2U));
    GPIOA->MODER |=  (1U << (LED_LD2_PIN * 2U));

    /* push-pull, low speed, no pull-up/down (reset values are fine) */
    led_off();
}

void BSP_ledRedOn(void)    { led_on();  }
void BSP_ledRedOff(void)   { led_off(); }
void BSP_ledBlueOn(void)   { led_on();  }
void BSP_ledBlueOff(void)  { led_off(); }
void BSP_ledGreenOn(void)  { led_on();  }
void BSP_ledGreenOff(void) { led_off(); }

void OS_onStartup(void) {
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / BSP_TICKS_PER_SEC);

    /* highest priority for SysTick */
    NVIC_SetPriority(SysTick_IRQn, 0U);
}

void OS_onIdle(void) {
    __WFI();
}

_Noreturn void Q_onAssert(char const * const module, int const id) {
    (void)module;
    (void)id;
#ifndef NDEBUG
    for (;;) { }
#endif
    NVIC_SystemReset();
}

_Noreturn void assert_failed(char const * const module, int const id);
_Noreturn void assert_failed(char const * const module, int const id) {
    Q_onAssert(module, id);
}
