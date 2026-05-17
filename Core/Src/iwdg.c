#include "iwdg.h"
#include "main.h"

/* LSI ~32 kHz, prescaler /256, reload 249 → timeout ≈ (256*250)/32000 = 2 s */
static IWDG_HandleTypeDef hiwdg;

void MX_IWDG_Init(void)
{
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload    = 249;

    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        Error_Handler();
    }
}

void iwdg_feed(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}
