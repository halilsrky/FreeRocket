#ifndef IMU_SNAPSHOT_H
#define IMU_SNAPSHOT_H

#include <stdint.h>
#include <stdbool.h>

/*
 * IMU task tarafından atomik olarak yayınlanan anlık görüntü.
 * Tüm alanlar tek bir queue kopyalamasında tutarlı — yarı-güncel karışma yok.
 */
typedef struct {
    uint32_t ts_ms;

    struct { float x, y, z; } accel;   /* m/s²  */
    struct { float x, y, z; } gyro;    /* rad/s */

    struct { float w, x, y, z; } q;    /* normalize quaternion */

    struct { float roll, pitch, yaw; } euler;  /* derece */
} imu_snapshot_t;

/*
 * Telemetry task bu fonksiyonu çağırır — IMU task'ına hiç dokunmaz.
 * Kuyruk boşsa (henüz ilk sample gelmemişse) false döner.
 * Blocking yok, en güncel snapshot'ı kopyalar.
 */
bool imu_snapshot_peek(imu_snapshot_t *out);

#endif /* IMU_SNAPSHOT_H */
