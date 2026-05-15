# mlmodel.md — Roket Faz Dedektörü (STM32F446)

Bu dosya eğitilmiş ML modelinin STM32 projesinde nasıl kullanılacağını açıklar.

---

## Model Özeti

| | |
|---|---|
| Görev | 5-sınıf anlık faz tespiti |
| Mimari | Dense NN: 14 → 64 → 32 → 5 |
| Parametre sayısı | 3 205 |
| TFLite int8 boyutu | **8.3 KB Flash** |
| Test doğruluğu | **%99.47** |
| Çıktı | Softmax olasılık vektörü (5 sınıf) |

---

## Sınıf Etiketleri

| Index | İsim | Açıklama |
|-------|------|----------|
| 0 | PAD | Raylarda bekleme — motor ateşlenmedi |
| 1 | BOOST | Motor yanıyor — yüksek ivme |
| 2 | COAST | Motor söndü — yukarı gidiyor |
| 3 | APOGEE | Zirve civarı — apoge ± 1 s |
| 4 | DESCENT | İniş — yükseklik azalıyor |

---

## Girdi: 14 Özellik

Her **1 saniyelik pencere** (200 IMU örneği, 10 baro örneği) için hesaplanır.
STM32'de her **500 ms**'de bir pencere değerlendirilir (%50 örtüşme).

### Özellik Listesi ve Hesaplama

```c
// IMU: 200 örnek, accel [m/s²], gyro [rad/s]
// Baro: 10 örnek (her 20. IMU örneğinde bir güncelleme)

float accel_z_mean  = mean(accel_z[0..199]);
float accel_z_std   = std(accel_z[0..199]);
float accel_z_max   = max(accel_z[0..199]);

float accel_x_mean  = mean(accel_x[0..199]);
float accel_x_std   = std(accel_x[0..199]);

float accel_y_mean  = mean(accel_y[0..199]);
float accel_y_std   = std(accel_y[0..199]);

// lateral RMS: yanal ivme büyüklüğü ortalaması
float accel_lateral_rms = mean(sqrt(accel_x[i]^2 + accel_y[i]^2))  for i in 0..199

// gyro büyüklüğü
float gyro_mag[i]   = sqrt(gx[i]^2 + gy[i]^2 + gz[i]^2)
float gyro_mag_mean = mean(gyro_mag[0..199]);
float gyro_mag_std  = std(gyro_mag[0..199]);

// Jerk: gerçek dt ile (interrupt jitter'a karşı dayanıklı)
float jerk_z_mean = mean(|accel_z[i] - accel_z[i-1]| / dt[i])  for i in 1..199
// dt[i] = imu_timestamp[i] - imu_timestamp[i-1]  (saniye cinsinden)

// Baro: penceredeki 10 geçerli okuma (her 20. IMU sample)
float baro_mean  = mean(baro[0..9]);
float baro_std   = std(baro[0..9]);
float baro_slope = linreg_slope(baro_t[0..9], baro[0..9]);  // m/s birimi
// linreg_slope ≈ (baro[9]-baro[0]) / (baro_t[9]-baro_t[0])  basitleştirilmiş
```

### Koordinat Sistemi

```
accel_z → Roketin boyuna ekseni (kuyruktan buruna)
accel_x, accel_y → Yanal eksenler
Yerçekimi dahil: durağanda accel_z ≈ +9.81 m/s²
Motor ateşlemesinde accel_z >> 9.81 (pik ~120 m/s²)
```

---

## Normalizasyon (Z-score)

**Model çalıştırmadan önce her özelliği normalize et:**

```c
normalized[i] = (feature[i] - MEAN[i]) / SCALE[i]
```

### Normalizasyon Sabitleri

