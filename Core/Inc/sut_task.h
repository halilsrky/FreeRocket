#ifndef SUT_TASK_H
#define SUT_TASK_H

#include <stdint.h>

/*
 * SUT_COMBINED paketi (PC → STM32, 0xAD):
 *   count×[sim_t(4)+gx(4)+gy(4)+gz(4)]  →  Mahony için gyro batch
 *   ax(4)+ay(4)+az(4)                    →  son örnek ivme, flight_sm için
 *   alt(4)+press(4)+baro_t(4)            →  Kalman için baro
 *
 * SUT_RESPONSE paketi (STM32 → PC, 0xAE, 26 byte):
 *   sim_time(4) | alt(4) | roll(4) | pitch(4) | yaw(4) | status(2) | chk | CR LF
 */

#define SUT_BATCH_MAX 25u

typedef struct {
    float sim_time;
    float gx, gy, gz;
} sut_imu_t;

typedef struct {
    uint8_t    count;
    sut_imu_t  imu[SUT_BATCH_MAX];
    float      last_ax, last_ay, last_az;
    float      altitude;
    float      pressure;
    float      baro_sim_time;
} sut_packet_t;

/* app.c içinden, scheduler başlamadan önce çağrılır */
void sut_task_create(void);

/* cmd_task'tan (task context) çağrılır */
void sut_task_notify_packet(const sut_packet_t *pkt);

#endif /* SUT_TASK_H */
