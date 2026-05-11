/*
 * i2c1.c — Bare-metal I2C1 driver for STM32F446.
 *
 * Reference: STM32F446 RM0390, §27 "I2C interface" (V1 peripheral).
 * Master-receiver sequencing follows §27.3.3 ("Method 2" / single-byte
 * and N>=2 split paths) — STM32F4 I2C V1 needs different STOP/NACK
 * choreography depending on payload length, which is why mem_read has
 * three branches.
 */
#include "i2c1.h"
#include "stm32f446xx.h"

/* Loop bound for blocking flag polls. At HSI 16 MHz this is ~10 ms;
 * at 168 MHz / 42 MHz APB1 closer to ~1 ms. Generous enough to ride out
 * a slow startup, tight enough to fail fast on a stuck bus.
 */
#define I2C1_TIMEOUT_LOOPS  100000U

/* Read APB1 clock from RCC settings so this works on both HSI 16 MHz
 * and HSE+PLL 168 MHz/APB1=42 MHz without recompilation.
 */
static uint32_t apb1_clock_hz(void)
{
    static const uint8_t apb1_div_table[8] = {1, 1, 1, 1, 2, 4, 8, 16};
    uint32_t ppre1 = (RCC->CFGR >> RCC_CFGR_PPRE1_Pos) & 0x7U;
    return SystemCoreClock / apb1_div_table[ppre1];
}

void i2c1_init(void)
{
    /* GPIOB + DMA1 clocks (AHB1) */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN;
    /* I2C1 clock */
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    /* short delay after enabling clock (ARM erratum-style read-back) */
    (void)RCC->APB1ENR;

    /* PB8/PB9 = I2C1_SCL/SDA: AF4, open-drain, very-high speed, no pull. */
    GPIOB->MODER   = (GPIOB->MODER & ~(GPIO_MODER_MODER8 | GPIO_MODER_MODER9))
                   | (GPIO_MODER_MODER8_1 | GPIO_MODER_MODER9_1);     /* AF mode */
    GPIOB->OTYPER |= GPIO_OTYPER_OT_8 | GPIO_OTYPER_OT_9;              /* open-drain */
    GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR8 | GPIO_OSPEEDER_OSPEEDR9; /* very high */
    GPIOB->PUPDR  &= ~(GPIO_PUPDR_PUPDR8 | GPIO_PUPDR_PUPDR9);         /* no pull */
    GPIOB->AFR[1]  = (GPIOB->AFR[1] & ~(GPIO_AFRH_AFSEL8 | GPIO_AFRH_AFSEL9))
                   | (GPIO_AFRH_AFSEL8_2 | GPIO_AFRH_AFSEL9_2);        /* AF4 = 0100 */

    /* Reset peripheral via SWRST so a previous wedged state is cleared. */
    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0U;

    uint32_t pclk1 = apb1_clock_hz();
    uint32_t pclk1_mhz = pclk1 / 1000000U;

    /* CR2: peripheral input clock in MHz (must be 2..50 for V1). */
    I2C1->CR2 = pclk1_mhz & 0x3FU;
    /* CCR for 100 kHz standard mode: CCR = pclk1 / (2 * 100k). */
    I2C1->CCR = pclk1 / (2U * 100000U);
    /* TRISE: max rise time. SM = 1000 ns => trise = pclk1_mhz + 1. */
    I2C1->TRISE = pclk1_mhz + 1U;

    /* Enable peripheral, ACK on, no POS, 7-bit addressing default. */
    I2C1->CR1 = I2C_CR1_PE | I2C_CR1_ACK;

    /* DMA1 Stream0 NVIC enable. Priority 6 (below SysTick=0 and EXTI=5
     * so DMA completion can't preempt the SysTick scheduler tick). */
    NVIC_SetPriority(DMA1_Stream0_IRQn, 6U);
    NVIC_EnableIRQ(DMA1_Stream0_IRQn);
}

/* Force STOP and clear AF/BERR/ARLO — recovers the peripheral state
 * without forcing a full SWRST every time.
 */
