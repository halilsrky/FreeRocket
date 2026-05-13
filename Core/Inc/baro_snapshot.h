#ifndef INC_BARO_SNAPSHOT_H_
#define INC_BARO_SNAPSHOT_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Baro task tarafından atomik olarak yayınlanan anlık görüntü.
 * imu_snapshot_t ile aynı kalıp: depth=1 queue + xQueueOverwrite.
 */
typedef struct {
    uint32_t ts_ms;
    float    temperature;  /* °C  */
    float    pressure;     /* hPa */
    float    humidity;     /* %RH */
    float    altitude;     /* m (MSL, standart atmosfer) */
} baro_snapshot_t;

/*
 * Non-blocking peek. Kuyruk henüz dolmamışsa (ilk ölçüm bekleniyor) false döner.
 * Task'a dokunmaz; kopya alır.
 */
bool baro_snapshot_peek(baro_snapshot_t *out);

#endif /* INC_BARO_SNAPSHOT_H_ */
