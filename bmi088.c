/*
 * bmi088.c — BMI088 IMU driver implementation (mini RTOS phase 1).
 */
#include "bmi088.h"
#include "i2c1.h"
#include "stm32f446xx.h"

/* ---- Thread / event binding -------------------------------------------- */

static OSThread *s_imu_thread;
static uint32_t  s_evt_accel_drdy;
static uint32_t  s_evt_gyro_drdy;

void bmi088_attach_thread(OSThread *t,
                          uint32_t accel_drdy_evt,
                          uint32_t gyro_drdy_evt)
{
    s_imu_thread     = t;
    s_evt_accel_drdy = accel_drdy_evt;
    s_evt_gyro_drdy  = gyro_drdy_evt;
}

/* ---- Boot-only busy-wait delay (scheduler not running yet) ------------- */

static void delay_busy_us(uint32_t us)
{
    /* Rough: 4 cycles per loop iteration on Cortex-M4 (NOP + branch).
     * Used only during init when no scheduler exists.
     */
    uint32_t loops = (SystemCoreClock / 4000000U) * us;
    while (loops--) { __asm volatile ("nop"); }
}

static void delay_busy_ms(uint32_t ms)
{
    while (ms--) { delay_busy_us(1000U); }
}

/* ---- Low-level register helpers ---------------------------------------- */

static bmi088_status_t acc_write(uint8_t reg, uint8_t val)
{
    return (i2c1_mem_write_byte(BMI088_ACC_I2C_ADDR, reg, val) == I2C1_OK)
           ? BMI088_OK : BMI088_ERR_I2C;
}

static bmi088_status_t gyro_write(uint8_t reg, uint8_t val)
{
    return (i2c1_mem_write_byte(BMI088_GYRO_I2C_ADDR, reg, val) == I2C1_OK)
           ? BMI088_OK : BMI088_ERR_I2C;
}

static bmi088_status_t acc_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return (i2c1_mem_read(BMI088_ACC_I2C_ADDR, reg, buf, len) == I2C1_OK)
           ? BMI088_OK : BMI088_ERR_I2C;
}

static bmi088_status_t gyro_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return (i2c1_mem_read(BMI088_GYRO_I2C_ADDR, reg, buf, len) == I2C1_OK)
           ? BMI088_OK : BMI088_ERR_I2C;
}

/* ---- Probe ------------------------------------------------------------- */

bmi088_status_t bmi088_probe(uint8_t *out_acc_id, uint8_t *out_gyro_id)
{
    uint8_t id;
    bmi088_status_t st;

    /* The BMI088 accel datasheet notes the very first I2C read after
     * power-on can return garbage; one dummy read is enough. */
    (void)acc_read(BMI088_ACC_CHIP_ID, &id, 1U);

    st = acc_read(BMI088_ACC_CHIP_ID, &id, 1U);
    if (st != BMI088_OK) return st;
    if (out_acc_id) *out_acc_id = id;
    if (id != BMI088_ACC_CHIP_ID_VAL) return BMI088_ERR_ACC_ID;

    st = gyro_read(BMI088_GYRO_CHIP_ID, &id, 1U);
    if (st != BMI088_OK) return st;
    if (out_gyro_id) *out_gyro_id = id;
    if (id != BMI088_GYRO_CHIP_ID_VAL) return BMI088_ERR_GYRO_ID;

    return BMI088_OK;
}

/* ---- EXTI / SYSCFG configuration --------------------------------------- */

static void exti_setup_pb3_pb4_rising(void)
{
    /* GPIOB clock (idempotent — i2c1_init also enables it) */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* PB3, PB4 as input, no pull (BMI088 INT pins are push-pull
     * active-high in our config, so MCU pull is unwanted). */
    GPIOB->MODER &= ~(GPIO_MODER_MODER3 | GPIO_MODER_MODER4);
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR3 | GPIO_PUPDR_PUPDR4);

    /* SYSCFG clock for EXTI line muxing */
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;

    /* Route EXTI3 / EXTI4 to port B. */
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~SYSCFG_EXTICR1_EXTI3_Msk)
                      | SYSCFG_EXTICR1_EXTI3_PB;
    SYSCFG->EXTICR[1] = (SYSCFG->EXTICR[1] & ~SYSCFG_EXTICR2_EXTI4_Msk)
                      | SYSCFG_EXTICR2_EXTI4_PB;

    /* Rising edge, unmask interrupt, no event */
    EXTI->RTSR |=  (EXTI_RTSR_TR3 | EXTI_RTSR_TR4);
    EXTI->FTSR &= ~(EXTI_FTSR_TR3 | EXTI_FTSR_TR4);
    EXTI->IMR  |=  (EXTI_IMR_MR3  | EXTI_IMR_MR4);

    /* NVIC priority 5 (matches SKYRTOS), enable */
    NVIC_SetPriority(EXTI3_IRQn, 5U);
    NVIC_SetPriority(EXTI4_IRQn, 5U);
    NVIC_EnableIRQ(EXTI3_IRQn);
    NVIC_EnableIRQ(EXTI4_IRQn);
}

