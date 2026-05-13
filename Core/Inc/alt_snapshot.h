#ifndef INC_ALT_SNAPSHOT_H_
#define INC_ALT_SNAPSHOT_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Kalman-filtered altitude snapshot published by baro_task on every baro update.
 * Veri sahipliği baro_task'ta; diğer task'lar sadece peek ile okur.
 */
typedef struct {
    uint32_t ts_ms;
    float    altitude_rel;  /* m — boot anına göre göreli irtifa */
    float    velocity;      /* m/s — dikey hız (yukarı pozitif) */
    float    accel_vert;    /* m/s² — filtrelenmiş dikey ivme (durağanda ≈0) */
} alt_snapshot_t;

/*
 * Non-blocking peek. İlk ölçüm henüz gelmemişse false döner.
 */
bool alt_snapshot_peek(alt_snapshot_t *out);

#endif /* INC_ALT_SNAPSHOT_H_ */
