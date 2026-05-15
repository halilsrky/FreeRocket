#ifndef MAHONY_H
#define MAHONY_H

typedef struct {
    float q[4];         /* quaternion [w, x, y, z] — normalize edilmiş */
    float twoKp;        /* 2 * proportional gain */
    float twoKi;        /* 2 * integral gain */
    float integral[3];  /* integral error [x, y, z] */
} mahony_t;

/* Varsayılan kazançlar */
#define MAHONY_KP_DEFAULT  2.0f
#define MAHONY_KI_DEFAULT  0.005f

void mahony_init(mahony_t *m);

/*
 * gx/gy/gz : rad/s (gyro)
 * ax/ay/az : m/s²  (accel — herhangi bir birim, normalize edilir)
 * dt       : saniye
 */
void mahony_update(mahony_t *m,
                   float gx, float gy, float gz,
                   float ax, float ay, float az,
                   float dt);

/* Quaternion'dan Euler açıları (derece) */
void mahony_get_euler(const mahony_t *m,
                      float *roll, float *pitch, float *yaw);

/* Roketin dikey eksenden sapma açısı — theta (derece).
 * θ=0 dik yukarı, θ=90 yatay, θ=180 dik aşağı. */
float mahony_get_theta(const mahony_t *m);

#endif /* MAHONY_H */
