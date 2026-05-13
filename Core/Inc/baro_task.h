#ifndef INC_BARO_TASK_H_
#define INC_BARO_TASK_H_

#include "i2c.h"

/* app.c bu fonksiyonu çağırır */
void baro_task_create(void);

/*
 * HAL_I2C_MemRxCpltCallback içinden I2C3 için çağrılır (ISR bağlamı).
 * imu_task.c bu fonksiyonu bilir; baro_task kendi task handle'ını yönetir.
 */
void baro_i2c_rx_done_from_isr(void);

/* HAL_I2C_ErrorCallback içinden I2C3 için çağrılır (ISR bağlamı) */
void baro_i2c_error_from_isr(void);

#endif /* INC_BARO_TASK_H_ */