/* ---- Init -------------------------------------------------------------- */

bmi088_status_t bmi088_init(const bmi088_config_t *cfg)
{
    bmi088_status_t st;

    if (cfg == (const bmi088_config_t *)0) return BMI088_ERR_I2C;

    /* I2C must already be up (i2c1_init() called by the caller). */
    st = bmi088_probe((uint8_t *)0, (uint8_t *)0);
    if (st != BMI088_OK) return st;

    /* ---- Soft-reset both halves, then re-probe -------------------- */
    (void)acc_write(BMI088_ACC_SOFTRESET, BMI088_ACC_RESET_CMD);
    delay_busy_ms(2U);
    (void)gyro_write(BMI088_GYRO_SOFT_RESET, BMI088_GYRO_RESET_CMD);
    delay_busy_ms(50U);  /* gyro reset min 30 ms per datasheet */

    /* Accel needs an extra dummy I2C read after reset (datasheet) */
    uint8_t dummy;
    (void)acc_read(BMI088_ACC_CHIP_ID, &dummy, 1U);
    delay_busy_ms(2U);

    /* ---- Accelerometer config ------------------------------------- */
    st = acc_write(BMI088_ACC_PWR_CONF, BMI088_ACC_PWR_SAVE_ACTIVE);
    if (st != BMI088_OK) return st;
    delay_busy_ms(5U);

    st = acc_write(BMI088_ACC_PWR_CTRL, BMI088_ACC_ENABLE);
    if (st != BMI088_OK) return st;
    delay_busy_ms(50U);  /* accel needs ~50 ms to start ODR after enable */

    st = acc_write(BMI088_ACC_RANGE, cfg->acc_range);
    if (st != BMI088_OK) return st;

    st = acc_write(BMI088_ACC_CONF, (uint8_t)((cfg->acc_bwp << 4) | cfg->acc_odr));
    if (st != BMI088_OK) return st;

    /* INT1 pin: push-pull, active high, output enable */
    st = acc_write(BMI088_ACC_INT1_IO_CTRL,
                   BMI088_ACC_INT1_OUT_EN |
                   BMI088_ACC_INT1_PUSH_PULL |
                   BMI088_ACC_INT1_ACTIVE_HIGH);
    if (st != BMI088_OK) return st;

    /* Map data-ready to INT1 pin */
    st = acc_write(BMI088_ACC_INT_MAP_DATA, BMI088_ACC_INT_MAP_DRDY_INT1);
    if (st != BMI088_OK) return st;

    /* ---- Gyroscope config ----------------------------------------- */
    st = gyro_write(BMI088_GYRO_RANGE, cfg->gyro_range);
    if (st != BMI088_OK) return st;

    st = gyro_write(BMI088_GYRO_BANDWIDTH, cfg->gyro_bandwidth);
    if (st != BMI088_OK) return st;

    st = gyro_write(BMI088_GYRO_LPM1, BMI088_GYRO_LPM_NORMAL);
    if (st != BMI088_OK) return st;
    delay_busy_ms(30U);  /* gyro power-up settle */

    st = gyro_write(BMI088_GYRO_INT_CTRL, BMI088_GYRO_INT_ENABLE);
    if (st != BMI088_OK) return st;

    /* INT3 pin: push-pull, active high (INT4 mirror tolerated) */
    st = gyro_write(BMI088_GYRO_INT3_4_IO_CONF,
                    BMI088_GYRO_INT3_PUSH_PULL |
                    BMI088_GYRO_INT3_ACTIVE_HIGH |
                    BMI088_GYRO_INT4_ACTIVE_HIGH);
    if (st != BMI088_OK) return st;

    /* Map DRDY to both INT3 and INT4 — defensive (matches SKYRTOS).
     * The board only wires one of them to the MCU (PB4 in our case).
     */
    st = gyro_write(BMI088_GYRO_INT3_4_IO_MAP, BMI088_GYRO_INT_MAP_DRDY_BOTH);
    if (st != BMI088_OK) return st;

    /* ---- EXTI on PB3 / PB4 ---------------------------------------- */
    exti_setup_pb3_pb4_rising();

    return BMI088_OK;
}

/* ---- ISR entry points -------------------------------------------------- */

void bmi088_exti_accel_isr(void)
{
    /* Clear pending bit (write 1 to clear) */
    EXTI->PR = EXTI_PR_PR3;
    if (s_imu_thread != (OSThread *)0) {
        OS_evtSignal_FromISR(s_imu_thread, s_evt_accel_drdy);
    }
}

void bmi088_exti_gyro_isr(void)
{
    EXTI->PR = EXTI_PR_PR4;
    if (s_imu_thread != (OSThread *)0) {
        OS_evtSignal_FromISR(s_imu_thread, s_evt_gyro_drdy);
    }
}
