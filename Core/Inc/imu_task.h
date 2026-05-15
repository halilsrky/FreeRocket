#ifndef IMU_TASK_H
#define IMU_TASK_H

#include "sys_mode.h"

void imu_task_create(void);

/* SUT modunda cmd_task'tan 100 ms'lik IMU batch teslim eder; task context'ten çağrılır */
void imu_task_notify_sut_batch(const sut_imu_batch_t *batch);

#endif /* IMU_TASK_H */
