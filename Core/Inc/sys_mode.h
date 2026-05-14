#ifndef SYS_MODE_H
#define SYS_MODE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MODE_NORMAL = 0,
    MODE_SIT,
    MODE_SUT,
} SystemMode_t;

typedef struct {
    float altitude;  /* m   — barometrik irtifa (dünya frame) */
    float pressure;  /* hPa */
    float accel_x;   /* m/s² */
    float accel_y;
    float accel_z;   /* pozitif yukarı, dünya frame */
    float gyro_x;    /* rad/s */
    float gyro_y;
    float gyro_z;
} sut_data_t;

/* FreeRTOS scheduler başladıktan sonra bir kez çağrılır */
void          sys_mode_init(void);

SystemMode_t  sys_mode_get(void);
void          sys_mode_set(SystemMode_t mode);

/* SUT sentetik veri kuyruğu — cmd_task yazar, baro_task okur */
bool          sys_mode_sut_peek(sut_data_t *out);
void          sys_mode_sut_put(const sut_data_t *data);

#endif /* SYS_MODE_H */