```c
// scaler_params.json'dan — doğrudan kopyala
static const float FEATURE_MEAN[14] = {
     6.1332f,   // accel_z_mean
     0.8215f,   // accel_z_std
     7.5851f,   // accel_z_max
     0.0011f,   // accel_x_mean
     0.0918f,   // accel_x_std
    -0.2282f,   // accel_y_mean
     0.0949f,   // accel_y_std
     0.3392f,   // accel_lateral_rms
     0.0568f,   // gyro_mag_mean
     0.0120f,   // gyro_mag_std
    13.3005f,   // jerk_z_mean
  1365.6988f,   // baro_mean
     1.6260f,   // baro_slope
    23.9758f,   // baro_std
};

static const float FEATURE_SCALE[14] = {
    16.2021f,   // accel_z_mean
     3.4651f,   // accel_z_std
    19.3601f,   // accel_z_max
     0.0803f,   // accel_x_mean
     0.1710f,   // accel_x_std
     0.4100f,   // accel_y_mean
     0.1874f,   // accel_y_std
     0.4175f,   // accel_lateral_rms
     0.1026f,   // gyro_mag_mean
     0.0391f,   // gyro_mag_std
    11.0951f,   // jerk_z_mean
  1114.5356f,   // baro_mean
   110.8266f,   // baro_slope
    20.9574f,   // baro_std
};
```

---

## STM32 Çalışma Döngüsü

### Bellek Yapısı

```c
#define IMU_WINDOW   200    // 1 sn @ 200 Hz
#define IMU_STEP     100    // %50 örtüşme
#define BARO_STEP    20     // her 20. IMU örneği = 10 Hz baro
#define N_FEATURES   14
#define N_CLASSES    5

typedef struct {
    float az[IMU_WINDOW], ax[IMU_WINDOW], ay[IMU_WINDOW];
    float gx[IMU_WINDOW], gy[IMU_WINDOW], gz[IMU_WINDOW];
    float ts[IMU_WINDOW];   // IMU timestamp (ms)
    float baro[IMU_WINDOW / BARO_STEP];  // 10 baro okuması
    float baro_t[IMU_WINDOW / BARO_STEP];
    uint16_t head;          // dairesel buffer başı
    uint16_t count;         // dolu örnek sayısı
} ImuBuffer;

uint8_t phase = 0;          // mevcut faz
```

### IMU Interrupt Handler

```c
void IMU_DataReady_IRQHandler(void) {
    float ts = HAL_GetTick() * 0.001f;  // saniye

    buf.az[buf.head] = read_accel_z();
    buf.ax[buf.head] = read_accel_x();
    buf.ay[buf.head] = read_accel_y();
    buf.gx[buf.head] = read_gyro_x();
    buf.gy[buf.head] = read_gyro_y();
    buf.gz[buf.head] = read_gyro_z();
    buf.ts[buf.head] = ts;

    // Baro: her BARO_STEP IMU örneğinde bir güncelle
    if (buf.head % BARO_STEP == 0) {
        uint8_t bi = buf.head / BARO_STEP;
        buf.baro[bi]   = read_baro_altitude();
        buf.baro_t[bi] = ts;
    }

    buf.head = (buf.head + 1) % IMU_WINDOW;
    if (buf.count < IMU_WINDOW) buf.count++;

    // Her IMU_STEP örnekte bir çıkarım tetikle
    if (buf.count >= IMU_WINDOW && buf.head % IMU_STEP == 0) {
        run_inference_flag = 1;
    }
}
```

### Özellik Çıkarma + Çıkarım

ST Edge AI analizi sonucu model **tam int8**:
```
input : int8[14]  QLinear(scale=0.192894, zero_point=-8)
output: int8[5]   QLinear(scale=0.003906, zero_point=-128)
Flash : 11.8 KB   RAM: 2.1 KB   MACC: 3280 (~0.1ms @ 180MHz)
```

Float feature → int8 dönüşümü zorunlu. Çıkış için dequantize gerekmez, argmax int8 üzerinde çalışır.

