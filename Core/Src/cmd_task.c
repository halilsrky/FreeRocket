/*
 * cmd_task.c — USART2 RX komut ayrıştırıcı
 *
 * İki paket tipi:
 *   CMD (5 byte)  : 0xAA | cmd | chk | 0x0D | 0x0A
 *   SUT (36 byte) : 0xAB | alt(4) | press(4) | ax(4) | ay(4) | az(4)
 *                         | gx(4) | gy(4) | gz(4) | chk | 0x0D | 0x0A
 *
 * cmd değerleri:
 *   0x00 → NORMAL   0x01 → SIT   0x02 → SUT
 */

#include "cmd_task.h"
#include "sys_mode.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdint.h>

extern DMA_HandleTypeDef hdma_usart2_rx;

#define CMD_HEADER  0xAAu
#define SUT_HEADER  0xABu
#define FOOTER1     0x0Du
#define FOOTER2     0x0Au

#define CMD_SIT     0x20u
#define CMD_SUT     0x22u
#define CMD_STOP    0x24u

#define RX_BUF_LEN  40U   /* en büyük paket 36 byte */

static uint8_t      s_rx_buf[RX_BUF_LEN];
static TaskHandle_t s_handle;

/* PC big-endian (MSB önce) → ARM little-endian float */
static float b2f(const uint8_t *b)
{
    union { float f; uint8_t b[4]; } u;
    u.b[3] = b[0];
    u.b[2] = b[1];
    u.b[1] = b[2];
    u.b[0] = b[3];
    return u.f;
}

static void parse_packet(const uint8_t *buf, uint16_t len)
{
    if (len == 5u && buf[0] == CMD_HEADER &&
        buf[3] == FOOTER1 && buf[4] == FOOTER2)
    {
        switch (buf[1]) {
            case CMD_SIT:  sys_mode_set(MODE_SIT);    break;
            case CMD_SUT:  sys_mode_set(MODE_SUT);    break;
            case CMD_STOP: sys_mode_set(MODE_NORMAL); break;
            default: break;
        }
        return;
    }

    if (len == 36u && buf[0] == SUT_HEADER &&
        buf[34] == FOOTER1 && buf[35] == FOOTER2)
    {
        uint8_t csum = 0u;
        for (uint8_t i = 0u; i < 33u; i++) csum += buf[i];
        if (csum != buf[33]) return;

        sut_data_t d = {
            .altitude = b2f(&buf[1]),
            .pressure = b2f(&buf[5]),
            .accel_x  = b2f(&buf[9]),
            .accel_y  = b2f(&buf[13]),
            .accel_z  = -b2f(&buf[17]),
            .gyro_x   = b2f(&buf[21]),
            .gyro_y   = b2f(&buf[25]),
            .gyro_z   = b2f(&buf[29]),
        };
        sys_mode_sut_put(&d);
    }
}

static void cmd_task(void *arg)
{
    (void)arg;
    s_handle = xTaskGetCurrentTaskHandle();

    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, s_rx_buf, RX_BUF_LEN);
    __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);  /* yalnızca IDLE tetiklesin */

    for (;;) {
        uint32_t len = 0u;
        xTaskNotifyWait(0u, UINT32_MAX, &len, portMAX_DELAY);

        if (len > 0u && len <= RX_BUF_LEN) {
            parse_packet(s_rx_buf, (uint16_t)len);
        }

        HAL_UARTEx_ReceiveToIdle_DMA(&huart2, s_rx_buf, RX_BUF_LEN);
        __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);
    }
}

/* USART2 IDLE IRQ → task notification */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART2 || s_handle == NULL) return;

    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_handle, (uint32_t)Size, eSetValueWithOverwrite, &woken);
    portYIELD_FROM_ISR(woken);
}

void cmd_task_create(void)
{
    static const osThreadAttr_t attr = {
        .name       = "CMD",
        .stack_size = 256 * 4,
        .priority   = osPriorityNormal,
    };
    osThreadNew(cmd_task, NULL, &attr);
}
