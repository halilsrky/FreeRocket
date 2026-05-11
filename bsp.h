#ifndef __BSP_H__
#define __BSP_H__

/* system clock tick [Hz] */
#define BSP_TICKS_PER_SEC 100U

/* HSE + PLL → 168 MHz SYSCLK, 42 MHz APB1, 84 MHz APB2.
 * Must be called before BSP_init() / any peripheral setup so clocks
 * and flash latency match what the rest of the driver code assumes. */
void SystemClock_Config(void);

void BSP_init(void);

void BSP_ledRedOn(void);
void BSP_ledRedOff(void);

void BSP_ledBlueOn(void);
void BSP_ledBlueOff(void);

void BSP_ledGreenOn(void);
void BSP_ledGreenOff(void);

#endif // __BSP_H__