```c
// Quantization sabitleri (stedgeai analyze çıktısından)
#define INPUT_SCALE      0.192893714f
#define INPUT_ZERO_POINT (-8)

static inline int8_t quantize_input(float x) {
    int32_t q = (int32_t)roundf(x / INPUT_SCALE) + INPUT_ZERO_POINT;
    if (q >  127) q =  127;
    if (q < -128) q = -128;
    return (int8_t)q;
}

void run_inference(void) {
    if (!run_inference_flag) return;
    run_inference_flag = 0;

    // 1. Ham özellik hesapla
    float features[N_FEATURES];
    extract_features(&buf, features);

    // 2. Z-score normalize
    normalize(features);

    // 3. Float → int8 (model int8 input bekliyor)
    int8_t input_q[N_FEATURES];
    for (uint8_t i = 0; i < N_FEATURES; i++)
        input_q[i] = quantize_input(features[i]);

    // 4. Çıkarım (ST Edge AI v4.0.0 stai_* API)
    int8_t *in_ptr  = (int8_t *)network_input_buffers[0];
    int8_t *out_ptr = (int8_t *)network_output_buffers[0];
    memcpy(in_ptr, input_q, N_FEATURES);
    stai_network_run(&network_ctx, STAI_MODE_SYNC);

    // 5. Argmax — int8 üzerinde doğrudan çalışır (scale aynı)
    phase = 0;
    for (uint8_t i = 1; i < N_CLASSES; i++)
        if (out_ptr[i] > out_ptr[phase]) phase = i;
}

static void extract_features(ImuBuffer *b, float *f) {
    float sum_az=0, sum_az2=0, max_az=-1e9;
    float sum_ax=0, sum_ax2=0;
    float sum_ay=0, sum_ay2=0;
    float sum_lat=0, sum_gm=0, sum_gm2=0, sum_jerk=0;
    uint16_t n = IMU_WINDOW;

    for (uint16_t i = 0; i < n; i++) {
        float az=b->az[i], ax=b->ax[i], ay=b->ay[i];
        float gx=b->gx[i], gy=b->gy[i], gz=b->gz[i];

        sum_az  += az;   sum_az2 += az*az;
        if (az > max_az) max_az = az;
        sum_ax  += ax;   sum_ax2 += ax*ax;
        sum_ay  += ay;   sum_ay2 += ay*ay;
        sum_lat += sqrtf(ax*ax + ay*ay);

        float gm = sqrtf(gx*gx + gy*gy + gz*gz);
        sum_gm  += gm;   sum_gm2 += gm*gm;

        if (i > 0) {
            float dt = b->ts[i] - b->ts[i-1];
            if (dt < 1e-6f) dt = 1e-6f;
            sum_jerk += fabsf(az - b->az[i-1]) / dt;
        }
    }

    f[0]  = sum_az  / n;
    f[1]  = sqrtf(sum_az2/n - f[0]*f[0]);
    f[2]  = max_az;
    f[3]  = sum_ax  / n;
    f[4]  = sqrtf(sum_ax2/n - f[3]*f[3]);
    f[5]  = sum_ay  / n;
    f[6]  = sqrtf(sum_ay2/n - f[5]*f[5]);
    f[7]  = sum_lat / n;
    f[8]  = sum_gm  / n;
    f[9]  = sqrtf(sum_gm2/n - f[8]*f[8]);
    f[10] = sum_jerk / (n - 1);

    // Baro (10 örnek, lineer regresyon eğimi)
    float bsum=0, btsum=0, bbt=0, bt2=0;
    uint8_t nb = IMU_WINDOW / BARO_STEP;
    for (uint8_t i = 0; i < nb; i++) {
        bsum  += b->baro[i];
        btsum += b->baro_t[i];
        bbt   += b->baro[i] * b->baro_t[i];
        bt2   += b->baro_t[i] * b->baro_t[i];
    }
    float baro_mean = bsum / nb;
    float t_mean    = btsum / nb;
    float slope_num = bbt - nb * baro_mean * t_mean;
    float slope_den = bt2 - nb * t_mean * t_mean;
    float baro_var  = 0;
    for (uint8_t i = 0; i < nb; i++)
        baro_var += (b->baro[i]-baro_mean)*(b->baro[i]-baro_mean);

    f[11] = baro_mean;
    f[12] = (fabsf(slope_den) > 1e-6f) ? slope_num / slope_den : 0.0f;
    f[13] = sqrtf(baro_var / nb);
}

static void normalize(float *f) {
    for (uint8_t i = 0; i < N_FEATURES; i++)
        f[i] = (f[i] - FEATURE_MEAN[i]) / FEATURE_SCALE[i];
}
```