static void abort_sequence(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
    I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
    /* re-enable ACK, clear POS for next sequence */
    I2C1->CR1 = (I2C1->CR1 & ~I2C_CR1_POS) | I2C_CR1_ACK;
}

/* Wait for `cond` to become true, with NACK + timeout escape hatches.
 * Returns I2C1_OK if cond is satisfied, otherwise reports the failure
 * reason and aborts the on-bus sequence.
 */
#define WAIT_OR_FAIL(cond)                                                  \
    do {                                                                    \
        uint32_t _to = I2C1_TIMEOUT_LOOPS;                                  \
        while (!(cond)) {                                                   \
            if ((I2C1->SR1 & I2C_SR1_AF) != 0U) {                           \
                abort_sequence();                                           \
                return I2C1_ERR_NACK;                                       \
            }                                                               \
            if (--_to == 0U) {                                              \
                abort_sequence();                                           \
                return I2C1_ERR_TIMEOUT;                                    \
            }                                                               \
        }                                                                   \
    } while (0)

i2c1_status_t i2c1_mem_write_byte(uint8_t dev_addr_8bit, uint8_t reg, uint8_t value)
{
    /* Wait until any previous transfer is fully off the bus. */
    WAIT_OR_FAIL((I2C1->SR2 & I2C_SR2_BUSY) == 0U);

    /* START */
    I2C1->CR1 |= I2C_CR1_START;
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_SB) != 0U);

    /* Address (write) — clears SB by writing DR. */
    I2C1->DR = dev_addr_8bit & 0xFEU;
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_ADDR) != 0U);
    (void)I2C1->SR2;  /* clear ADDR by reading SR1 (above) then SR2 */

    /* Register pointer */
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_TXE) != 0U);
    I2C1->DR = reg;

    /* Payload */
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_TXE) != 0U);
    I2C1->DR = value;

    /* Wait for last byte fully shifted out */
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_BTF) != 0U);

    /* STOP */
    I2C1->CR1 |= I2C_CR1_STOP;
    return I2C1_OK;
}

