#ifndef INC_FLIGHT_SM_H_
#define INC_FLIGHT_SM_H_

#include "alt_snapshot.h"
#include "imu_snapshot.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    FLIGHT_IDLE = 0,
    FLIGHT_BOOST,
    FLIGHT_COAST,
    FLIGHT_APOGEE,
    FLIGHT_DROGUE_DESCENT,
    FLIGHT_MAIN_DESCENT,
    FLIGHT_LANDED,
} FlightPhase_t;

/* Cumulative event flags — bir kez set edilince kalır */
#define FSM_BIT_LAUNCHED    0x0001u  /* Fırlatma tespit edildi */
#define FSM_BIT_BURNOUT     0x0002u  /* Motor yanması bitti */
#define FSM_BIT_ARMED       0x0004u  /* Min. irtifaya ulaşıldı, arming aktif */
#define FSM_BIT_TILT_EMERG  0x0008u  /* Açı eşiği aşıldı — acil drogue */
#define FSM_BIT_APOGEE      0x0010u  /* Apogee tespit edildi */
#define FSM_BIT_DROGUE      0x0020u  /* Drogue paraşüt tetiklendi */
#define FSM_BIT_MAIN_ALT    0x0040u  /* Ana paraşüt irtifasına ulaşıldı */
#define FSM_BIT_MAIN        0x0080u  /* Ana paraşüt tetiklendi */
#define FSM_BIT_LANDED      0x0100u  /* İniş tespit edildi */
#define FSM_BIT_VEL_APOGEE  0x0200u  /* Hız bazlı apogee tespiti onaylandı */

typedef struct {
    uint32_t      ts_ms;
    FlightPhase_t phase;
    uint16_t      status;
} flight_snapshot_t;

/* baro_task_create() içinden bir kez çağrılır */
void flight_sm_init(void);

/* Tüm state'i sıfırlar — STOP komutu veya mod geçişinde çağrılır */
void flight_sm_reset(void);

/*
 * Her 10 Hz baro döngüsünde, Kalman güncellemesinden hemen sonra çağrılır.
 * imu: IMU snapshot yoksa NULL geçilebilir; NULL ise launch ve tilt kontrolleri atlanır.
 */
void flight_sm_update(const alt_snapshot_t *alt, const imu_snapshot_t *imu);

/* Non-blocking peek — ilk güncelleme gelmemişse false döner */
bool flight_snapshot_peek(flight_snapshot_t *out);

FlightPhase_t flight_sm_get_phase(void);

#endif /* INC_FLIGHT_SM_H_ */
