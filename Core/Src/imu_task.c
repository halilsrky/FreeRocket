#include "imu_task.h"
#include "sys_mode.h"
#include "bmi088_defs.h"
#include "imu_snapshot.h"
#include "bmi088.h"
#include "mahony.h"
#include "main.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include <stdbool.h>

/* ── Notification bit masks ── */
#define NOTIFY_ACC_DRDY    (1U << 0)
#define NOTIFY_GYRO_DRDY   (1U << 1)
#define NOTIFY_DMA_DONE    (1U << 2)
#define NOTIFY_I2C_ERROR   (1U << 3)
#define NOTIFY_SUT_BATCH   (1U << 4)

/* ── Sensor config ── */
static const bmi088_config_t k_bmi_cfg = {
    .hi2c       = &hi2c1,
    .acc_range  = ACC_RANGE_12G,
    .acc_odr    = ACC_ODR_100,
    .gyro_range = GYRO_RANGE_2000DPS,
    .gyro_bw    = GYRO_BW_12HZ_ODR100,
};

/* ── Module-private state ── */
typedef enum { DMA_IDLE, DMA_READING_ACC, DMA_READING_GYRO } dma_state_t;

static TaskHandle_t  s_handle;
static QueueHandle_t s_snapshot_q;   /* depth=1 — daima en güncel snapshot */
static uint8_t       s_acc_buf[6];
static uint8_t       s_gyro_buf[6];

/* ── Forward declarations ── */
static void imu_task(void *arg);

/* ── Public API ── */

void imu_task_create(void)
{
    /*
     * Queue task başlamadan önce oluşturulur — telemetry task create
     * sırasında queue zaten hazır olsun.
     */
    s_snapshot_q = xQueueCreate(1, sizeof(imu_snapshot_t));

    static const osThreadAttr_t attr = {
        .name       = "IMU",
        .stack_size = 512 * 4,
        .priority   = osPriorityHigh,
    };
    osThreadNew(imu_task, NULL, &attr);
}

bool imu_snapshot_peek(imu_snapshot_t *out)
{
    return xQueuePeek(s_snapshot_q, out, 0) == pdTRUE;
}

/* ── Task implementation ── */

