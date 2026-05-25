#include "gnss_task.h"
#include "gnss_snapshot.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "cmsis_os.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>

/* DMA handle tanımı usart.c'de; extern ile erişiyoruz */
extern DMA_HandleTypeDef hdma_usart6_rx;

/* ── Dairesel DMA çift buffer ── */
#define GNSS_BUF_HALF   512U
#define GNSS_BUF_TOTAL  (GNSS_BUF_HALF * 2U)

static TaskHandle_t  s_handle;
static QueueHandle_t s_gnss_q;
static uint8_t       s_dma_buf[GNSS_BUF_TOTAL];

/* Parse scratch — stack'e yüklememek için statik */
static char s_parse_buf[GNSS_BUF_HALF + 1U];

static void parse_half(const uint8_t *src);
static bool parse_gnrmc(const char *buf, gnss_snapshot_t *out);
static bool parse_gpgga(const char *buf, gnss_snapshot_t *out);
static void gnss_task(void *arg);

/* ── Public API ─────────────────────────────────────────────────── */

void gnss_task_create(void)
{
    s_gnss_q = xQueueCreate(1, sizeof(gnss_snapshot_t));

    static const osThreadAttr_t attr = {
        .name       = "GNSS",
        .stack_size = 512 * 4,
        .priority   = osPriorityBelowNormal,
    };
    osThreadNew(gnss_task, NULL, &attr);
}

bool gnss_snapshot_peek(gnss_snapshot_t *out)
{
    return xQueuePeek(s_gnss_q, out, 0) == pdTRUE;
}

/* ── Task ───────────────────────────────────────────────────────── */

static void gnss_task(void *arg)
{
    (void)arg;
    s_handle = xTaskGetCurrentTaskHandle();

    /*
     * L86, fabrika çıkışında 9600 baud çalışır.
     * PMTK251 komutuyla 57600'e geçiriyoruz; ardından UART yeniden başlatılıyor.
     */
    HAL_UART_Transmit(&huart6, (uint8_t *)"$PMTK251,57600*2C\r\n", 19U, 200U);
    vTaskDelay(pdMS_TO_TICKS(100U));

    HAL_UART_DeInit(&huart6);
    huart6.Init.BaudRate = 57600U;
    HAL_UART_Init(&huart6);
    HAL_DMA_Init(&hdma_usart6_rx);

    HAL_UART_Receive_DMA(&huart6, s_dma_buf, GNSS_BUF_TOTAL);

    for (;;) {
        uint32_t notif;
        BaseType_t got = xTaskNotifyWait(0U, 0x03U, &notif,
                                         pdMS_TO_TICKS(100U));

        if (got == pdTRUE) {
            if (notif & 0x01U) parse_half(s_dma_buf);
            if (notif & 0x02U) parse_half(s_dma_buf + GNSS_BUF_HALF);
        }
        /* GNSS modülü olmasa bile 100 ms'de bir buraya geliyoruz —
         * SystemView'da task görünür, snapshot is_valid=false kalır. */
    }
}

/* ── ISR callbacks (dairesel DMA ping-pong) ─────────────────────── */

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART6 || s_handle == NULL) return;
    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_handle, 0x01U, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART6 || s_handle == NULL) return;
    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_handle, 0x02U, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

/* ── NMEA parse ─────────────────────────────────────────────────── */

/*
 * GNRMC cümlesini ayrıştırır: konum, hız, rota, zaman, tarih.
 * Başarıysa out doldurulur ve true döner; geçersiz veri / parse hatası → false.
 */
static bool parse_gnrmc(const char *buf, gnss_snapshot_t *out)
{
    const char *p = strstr(buf, "GNRMC");
    if (p == NULL) return false;

    /* '*' bulunana kadar geçici tampona kopyala */
    char tmp[120];
    uint16_t i = 0;
    while (*p && *p != '*' && i < (uint16_t)(sizeof(tmp) - 1U)) {
        tmp[i++] = *p++;
    }
    tmp[i] = '\0';

    float    raw_time = 0.0f, raw_lat = 0.0f, raw_lon = 0.0f;
    unsigned raw_date = 0U;
    char     is_valid = 'V', ns = 'N', ew = 'E', pos_mode = 'N';

    int n = sscanf(tmp,
        "GNRMC,%f,%c,%f,%c,%f,%c,%f,%f,%u,,,%c",
        &raw_time, &is_valid, &raw_lat, &ns,
        &raw_lon, &ew, &out->speed_knots, &out->course_deg,
        &raw_date, &pos_mode);

    if (n < 9 || is_valid != 'A') return false;

    out->is_valid = true;

    /* DDMM.MMMMM → ondalık derece */
    float deg = floorf(raw_lat / 100.0f);
    out->latitude = deg + (raw_lat - deg * 100.0f) / 60.0f;
    if (ns == 'S') out->latitude = -out->latitude;

    deg = floorf(raw_lon / 100.0f);
    out->longitude = deg + (raw_lon - deg * 100.0f) / 60.0f;
    if (ew == 'W') out->longitude = -out->longitude;

    /* HHMMSS.sss → saat/dakika/saniye (UTC+3) */
    uint8_t h = (uint8_t)(raw_time / 10000.0f);
    raw_time -= h * 10000.0f;
    uint8_t m = (uint8_t)(raw_time / 100.0f);
    uint8_t s = (uint8_t)(raw_time - (float)(m * 100U));
    h = (uint8_t)(h + 3U);
    if (h >= 24U) h = (uint8_t)(h - 24U);
    out->time[0] = h;
    out->time[1] = m;
    out->time[2] = s;

    /* DDMMYY → gün/ay/yıl */
    out->date[0] = (uint8_t)(raw_date / 10000U);
    raw_date     -= out->date[0] * 10000U;
    out->date[1]  = (uint8_t)(raw_date / 100U);
    out->date[2]  = (uint8_t)(raw_date % 100U);

    return true;
}

/*
 * GPGGA cümlesini ayrıştırır: yükseklik, uydu sayısı, HDOP.
 * Snapshot'u zenginleştirir; fix yoksa false döner.
 */
static bool parse_gpgga(const char *buf, gnss_snapshot_t *out)
{
    const char *p = strstr(buf, "GPGGA");
    if (p == NULL) return false;

    char tmp[120];
    uint16_t i = 0;
    while (*p && *p != '*' && i < (uint16_t)(sizeof(tmp) - 1U)) {
        tmp[i++] = *p++;
    }
    tmp[i] = '\0';

    float    raw_time = 0.0f, raw_lat = 0.0f, raw_lon = 0.0f, geoid = 0.0f;
    char     ns = 'N', ew = 'E';
    unsigned fix = 0U, sats = 0U;

    int n = sscanf(tmp,
        "GPGGA,%f,%f,%c,%f,%c,%u,%u,%f,%f,M,%f,M,,",
        &raw_time, &raw_lat, &ns, &raw_lon, &ew,
        &fix, &sats, &out->hdop, &out->altitude, &geoid);

    if (n < 9 || fix == 0U) return false;

    out->satellites = (uint8_t)sats;
    return true;
}

static void parse_half(const uint8_t *src)
{
    memcpy(s_parse_buf, src, GNSS_BUF_HALF);
    s_parse_buf[GNSS_BUF_HALF] = '\0';

    gnss_snapshot_t snap = {0};
    snap.ts_ms = HAL_GetTick();

    if (parse_gnrmc(s_parse_buf, &snap)) {
        parse_gpgga(s_parse_buf, &snap);  /* yükseklik/uydu/HDOP varsa ekle */
        xQueueOverwrite(s_gnss_q, &snap);
    }
}
