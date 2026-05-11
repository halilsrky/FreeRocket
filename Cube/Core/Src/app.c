/*
 * app.c — MiROS bring-up, imuTask, HAL callback dispatchers.
 *
 * Architecture for this phase (single-task):
 *
 *   EXTI3 (PB3, ACC INT1)  --HAL_GPIO_EXTI_IRQHandler-->
 *       HAL_GPIO_EXTI_Callback(GPIO_PIN_3) -> POST EVT_ACCEL_DRDY
 *
 *   EXTI4 (PB4, GYRO INT3) --HAL_GPIO_EXTI_IRQHandler-->
 *       HAL_GPIO_EXTI_Callback(GPIO_PIN_4) -> POST EVT_GYRO_DRDY
 *
 *   imuThread receives DRDY -> kick HAL_I2C_Mem_Read_DMA(6 bytes)
 *   DMA1_Stream0 IRQ -> HAL_DMA_IRQHandler -> HAL_I2C_MemRxCpltCallback
 *       -> POST EVT_DMA_DONE
 *   imuThread wakes -> parse raw -> Mahony (TODO)
 *
 * Single I2C bus + single DMA stream => DRDY events must be serialised.
 * The state machine in main_imuThread tracks "what's currently in flight"
 * so accel and gyro never race.
 */
#include "app.h"
#include "main.h"
#include "i2c.h"
#include "bmi088.h"
#include "qassert.h"

/* ---- Thread storage ---------------------------------------------------- */

static OSThread  imuThread;
static uint32_t  stack_imu[512];   /* 2 kB: HAL I2C + FPU exception frames */

static OSThread  idleThreadStack;  /* Q_REQUIRE inside OS_init uses idleThread */
static uint32_t  stack_idle[64];   /* 256 B: sadece __WFI, ama IRQ frame lazım */

/* ---- Sensor sample buffers (DMA-targeted) ----------------------------- */

/* Aligned to make DMA happy and explicitly out of any cached region.
 * On F446 SRAM there's no D-cache, so this is just defensive. */
static uint8_t s_raw_accel[6] __attribute__((aligned(4)));
static uint8_t s_raw_gyro [6] __attribute__((aligned(4)));

/* What DMA transfer is currently outstanding. Set when we kick a read,
 * consumed in the EVT_DMA_DONE branch so we know which buffer to parse. */
typedef enum {
    IMU_IDLE       = 0,
    IMU_READ_ACCEL = 1,
    IMU_READ_GYRO  = 2,
} imu_phase_t;

static volatile imu_phase_t s_phase = IMU_IDLE;

/* Pending DRDY flags — if a DRDY fires while a DMA read is in progress,
 * we defer the kick until the in-flight read completes. */
static volatile uint8_t s_pending_accel;
static volatile uint8_t s_pending_gyro;

/* Latest decoded samples (read by future flightTask). */
static bmi088_vec3_t s_accel_g;
static bmi088_vec3_t s_gyro_dps;

/* ---- imuTask body ------------------------------------------------------ */

static void imu_kick_next(void)
{
    /* Choose the next pending read. Accel wins ties — both 400 Hz so
     * unlikely to backlog, and accel feeds gravity correction. */
    if (s_pending_accel) {
        s_pending_accel = 0U;
        s_phase = IMU_READ_ACCEL;
        if (bmi088_read_accel_dma(s_raw_accel) != HAL_OK) {
            s_phase = IMU_IDLE;
        }
    } else if (s_pending_gyro) {
        s_pending_gyro = 0U;
        s_phase = IMU_READ_GYRO;
        if (bmi088_read_gyro_dma(s_raw_gyro) != HAL_OK) {
            s_phase = IMU_IDLE;
        }
    }
}

static void main_imuThread(void)
{
    bmi088_bind(&hi2c1);

    bmi088_config_t cfg = {
        .acc_range      = BMI088_ACC_RANGE_24G,
        .acc_bwp        = BMI088_ACC_BWP_NORMAL,
        .acc_odr        = BMI088_ACC_ODR_400,
        .gyro_range     = BMI088_GYRO_RANGE_2000_DPS,
        .gyro_bandwidth = BMI088_GYRO_BW_47,
    };

    if (bmi088_init(&cfg) != BMI088_OK) {
        /* Park the thread on init failure. Visible via LED scope later. */
        while (1) { OS_delay(APP_TICKS_PER_SEC); }
    }

    while (1) {
        uint32_t evt = OS_evtWait(EVT_ACCEL_DRDY | EVT_GYRO_DRDY
                                | EVT_DMA_DONE  | EVT_I2C_ERROR);

        switch (evt) {
        case EVT_ACCEL_DRDY:
            s_pending_accel = 1U;
            if (s_phase == IMU_IDLE) imu_kick_next();
            break;

        case EVT_GYRO_DRDY:
            s_pending_gyro = 1U;
            if (s_phase == IMU_IDLE) imu_kick_next();
            break;

        case EVT_DMA_DONE:
            if (s_phase == IMU_READ_ACCEL) {
                bmi088_parse_accel(s_raw_accel, &s_accel_g);
            } else if (s_phase == IMU_READ_GYRO) {
                bmi088_parse_gyro(s_raw_gyro, &s_gyro_dps);
            }
            s_phase = IMU_IDLE;
            /* TODO: Mahony update here once both halves are fresh. */
            imu_kick_next();
            break;

        case EVT_I2C_ERROR:
            /* Recover by abandoning the in-flight transfer. Next DRDY
             * will kick a fresh read. */
            s_phase = IMU_IDLE;
            imu_kick_next();
            break;

        default:
            break;
        }
    }
}

/* ---- MiROS callbacks --------------------------------------------------- */

void OS_onStartup(void)
{
    /* HAL runs its tick off TIM6 so SysTick is ours. */
    SysTick_Config(SystemCoreClock / APP_TICKS_PER_SEC);
    NVIC_SetPriority(SysTick_IRQn, 0U);
}

void OS_onIdle(void)
{
    __WFI();
}

void SysTick_Handler(void)
{
    OS_tick();
    __disable_irq();
    OS_sched();
    __enable_irq();
}

/* ---- HAL callback overrides ------------------------------------------- */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_3) {
        OS_evtSignal_FromISR(&imuThread, EVT_ACCEL_DRDY);
    } else if (GPIO_Pin == GPIO_PIN_4) {
        OS_evtSignal_FromISR(&imuThread, EVT_GYRO_DRDY);
    }
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c1) {
        OS_evtSignal_FromISR(&imuThread, EVT_DMA_DONE);
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == &hi2c1) {
        OS_evtSignal_FromISR(&imuThread, EVT_I2C_ERROR);
    }
}

/* ---- Assertion failure handler (used by miros.c via qassert) --------- */

_Noreturn void Q_onAssert(char const * const module, int const id)
{
    (void)module;
    (void)id;
    __disable_irq();
    for (;;) { }
}

/* ---- Entry point — called from main.c USER CODE 2 block -------------- */

void Application_Start(void)
{
    (void)idleThreadStack;  /* keep symbol — currently storage only */

    /* MiROS PendSV only saves r4-r11, not FPU registers. Disabling automatic
     * FPU context preservation (ASPEN+LSPEN=0) keeps exception frames basic
     * (32 bytes) even when float is used. Safe as long as no ISR uses FPU. */
    FPU->FPCCR &= ~(FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk);

    OS_init(stack_idle, sizeof(stack_idle));

    OSThread_start(&imuThread, 5U, &main_imuThread,
                   stack_imu, sizeof(stack_imu));

    OS_run();  /* never returns */
}
