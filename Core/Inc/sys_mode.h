#ifndef SYS_MODE_H
#define SYS_MODE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MODE_NORMAL = 0,
    MODE_SIT,
    MODE_SUT,
} SystemMode_t;

/* SUT IMU tek örnek — sim zamanı + gyro (Mahony gyro-only SUT modu) */
typedef struct {
    float sim_time;
    float gyro_x;
    float gyro_y;
    float gyro_z;
} sut_imu_sample_t;

/* SUT IMU batch — 100 ms'lik penceredeki tüm örnekler */
#define SUT_IMU_BATCH_MAX 25u

typedef struct {
    uint8_t          count;
    sut_imu_sample_t samples[SUT_IMU_BATCH_MAX];
} sut_imu_batch_t;

/* SUT baro verisi — irtifa + basınç + sim zamanı, Kalman filtresi için */
typedef struct {
    float altitude;
    float pressure;
    float sim_time;
} sut_baro_t;

/* FreeRTOS scheduler başladıktan sonra bir kez çağrılır */
void         sys_mode_init(void);

SystemMode_t sys_mode_get(void);
void         sys_mode_set(SystemMode_t mode);

/* SUT IMU batch queue: cmd_task yazar, imu_task okur */
void sys_mode_sut_imu_batch_put(const sut_imu_batch_t *batch);
bool sys_mode_sut_imu_batch_receive(sut_imu_batch_t *out, uint32_t timeout_ms);

/* SUT baro queue: cmd_task yazar, baro_task okur */
void sys_mode_sut_baro_put(const sut_baro_t *data);
bool sys_mode_sut_baro_receive(sut_baro_t *out, uint32_t timeout_ms);

#endif /* SYS_MODE_H */