i2c1_status_t i2c1_mem_read(uint8_t dev_addr_8bit, uint8_t reg,
                            uint8_t *buf, uint16_t len)
{
    if ((buf == (uint8_t *)0) || (len == 0U)) {
        return I2C1_ERR_ARG;
    }

    WAIT_OR_FAIL((I2C1->SR2 & I2C_SR2_BUSY) == 0U);

    /* Phase 1: write register pointer (master-transmitter) */
    I2C1->CR1 |= I2C_CR1_ACK;
    I2C1->CR1 |= I2C_CR1_START;
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_SB) != 0U);

    I2C1->DR = dev_addr_8bit & 0xFEU;  /* write mode */
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_ADDR) != 0U);
    (void)I2C1->SR2;

    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_TXE) != 0U);
    I2C1->DR = reg;
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_BTF) != 0U);

    /* Phase 2: repeated START → master-receiver. The N=1, N=2, N>=3
     * branches differ because the I2C V1 peripheral cannot NACK the
     * last received byte without explicit ACK/POS choreography around
     * the ADDR-clear step (RM0390 §27.3.3).
     */
    I2C1->CR1 |= I2C_CR1_START;
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_SB) != 0U);

    I2C1->DR = dev_addr_8bit | 0x01U;  /* read mode */
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_ADDR) != 0U);

    if (len == 1U) {
        /* For 1 byte: clear ACK BEFORE clearing ADDR, then STOP. */
        I2C1->CR1 &= ~I2C_CR1_ACK;
        __disable_irq();
        (void)I2C1->SR2;                  /* clear ADDR */
        I2C1->CR1 |= I2C_CR1_STOP;
        __enable_irq();
        WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_RXNE) != 0U);
        buf[0] = (uint8_t)I2C1->DR;
    }
    else if (len == 2U) {
        /* For 2 bytes: POS=1 (NACK the next byte), ACK=0 BEFORE clearing
         * ADDR. After BTF, both bytes are pending — STOP, then read 2x DR.
         */
        I2C1->CR1 |= I2C_CR1_POS;
        I2C1->CR1 &= ~I2C_CR1_ACK;
        __disable_irq();
        (void)I2C1->SR2;                  /* clear ADDR */
        __enable_irq();
        WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_BTF) != 0U);
        __disable_irq();
        I2C1->CR1 |= I2C_CR1_STOP;
        buf[0] = (uint8_t)I2C1->DR;
        __enable_irq();
        buf[1] = (uint8_t)I2C1->DR;
        I2C1->CR1 &= ~I2C_CR1_POS;
    }
    else {
        /* N>=3: read first N-3 bytes via RXNE, then choreograph the last
         * three around BTF/STOP/NACK to avoid losing the shift register
         * contents.
         */
        (void)I2C1->SR2;                  /* clear ADDR with ACK=1 */
        uint16_t i = 0U;
        while (i < (uint16_t)(len - 3U)) {
            WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_RXNE) != 0U);
            buf[i++] = (uint8_t)I2C1->DR;
            /* If BTF asserted while we were waiting, drain immediately
             * to keep the shift register flowing. */
            if ((I2C1->SR1 & I2C_SR1_BTF) != 0U) {
                buf[i++] = (uint8_t)I2C1->DR;
            }
        }
        /* Three bytes left: wait BTF (DR + shift register full). */
        WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_BTF) != 0U);
        I2C1->CR1 &= ~I2C_CR1_ACK;        /* will NACK the next byte */
        __disable_irq();
        buf[i++] = (uint8_t)I2C1->DR;     /* N-3 */
        I2C1->CR1 |= I2C_CR1_STOP;
        buf[i++] = (uint8_t)I2C1->DR;     /* N-2 */
        __enable_irq();
        WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_RXNE) != 0U);
        buf[i++] = (uint8_t)I2C1->DR;     /* N-1 */
    }

    /* Restore default ACK=1 for the next sequence. */
    I2C1->CR1 |= I2C_CR1_ACK;
    return I2C1_OK;
}

/* ============================================================================
 *  DMA-driven receive path (DMA1 Stream0, Channel 1 = I2C1_RX)
 * ============================================================================
 *
 * Sequence (RM0390 §27.6.4 "Reception using DMA"):
 *
 *   1. Program DMA Stream0 (P→M, byte, MINC=1, len) but do NOT enable yet.
 *   2. Set I2C1_CR2.LAST so the peripheral NACKs the byte coincident with
 *      DMA EOT — this terminates the master-receiver cleanly.
 *   3. Set I2C1_CR2.DMAEN so RXNE triggers a DMA request.
 *   4. Polled phase 1: START, addr+W, reg byte. Wait BTF.
 *   5. Repeated START, addr+R, clear ADDR (SR1+SR2 read).
 *   6. Enable DMA Stream — it now drains DR into `buf`.
 *   7. On DMA TC interrupt: send STOP, tear down stream + flags, POST event.
 */

static OSThread     *s_dma_rx_thread;
static uint32_t      s_dma_rx_evt;
static volatile i2c1_status_t s_dma_rx_status = I2C1_OK;
static volatile uint8_t       s_dma_rx_busy   = 0U;

void i2c1_attach_rx_thread(OSThread *t, uint32_t done_evt)
{
    s_dma_rx_thread = t;
    s_dma_rx_evt    = done_evt;
}

i2c1_status_t i2c1_dma_rx_result(void)
{
    return s_dma_rx_status;
}

/* Clear all Stream0 flags in DMA1 LISR via LIFCR. */
static void dma_rx_clear_flags(void)
{
    DMA1->LIFCR = DMA_LIFCR_CTCIF0 | DMA_LIFCR_CHTIF0
                | DMA_LIFCR_CTEIF0 | DMA_LIFCR_CDMEIF0
                | DMA_LIFCR_CFEIF0;
}

