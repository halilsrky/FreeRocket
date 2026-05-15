#ifndef PHASE_ML_H
#define PHASE_ML_H

#include <stdint.h>

/*
 * Faz sınıfları (model çıkışı):
 *   0 PAD   1 BOOST   2 COAST   3 APOGEE   4 DESCENT
 *
 * Çalışma şekli:
 *   - Her IMU örneği için phase_ml_push() çağır.
 *   - Her baro okuması için phase_ml_push_baro() çağır.
 *   - 200 örnek dolunca ve her 100 örnekte bir otomatik çıkarım yapar.
 *   - phase_ml_get_phase() son çıkarım sonucunu döner (buffer dolmadıysa 0).
 */

void    phase_ml_init(void);

void    phase_ml_push(float az, float ax, float ay,
                      float gx, float gy, float gz, float ts);

void    phase_ml_push_baro(float alt, float t);

uint8_t phase_ml_get_phase(void);

#endif /* PHASE_ML_H */
