#include "flight_sm.h"
#include "main.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <math.h>

/* -------------------------------------------------------------------------
 * Eşik değerleri
 * ------------------------------------------------------------------------- */
#define LAUNCH_ACCEL_THR    45.0f   /* m/s²  — toplam IMU ivmesi (yerçekimi dahil) */
#define BURNOUT_AVERT_THR    0.0f   /* m/s²  — Kalman dikey ivmesi < 0 → motor söndü */
#define BURNOUT_CONFIRM         5   /* örnek — 10 Hz'de 500 ms */
#define BURNOUT_TIMEOUT_MS   8000   /* ms    — max boost süresi */
#define MIN_ARM_ALT_M      100.0f   /* m AGL — arming irtifası */
#define MAX_TILT_DEG        70.0f   /* derece — acil drogue açı eşiği */
#define APOGEE_CONFIRM          5   /* örnek — 10 Hz'de 500 ms */
#define MAIN_ALT_M         300.0f   /* m AGL — ana paraşüt irtifası */
#define LAND_VEL_THR         2.0f   /* m/s   — iniş hız eşiği */
#define LAND_CONFIRM           30   /* örnek — 10 Hz'de 3 s */

/* -------------------------------------------------------------------------
 * Modül state'i
 * ------------------------------------------------------------------------- */
static QueueHandle_t  s_q;

static FlightPhase_t  s_phase  = FLIGHT_IDLE;
static uint16_t       s_status = 0u;

static uint16_t s_burnout_cnt = 0;
static uint16_t s_apogee_cnt  = 0;
static uint16_t s_land_cnt    = 0;

static uint8_t  s_armed           = 0;
static uint8_t  s_drogue_deployed = 0;
static uint8_t  s_main_deployed   = 0;
static uint32_t s_launch_tick     = 0;

/* -------------------------------------------------------------------------
 * Yardımcı hesaplamalar
 * ------------------------------------------------------------------------- */

/* Toplam IMU ivme büyüklüğü (m/s²) — yerçekimi dahil, durağanda ≈ 9.81 */
static float total_accel(const imu_snapshot_t *imu)
{
    float ax = imu->accel.x, ay = imu->accel.y, az = imu->accel.z;
    return sqrtf(ax*ax + ay*ay + az*az);
}

/* Dikey ekseninden toplam eğim açısı (derece) — sqrt(roll²+pitch²) */
static float tilt_deg(const imu_snapshot_t *imu)
{
    float r = imu->euler.roll, p = imu->euler.pitch;
    return sqrtf(r*r + p*p);
}

