#ifndef INC_GNSS_SNAPSHOT_H_
#define INC_GNSS_SNAPSHOT_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t ts_ms;
    bool     is_valid;
    float    latitude;      /* decimal degrees, kuzey pozitif */
    float    longitude;     /* decimal degrees, doğu pozitif */
    float    altitude;      /* WGS84 elipsoidal yükseklik (m) */
    float    speed_knots;
    float    course_deg;
    uint8_t  satellites;
    float    hdop;
    uint8_t  time[3];       /* UTC+3: saat, dakika, saniye */
    uint8_t  date[3];       /* gün, ay, yıl */
} gnss_snapshot_t;

bool gnss_snapshot_peek(gnss_snapshot_t *out);

#endif /* INC_GNSS_SNAPSHOT_H_ */