static void imu_task(void *arg)
{
    (void)arg;

    HAL_NVIC_DisableIRQ(EXTI3_IRQn);
    HAL_NVIC_DisableIRQ(EXTI4_IRQn);

    s_handle = xTaskGetCurrentTaskHandle();

    uint8_t init_err = bmi088_init(&k_bmi_cfg);
    (void)init_err;

    bmi088_config(&k_bmi_cfg);

    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);

    mahony_t mahony;
    mahony_init(&mahony);

    dma_state_t dma_state   = DMA_IDLE;
    bool        acc_pending  = false;
    bool        gyro_pending = false;
    bool        acc_fresh    = false;
    bool        gyro_fresh   = false;

    float    ax = 0.0f, ay = 0.0f, az = 0.0f;
    float    gx = 0.0f, gy = 0.0f, gz = 0.0f;
    uint32_t last_tick  = 0U;
    SystemMode_t prev_mode = MODE_NORMAL;

    for (;;) {
        uint32_t bits = 0;
        xTaskNotifyWait(0U, UINT32_MAX, &bits, pdMS_TO_TICKS(500));

        SystemMode_t mode = sys_mode_get();

        /* Mod değişiminde Mahony ve zamanlayıcıyı sıfırla */
        if (mode != prev_mode) {
            mahony_init(&mahony);
            last_tick  = 0U;
            acc_fresh  = false;
            gyro_fresh = false;
            dma_state  = DMA_IDLE;
            prev_mode  = mode;
        }

        /* ── SUT modu: 100 ms batch IMU yolu ────────────────────────
         * Her batch N örnek içerir; her örneğin sim_time'ına göre
         * doğru dt hesaplanır ve Mahony sırayla güncellenir.
         */
        if (mode == MODE_SUT) {
            if (bits & NOTIFY_SUT_BATCH) {
                sut_imu_batch_t batch;
                if (sys_mode_sut_imu_batch_receive(&batch, 0U)) {
                    float prev_sim = -1.0f;
                    for (uint8_t i = 0u; i < batch.count; i++) {
                        float dt = (prev_sim < 0.0f)
                                   ? 0.005f
                                   : (batch.samples[i].sim_time - prev_sim);
                        if (dt <= 0.0f || dt > 0.2f) dt = 0.005f;
                        prev_sim = batch.samples[i].sim_time;

                        mahony_update(&mahony,
                                      batch.samples[i].gyro_x,
                                      batch.samples[i].gyro_y,
                                      batch.samples[i].gyro_z,
                                      0.0f, 0.0f, 0.0f,
                                      dt);
                    }

                    uint32_t now = HAL_GetTick();
                    const sut_imu_sample_t *last =
                        &batch.samples[batch.count > 0u ? batch.count - 1u : 0u];

                    imu_snapshot_t snap;
                    snap.ts_ms   = now;
                    snap.accel.x = 0.0f;
                    snap.accel.y = 0.0f;
                    snap.accel.z = 0.0f;
                    snap.gyro.x  = last->gyro_x;
                    snap.gyro.y  = last->gyro_y;
                    snap.gyro.z  = last->gyro_z;
                    snap.q.w = mahony.q[0]; snap.q.x = mahony.q[1];
                    snap.q.y = mahony.q[2]; snap.q.z = mahony.q[3];
                    mahony_get_euler(&mahony,
                                     &snap.euler.roll,
                                     &snap.euler.pitch,
                                     &snap.euler.yaw);

                    xQueueOverwrite(s_snapshot_q, &snap);
                }
            }
            continue;  /* DRDY bitlerini SUT modunda yoksay */
        }

        /* ── NORMAL / SIT modu: gerçek sensör yolu ───────────────── */
        if (bits & NOTIFY_ACC_DRDY)  acc_pending  = true;
        if (bits & NOTIFY_GYRO_DRDY) gyro_pending = true;

        if (bits & NOTIFY_DMA_DONE) {
            if (dma_state == DMA_READING_ACC) {
                bmi088_parse_accel(s_acc_buf, k_bmi_cfg.acc_range, &ax, &ay, &az);
                acc_fresh = true;
            } else if (dma_state == DMA_READING_GYRO) {
                bmi088_parse_gyro(s_gyro_buf, k_bmi_cfg.gyro_range, &gx, &gy, &gz);
                gyro_fresh = true;
            }
            dma_state = DMA_IDLE;
        }

        if (bits & NOTIFY_I2C_ERROR) {
            dma_state    = DMA_IDLE;
            acc_pending  = true;
            gyro_pending = true;
        }

        if (dma_state == DMA_IDLE) {
            if (acc_pending) {
                if (bmi088_start_accel_dma(k_bmi_cfg.hi2c, s_acc_buf) == HAL_OK) {
                    dma_state   = DMA_READING_ACC;
                    acc_pending = false;
                }
            } else if (gyro_pending) {
                if (bmi088_start_gyro_dma(k_bmi_cfg.hi2c, s_gyro_buf) == HAL_OK) {
                    dma_state    = DMA_READING_GYRO;
                    gyro_pending = false;
                }
            }
        }

        if (acc_fresh && gyro_fresh) {
            uint32_t now = HAL_GetTick();
            float dt = (last_tick == 0U) ? 0.0f
                                         : (float)(now - last_tick) * 0.001f;
            last_tick = now;

            mahony_update(&mahony, gx, gy, gz, ax, ay, az, dt);

            imu_snapshot_t snap;
            snap.ts_ms    = now;
            snap.accel.x  = ax;  snap.accel.y = ay;  snap.accel.z = az;
            snap.gyro.x   = gx;  snap.gyro.y  = gy;  snap.gyro.z  = gz;
            snap.q.w = mahony.q[0]; snap.q.x = mahony.q[1];
            snap.q.y = mahony.q[2]; snap.q.z = mahony.q[3];
            mahony_get_euler(&mahony,
                             &snap.euler.roll,
                             &snap.euler.pitch,
                             &snap.euler.yaw);

            xQueueOverwrite(s_snapshot_q, &snap);

            acc_fresh  = false;
            gyro_fresh = false;
        }
    }
}

/* ── SUT batch teslimi (cmd_task → imu_task) ── */
void imu_task_notify_sut_batch(const sut_imu_batch_t *batch)
{
    sys_mode_sut_imu_batch_put(batch);
    if (s_handle != NULL) {
        xTaskNotify(s_handle, NOTIFY_SUT_BATCH, eSetBits);
    }
}

/* ── HAL callbacks (ISR context) ── */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (s_handle == NULL) return;

    BaseType_t woken = pdFALSE;
    if (GPIO_Pin == GPIO_PIN_3) {
        xTaskNotifyFromISR(s_handle, NOTIFY_ACC_DRDY, eSetBits, &woken);
    } else if (GPIO_Pin == GPIO_PIN_4) {
        xTaskNotifyFromISR(s_handle, NOTIFY_GYRO_DRDY, eSetBits, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1 || s_handle == NULL) return;

    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_handle, NOTIFY_DMA_DONE, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1 || s_handle == NULL) return;

    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_handle, NOTIFY_I2C_ERROR, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}