static void publish(uint32_t ts)
{
    flight_snapshot_t snap = { .ts_ms = ts, .phase = s_phase, .status = s_status };
    xQueueOverwrite(s_q, &snap);
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void flight_sm_init(void)
{
    s_q = xQueueCreate(1, sizeof(flight_snapshot_t));
}

void flight_sm_reset(void)
{
    s_phase          = FLIGHT_IDLE;
    s_status         = 0u;
    s_burnout_cnt    = 0;
    s_apogee_cnt     = 0;
    s_land_cnt       = 0;
    s_armed          = 0;
    s_drogue_deployed = 0;
    s_main_deployed  = 0;
    s_launch_tick    = 0;
}

void flight_sm_update(const alt_snapshot_t *alt, const imu_snapshot_t *imu)
{
    switch (s_phase) {

    /* -----------------------------------------------------------------
     * IDLE: Rampa üzerinde bekliyor.
     * Çıkış: Yüksek IMU ivmesi → BOOST
     * ----------------------------------------------------------------- */
    case FLIGHT_IDLE:
        if (imu && total_accel(imu) > LAUNCH_ACCEL_THR) {
            s_launch_tick = HAL_GetTick();
            s_status |= FSM_BIT_LAUNCHED;
            s_phase = FLIGHT_BOOST;
        }
        break;

    /* -----------------------------------------------------------------
     * BOOST: Motor yanıyor.
     * Çıkış: Kalman dikey ivmesi < 0 (N örnekte) VEYA timeout → COAST
     * ----------------------------------------------------------------- */
    case FLIGHT_BOOST:
        if (alt->accel_vert < BURNOUT_AVERT_THR) {
            s_burnout_cnt++;
        } else if (s_burnout_cnt > 0) {
            s_burnout_cnt--;
        }

        if (s_burnout_cnt >= BURNOUT_CONFIRM ||
            (HAL_GetTick() - s_launch_tick) > BURNOUT_TIMEOUT_MS) {
            s_burnout_cnt = 0;
            s_status |= FSM_BIT_BURNOUT;
            s_phase = FLIGHT_COAST;
        }
        break;

    /* -----------------------------------------------------------------
     * COAST: Motorsuz tırmanış.
     * Çıkış: velocity < 0 (N örnekte) → APOGEE
     *        Veya açı eşiği aşılırsa acil → APOGEE
     * ----------------------------------------------------------------- */
    case FLIGHT_COAST:
        if (!s_armed && alt->altitude_rel > MIN_ARM_ALT_M) {
            s_armed = 1;
            s_status |= FSM_BIT_ARMED;
        }

        if (s_armed) {
            /* Acil durum: aşırı eğim */
            if (imu && tilt_deg(imu) > MAX_TILT_DEG) {
                s_status |= FSM_BIT_TILT_EMERG | FSM_BIT_APOGEE;
                s_phase = FLIGHT_APOGEE;
                break;
            }

            /* Normal apogee: hız negatife döndü ve N örnekte sabit kaldı */
            if (alt->velocity < 0.0f) {
                s_apogee_cnt++;
            } else if (s_apogee_cnt > 0) {
                s_apogee_cnt--;
            }

            if (s_apogee_cnt >= APOGEE_CONFIRM) {
                s_apogee_cnt = 0;
                s_status |= FSM_BIT_APOGEE;
                s_phase = FLIGHT_APOGEE;
            }
        }
        break;

    /* -----------------------------------------------------------------
     * APOGEE: Zirve noktası — drogue tetiklenir, hemen iniş fazına geçilir.
     * ----------------------------------------------------------------- */
    case FLIGHT_APOGEE:
        if (!s_drogue_deployed) {
            s_drogue_deployed = 1;
            s_status |= FSM_BIT_DROGUE;
            /* TODO: drogue GPIO tetikle */
        }
        s_phase = FLIGHT_DROGUE_DESCENT;
        break;

    /* -----------------------------------------------------------------
     * DROGUE_DESCENT: Drogue paraşütle iniş.
     * Çıkış: altitude_rel < MAIN_ALT_M → MAIN_DESCENT
     * ----------------------------------------------------------------- */
    case FLIGHT_DROGUE_DESCENT:
        if (alt->altitude_rel < MAIN_ALT_M) {
            s_status |= FSM_BIT_MAIN_ALT;
            s_phase = FLIGHT_MAIN_DESCENT;
        }
        break;

    /* -----------------------------------------------------------------
     * MAIN_DESCENT: Ana paraşütle iniş.
     * Çıkış: |velocity| < eşik (N örnekte) → LANDED
     * ----------------------------------------------------------------- */
    case FLIGHT_MAIN_DESCENT:
        if (!s_main_deployed) {
            s_main_deployed = 1;
            s_status |= FSM_BIT_MAIN;
            /* TODO: main GPIO tetikle */
        }

        if (fabsf(alt->velocity) < LAND_VEL_THR) {
            s_land_cnt++;
        } else {
            s_land_cnt = 0;
        }

        if (s_land_cnt >= LAND_CONFIRM) {
            s_status |= FSM_BIT_LANDED;
            s_phase = FLIGHT_LANDED;
        }
        break;

    /* -----------------------------------------------------------------
     * LANDED: Terminal durum. Reset olmadan çıkış yok.
     * ----------------------------------------------------------------- */
    case FLIGHT_LANDED:
        break;
    }

    publish(alt->ts_ms);
}

bool flight_snapshot_peek(flight_snapshot_t *out)
{
    return xQueuePeek(s_q, out, 0) == pdTRUE;
}

FlightPhase_t flight_sm_get_phase(void)
{
    return s_phase;
}
