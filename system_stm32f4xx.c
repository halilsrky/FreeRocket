/* Minimal CMSIS system file for STM32F446.
 *
 * SystemInit() runs from the startup file before main(); we keep it
 * lean (FPU enable + reset RCC to known state + VTOR).
 *
 * SystemCoreClockUpdate() derives the current HCLK from RCC->CFGR /
 * RCC->PLLCFGR so it stays correct whether we're running on HSI
 * (16 MHz, post-reset) or HSE+PLL (168 MHz, after SystemClock_Config).
 */
#include "stm32f4xx.h"

#ifndef HSI_VALUE
#define HSI_VALUE   16000000U
#endif
#ifndef HSE_VALUE
#define HSE_VALUE   8000000U   /* NUCLEO-F446RE: ST-Link MCO @ 8 MHz */
#endif

uint32_t SystemCoreClock = HSI_VALUE;

const uint8_t AHBPrescTable[16] = {0,0,0,0,0,0,0,0,1,2,3,4,6,7,8,9};
const uint8_t APBPrescTable[8]  = {0,0,0,0,1,2,3,4};

void SystemInit(void) {
#if (__FPU_PRESENT == 1U) && (__FPU_USED == 1U)
    SCB->CPACR |= ((3U << 10U*2U) | (3U << 11U*2U)); /* CP10, CP11 full access */
#endif
    /* Reset RCC clock configuration to default reset state. */
    RCC->CR |= RCC_CR_HSION;
    RCC->CFGR = 0x00000000U;
    RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_CSSON | RCC_CR_PLLON);
    RCC->PLLCFGR = 0x24003010U;
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->CIR = 0x00000000U;

    /* Vector table is at flash base */
    SCB->VTOR = FLASH_BASE;
}

void SystemCoreClockUpdate(void) {
    uint32_t sysclk;

    switch (RCC->CFGR & RCC_CFGR_SWS) {
    case RCC_CFGR_SWS_HSI:
        sysclk = HSI_VALUE;
        break;
    case RCC_CFGR_SWS_HSE:
        sysclk = HSE_VALUE;
        break;
    case RCC_CFGR_SWS_PLL: {
        /* PLLVCO = (src / PLLM) * PLLN
         * SYSCLK = PLLVCO / PLLP, where PLLP encoding {00,01,10,11} = {2,4,6,8}
         */
        uint32_t pllm = (RCC->PLLCFGR & RCC_PLLCFGR_PLLM)
                      >> RCC_PLLCFGR_PLLM_Pos;
        uint32_t plln = (RCC->PLLCFGR & RCC_PLLCFGR_PLLN)
                      >> RCC_PLLCFGR_PLLN_Pos;
        uint32_t pllp = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLP)
                          >> RCC_PLLCFGR_PLLP_Pos) + 1U) * 2U;
        uint32_t src  = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC)
                      ? HSE_VALUE : HSI_VALUE;
        sysclk = ((src / pllm) * plln) / pllp;
        break;
    }
    default:
        sysclk = HSI_VALUE;
        break;
    }

    SystemCoreClock = sysclk >> AHBPrescTable[
        (RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos];
}