---

## STM32Cube.AI Entegrasyonu (ST Edge AI v4.0.0)

1. STM32CubeIDE → **X-CUBE-AI** eklentisini ekle (v4.0.0+)
2. `phase_detector.tflite` dosyasını import et (.h5 yerine — TF sürüm uyumsuzluğu yok)
3. **Analyze** → model doğrulanır (3205 param, 11.8KB Flash, 2.1KB RAM)
4. **Generate Code** → `network.c` / `network.h` / `network_data.c` oluşturulur
5. `App/` klasöründe aşağıdaki init + run kalıbını kullan

### Başlatma (bir kez, main() içinde)

```c
#include "network.h"
#include "network_data.h"
#include <string.h>

// Aktivasyon tamponu: input (14 byte) + output (5 byte) + ara katmanlar buraya sığar
static uint8_t act_buf[STAI_NETWORK_ACTIVATIONS_SIZE] __attribute__((aligned(4)));
static stai_network network_ctx;

// Input/output pointer'ları (set_activations sonrası otomatik atanır)
static stai_ptr network_input_buffers[STAI_NETWORK_IN_NUM];
static stai_ptr network_output_buffers[STAI_NETWORK_OUT_NUM];

void ai_init(void) {
    stai_network_init(&network_ctx);

    stai_ptr act = (stai_ptr)act_buf;
    stai_network_set_activations(&network_ctx, &act, 1);

    // set_activations otomatik olarak input/output'u act_buf içine yerleştirir
    stai_size n;
    stai_network_get_inputs(&network_ctx, network_input_buffers, &n);
    stai_network_get_outputs(&network_ctx, network_output_buffers, &n);
}
```

### Çıkarım (run_inference içinde, her 500ms)

```c
void run_inference(void) {
    if (!run_inference_flag) return;
    run_inference_flag = 0;

    float features[N_FEATURES];
    extract_features(&buf, features);
    normalize(features);

    // Float → int8 quantize
    int8_t *in_ptr = (int8_t *)network_input_buffers[0];
    for (uint8_t i = 0; i < N_FEATURES; i++)
        in_ptr[i] = quantize_input(features[i]);

    stai_network_run(&network_ctx, STAI_MODE_SYNC);

    // Argmax — dequantize gerekmez (scale aynı tüm çıkışlarda)
    int8_t *out_ptr = (int8_t *)network_output_buffers[0];
    phase = 0;
    for (uint8_t i = 1; i < N_CLASSES; i++)
        if (out_ptr[i] > out_ptr[phase]) phase = i;
}
```

### Bellek Özeti (network.h'dan)

```
STAI_NETWORK_ACTIVATIONS_SIZE = 748 bytes  (input+output+ara tampanlar)
STAI_NETWORK_WEIGHTS_SIZE     = 3508 bytes (Flash'te, salt okunur)
Input  offset: act_buf + 732   (14 byte, int8)
Output offset: act_buf + 0     (5 byte,  int8)
```

---

## Performans Referansı

| Faz | F1 Skoru | Not |
|-----|----------|-----|
| PAD | 1.00 | Mükemmel |
| BOOST | 0.99 | Çok iyi |
| COAST | 0.99 | Çok iyi |
| APOGEE | **0.93** | En zor sınıf (%3 veri) |
| DESCENT | 1.00 | Mükemmel |
| **Genel** | **0.9947** | |

APOGEE tespiti %93 — kritikse çıkarım periyodunu 500ms'den 250ms'ye düşür
(IMU_STEP = 50 yap), apoge penceresi daha fazla örtüşür.

---

## Dosyalar

```
outputs/model/
├── phase_detector.h5          ← STM32Cube.AI'ye import edilecek
├── phase_detector.tflite      ← alternatif: TFLite Micro için (8.3 KB)
├── scaler_params.json         ← normalizasyon sabitleri (C'ye kopyalanacak)
├── training_history.png       ← eğitim grafiği
└── confusion_matrix.png       ← sınıf bazında performans
```
