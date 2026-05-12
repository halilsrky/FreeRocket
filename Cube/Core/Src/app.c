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
#include "usart.h"
#include "bmi088.h"
#include "mahony.h"
#include "qassert.h"
#include <stdio.h>
#include <string.h>

/* ---- Thread storage ---------------------------------------------------- */

/* Stacks declared BEFORE their OSThread TCBs so that a stack overflow grows
 * into lower-addressed BSS variables rather than directly into the TCB.
 * (Linker places BSS in declaration order: stack_imu[0] is the low-address
 * end, imuThread sits at a higher address, above the danger zone.) */
static uint32_t  stack_imu[1024];  /* 4 kB: HAL I2C + FPU frames + libm (atan2f/acosf/snprintf) depth */
static OSThread  imuThread;

static uint32_t  stack_idle[64];   /* 256 B: sadece __WFI, IRQ frame lazım */
static OSThread  idleThreadStack;

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

/* Mahony pairing: a fusion step needs one fresh accel AND one fresh gyro.
 * Halves arrive on separate DRDYs, so we set the flag in the parse branch
 * and consume both when the second one lands. */
static uint8_t s_have_accel;
static uint8_t s_have_gyro;

/* dt for Mahony. BMI088 acc ODR = 400 Hz, gyro at the matching 400 Hz
 * setting; assuming equal-rate pairing, dt = 1/400 = 2.5 ms. TODO: replace
 * with a microsecond timer once TIM2 is wired up. */
#define MAHONY_DT_S         (1.0f / 400.0f)

/* UART2 telemetry: format strings need printf, but we keep all values
 * scaled-int to skip the libc float-printf dependency. Quaternion x10000,
 * Euler x100, gyro-only flag as 0/1. One line per send, drop on busy. */
#define TELEM_DECIMATE      4U      /* every 4th Mahony update -> 100 Hz */
static char    s_telem_buf[80];
static uint8_t s_telem_div;

/* ---- imuTask body ------------------------------------------------------ */

/* Try to push one telemetry line over UART2 via DMA. Drops the sample
 * silently if the previous transfer is still in flight — for streaming
 * telemetry losing one packet is preferable to blocking the imuThread. */
static void telem_try_send(void)
{
    if (huart2.gState != HAL_UART_STATE_READY) {
        return;
    }

    mahony_quat_t  q = mahony_get_quat();
    mahony_euler_t e = mahony_get_euler();

    int16_t qw = (int16_t)(q.w * 10000.0f);
    int16_t qx = (int16_t)(q.x * 10000.0f);
    int16_t qy = (int16_t)(q.y * 10000.0f);
    int16_t qz = (int16_t)(q.z * 10000.0f);
    int16_t er = (int16_t)(e.roll  * 100.0f);
    int16_t ep = (int16_t)(e.pitch * 100.0f);
    int16_t ey = (int16_t)(e.yaw   * 100.0f);
    uint8_t gm = mahony_is_gyro_only() ? 1U : 0U;

    int n = snprintf(s_telem_buf, sizeof(s_telem_buf),
                     "q,%d,%d,%d,%d,e,%d,%d,%d,m,%u\r\n",
                     qw, qx, qy, qz, er, ep, ey, (unsigned)gm);
    if (n > 0 && (size_t)n < sizeof(s_telem_buf)) {
        (void)HAL_UART_Transmit_DMA(&huart2, (uint8_t *)s_telem_buf, (uint16_t)n);
    }
}

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
    mahony_reset();

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
                s_have_accel = 1U;
            } else if (s_phase == IMU_READ_GYRO) {
                bmi088_parse_gyro(s_raw_gyro, &s_gyro_dps);
                s_have_gyro = 1U;
            }
            s_phase = IMU_IDLE;

            /* Both halves fresh -> one Mahony step + throttled telemetry. */
            if (s_have_accel && s_have_gyro) {
                s_have_accel = 0U;
                s_have_gyro  = 0U;
                mahony_update(s_gyro_dps.x, s_gyro_dps.y, s_gyro_dps.z,
                              s_accel_g.x, s_accel_g.y, s_accel_g.z,
                              MAHONY_DT_S);
                if (++s_telem_div >= TELEM_DECIMATE) {
                    s_telem_div = 0U;
                    telem_try_send();
                }
            }

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

    OS_init(stack_idle, sizeof(stack_idle));

    OSThread_start(&imuThread, 5U, &main_imuThread,
                   stack_imu, sizeof(stack_imu));

    OS_run();  /* never returns */
}