i2c1_status_t i2c1_mem_read_dma_start(uint8_t dev_addr_8bit, uint8_t reg,
                                      uint8_t *buf, uint16_t len)
{
    if ((buf == (uint8_t *)0) || (len < 3U)) {
        return I2C1_ERR_ARG;
    }
    if (s_dma_rx_busy != 0U) {
        return I2C1_ERR_BUSY;
    }

    WAIT_OR_FAIL((I2C1->SR2 & I2C_SR2_BUSY) == 0U);

    /* ---- Program DMA Stream0 (but don't enable yet) -------------------- */
    DMA1_Stream0->CR = 0U;                 /* disable + reset */
    while ((DMA1_Stream0->CR & DMA_SxCR_EN) != 0U) { }
    dma_rx_clear_flags();

    DMA1_Stream0->PAR  = (uint32_t)&I2C1->DR;
    DMA1_Stream0->M0AR = (uint32_t)buf;
    DMA1_Stream0->NDTR = len;
    DMA1_Stream0->CR =
          (1U << DMA_SxCR_CHSEL_Pos)       /* Channel 1 = I2C1_RX */
        | (2U << DMA_SxCR_PL_Pos)          /* priority high */
        | DMA_SxCR_MINC                    /* memory increment */
        | DMA_SxCR_TCIE                    /* TC interrupt */
        | DMA_SxCR_TEIE;                   /* transfer-error interrupt */
    /* DIR = 00 (P→M), PSIZE/MSIZE = 00 (byte), CIRC = 0 — all left at 0. */

    /* ---- Phase 1: master-transmitter, send register pointer ------------ */
    I2C1->CR1 |= I2C_CR1_ACK;
    I2C1->CR1 |= I2C_CR1_START;
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_SB) != 0U);

    I2C1->DR = dev_addr_8bit & 0xFEU;       /* write mode */
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_ADDR) != 0U);
    (void)I2C1->SR2;

    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_TXE) != 0U);
    I2C1->DR = reg;
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_BTF) != 0U);

    /* ---- Phase 2 setup: arm LAST + DMAEN, repeated START, addr+R ------- */
    I2C1->CR2 |= I2C_CR2_LAST | I2C_CR2_DMAEN;

    I2C1->CR1 |= I2C_CR1_START;
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_SB) != 0U);

    I2C1->DR = dev_addr_8bit | 0x01U;       /* read mode */
    WAIT_OR_FAIL((I2C1->SR1 & I2C_SR1_ADDR) != 0U);
    (void)I2C1->SR2;                        /* clear ADDR */

    /* ---- Hand off to DMA ----------------------------------------------- */
    s_dma_rx_status = I2C1_OK;
    s_dma_rx_busy   = 1U;
    DMA1_Stream0->CR |= DMA_SxCR_EN;

    return I2C1_OK;
}

void i2c1_dma_rx_isr(void)
{
    uint32_t lisr = DMA1->LISR;

    /* Stop master-receiver. With LAST set, the peripheral already NACKed
     * the final byte; we just need to release the bus. */
    I2C1->CR1 |= I2C_CR1_STOP;

    /* Tear down stream and I2C DMA hooks regardless of TC/TE — we're done. */
    DMA1_Stream0->CR &= ~DMA_SxCR_EN;
    while ((DMA1_Stream0->CR & DMA_SxCR_EN) != 0U) { }
    I2C1->CR2 &= ~(I2C_CR2_DMAEN | I2C_CR2_LAST);

    if ((lisr & DMA_LISR_TEIF0) != 0U) {
        s_dma_rx_status = I2C1_ERR_DMA;
    } else if ((lisr & DMA_LISR_TCIF0) != 0U) {
        s_dma_rx_status = I2C1_OK;
    } else {
        /* Spurious — shouldn't happen with TCIE/TEIE only. */
        s_dma_rx_status = I2C1_ERR_DMA;
    }

    dma_rx_clear_flags();
    s_dma_rx_busy = 0U;

    if (s_dma_rx_thread != (OSThread *)0) {
        OS_evtSignal_FromISR(s_dma_rx_thread, s_dma_rx_evt);
    }
}
