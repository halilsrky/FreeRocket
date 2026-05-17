# SKYRTOS — STM32F446 NUCLEO Flight Computer

Halil'in bitirme projesi. BMI088 IMU + BME280 baro + GNSS  içeren roket flight computer yazılımı.

**Karar değişti:** Kendi mini RTOS kernel'ımızı yazmayacağız.

* Neden: stack overflow, hard fault, race condition, scheduler edge-case ve ISR/priority hataları proje hızını ve güvenilirliğini bozar.
* Hedef: **STM32CubeIDE ile eklenmiş FreeRTOS tabanlı, temiz, okunabilir, adım adım ilerleyen bir mimari**.
* Bu proje artık "kernel yazma" projesi değil; **uçuş bilgisayarı mimarisi ve güvenilirlik projesi**.

## Klasör yapısı

```
FreeRtos_project/       ← aktif proje (STM32CubeIDE .ioc + HAL + FreeRTOS)
├── OLD_Project/        ← SKYRTOS arşivi — referans, driver ve algoritma portu için
├── Report.md           ← bitirme raporu için teknik notlar
└── CLAUDE.md           ← bu dosya
```

**`OLD_Project/` asla silinmeyecek.** Buradan algoritma ve sürücü mantığı port edilecek; doğrudan kopyala-yapıştır değil, okuyup sadeleştirerek uyarlanacak.

**`Report.md`** — bitirme raporu için ham teknik notlar deposu. Sadece rapor değeri olan olaylar yazılır: kök neden, teşhis, çözüm, tekrar etmemesi için alınan önlem. Sıradan refactor notları buraya girmez.

## Git branch yapısı

| Branch | İçerik |
| ------ | ------- |
| `master` | Tam uçuş bilgisayarı — `flight_sm.c` ile klasik faz tespiti |
| `ai` | **Aktif geliştirme** — SUT testinde `flight_sm` yerine ML faz dedektörü |

`ai` branch'inde SUT pipeline tamamen değişti: Mahony + Kalman + flight_sm kaldırıldı, yerini `phase_ml` modülü aldı. Normal uçuş akışı (imu_task, baro_task, flight_sm) dokunulmadan kaldı.

## Aktif proje — mevcut dosya yapısı

### CubeMX üretilen (dokunma)

| Dosya | Görev |
| ----- | ----- |
| `Core/Src/main.c` | HAL init → `osKernelStart()` |
| `Core/Src/freertos.c` | `MX_FREERTOS_Init()` → `Application_Start()` |
| `Core/Src/gpio.c`, `i2c.c`, `dma.c`, `usart.c`, `tim.c` | Peripheral init |
| `Core/Src/stm32f4xx_it.c` | IRQ vektörleri |
| `Middlewares/Third_Party/FreeRTOS/` | FreeRTOS middleware |

### Uygulama katmanı (bizim yazdığımız)

| Dosya | Görev | Durum |
| ----- | ----- | ----- |
| `Core/Src/app.c` / `Inc/app.h` | Tek giriş noktası — task'ları oluşturur | ✅ Tamamlandı |
| `Core/Src/bmi088.c` / `Inc/bmi088.h` | BMI088 driver: init, config, DMA başlat, parse | ✅ Tamamlandı |
| `Core/Inc/bmi088_defs.h` | Register adresleri ve sabitler | ✅ Tamamlandı |
| `Core/Src/mahony.c` / `Inc/mahony.h` | Mahony AHRS filtresi (6DOF) | ✅ Tamamlandı |
| `Core/Src/imu_task.c` / `Inc/imu_task.h` | IMU pipeline task | ✅ Tamamlandı |
| `Core/Inc/imu_snapshot.h` | `imu_snapshot_t` struct + `imu_snapshot_peek()` | ✅ Tamamlandı |
| `Core/Src/telemetry_task.c` / `Inc/telemetry_task.h` | UART2 DMA binary telemetry (50 Hz, addDataPacketNormal formatı) | ✅ Tamamlandı |
| `Core/Src/bme280.c` / `Inc/bme280.h` | BME280 driver: init, config, DMA başlat, parse | ✅ Tamamlandı |
| `Core/Inc/baro_snapshot.h` | `baro_snapshot_t` struct + `baro_snapshot_peek()` | ✅ Tamamlandı |
| `Core/Src/baro_task.c` / `Inc/baro_task.h` | Baro pipeline task (10 Hz, I2C3 DMA) + Kalman tetikleme | ✅ Tamamlandı |
| `Core/Src/alt_kalman.c` / `Inc/alt_kalman.h` | 3-state altitude Kalman filtresi (baro + IMU füzyonu) | ✅ Tamamlandı |
| `Core/Inc/alt_snapshot.h` | `alt_snapshot_t` struct + `alt_snapshot_peek()` | ✅ Tamamlandı |
| `Core/Src/gnss_task.c` / `Inc/gnss_task.h` | GNSS task: baud switch + circular DMA RX + NMEA parse + snapshot | ✅ Tamamlandı |
| `Core/Inc/gnss_snapshot.h` | `gnss_snapshot_t` struct + `gnss_snapshot_peek()` | ✅ Tamamlandı |
| `Core/Src/flight_sm.c` / `Inc/flight_sm.h` | Flight state machine: 7 faz, baro_task'tan 10 Hz tetikleme | ✅ Tamamlandı |
| `Core/Src/phase_ml.c` / `Inc/phase_ml.h` | **[ai branch]** ML faz dedektörü: 200-sample ring buffer + feature extraction + ST Edge AI çıkarım | ✅ Tamamlandı |
| `Core/Src/sut_task.c` / `Inc/sut_task.h` | SUT test pipeline — **ai branch'inde** phase_ml kullanır, master'da flight_sm | ✅ Tamamlandı |
| `Core/Src/cmd_task.c` / `Inc/cmd_task.h` | PC komut ayrıştırma, mod yönetimi | ✅ Tamamlandı |
| `Middlewares/SEGGER/` | SEGGER RTT + SystemView middleware | ✅ Post-Mortem modu aktif |
| `Middlewares/phase_detector.tflite-NUCLEO-F446RE-code/` | **[ai branch]** ST Edge AI v4 üretilen kod: `network.c`, `network_data.c`, `Inc/`, `Lib/*.a` | ✅ Tamamlandı |
| `mlmodel.md` | **[ai branch]** ML model dokümantasyonu: giriş özellikleri, normalizasyon sabitleri, API kullanımı | ✅ Tamamlandı |

### Henüz kapsam dışı

* ~~GNSS (L86)~~
* LoRa (E22)
* ~~Flight state machine~~
* IWDG watchdog
* Gyro offset kalibrasyonu
* Eksen haritalaması (PCB montajına göre ayarlanacak)

## Mimari kararlar

* **FreeRTOS kullanılıyor.** Kendi kernel'ımız yok.
* **AO / QP / ikinci bir scheduler yok.** Tek yürütme modeli FreeRTOS.
* **Blocking çağrılar minimumda.** `HAL_Delay` yok; init sırasında `vTaskDelay` kullanılıyor.
* **ISR içinde iş minimum.** ISR sadece `xTaskNotifyFromISR` çağırır, başka iş yapmaz.
* **Veri sahipliği tek kaynaklı.** IMU task kendi state'ini tek başına yönetir; pointer paylaşmaz.
* **Watchdog (IWDG) şart.** Henüz eklenmedi — öncelikli sonraki adım.
* **Synchronization primitif seçimi:**
  * ISR → task: `task notification` (`xTaskNotifyFromISR` + `xTaskNotifyWait`)
  * Task → task (snapshot): `queue depth=1` + `xQueueOverwrite` / `xQueuePeek`
  * Mutex: IMU path'inde kullanılmıyor
  * Semaphore: kullanılmıyor
* **Mode geçişleri explicit state machine ile yönetilecek.**


## Task modeli

| Task | Öncelik | Stack | Görev |
| ---- | ------- | ----- | ----- |
| IMU | `osPriorityHigh` | 512 × 4 B | Sensör okuma + Mahony + snapshot publish |
| Baro | `osPriorityBelowNormal` | 512 × 4 B | 10 Hz I2C3 DMA baro okuma + Kalman güncelleme + snapshot |
| GNSS | `osPriorityBelowNormal` | 512 × 4 B | USART6 circular DMA RX + NMEA parse + snapshot (1 Hz) |
| Telemetry | `osPriorityBelowNormal` | 256 × 4 B | 50 Hz UART2 DMA TX |
| SUT | `osPriorityNormal` | 512 × 4 B | SUT test pipeline — paket al, phase_ml çalıştır, response gönder |
| CMD | `osPriorityNormal` | 256 × 4 B | PC komutlarını ayrıştır, mod geçişlerini yönet |

## Donanım haritası

| Peripheral | Pin | Görev |
| ---------- | --- | ----- |
| I2C1 | PB8 SCL, PB9 SDA | BMI088 (ACC + GYRO ortak bus) |
| I2C3 | PA8 SCL, PC9 SDA | BME280 |
| EXTI3 | PB3 | BMI088 ACC DRDY |
| EXTI4 | PB4 | BMI088 GYRO DRDY |
| USART2 | DMA TX/RX | Telemetry (ST-Link VCP) |
| USART6 | DMA RX | GNSS (L86) |
| UART4 | DMA TX | LoRa (E22) |
| SPI1 / SPI3 | — | Flash / RF |
| BUZZER | PB14 | Durum sinyali |

## Build

```powershell
# FreeRtos_project/ klasöründen
cmake --preset Debug
cmake --build build/Debug
```

Çıktı: `FreeRtos_project/build/Debug/*.elf` + `.hex` + `.bin`

Mevcut boyutlar: FLASH ~75 KB / 512 KB (%14), RAM ~66 KB / 128 KB (%50).

## SEGGER SystemView — Post-Mortem modu
JTAG/canlı bağlantı olmadan çalışır. Sistem çalışırken olaylar RAM'deki ring buffer'a yazılır. Sonra debugger ile buffer dump edilip SystemView masaüstü uygulamasında açılır.


## old_project'ten port durumu

| Dosya | İçerik | Durum |
| ----- | ------- | ----- |
| `bmi088.c` | BMI088 driver | ✅ Yeniden yazıldı, port edildi |
| `queternion.c` | Mahony quaternion | ✅ `mahony.c` olarak port edildi |
| `bme280.c` | BME280 driver | ✅ Yeniden yazıldı, I2C3 DMA ile port edildi |
| `kalman.c` | Altitude Kalman filter | ✅ `alt_kalman.c` olarak port edildi, sadeleştirildi |
| `flight_algorithm.c` | Flight state machine | Sonraki aşama |
| `sensor_fusion.c` | Fusion mantığı | Sonraki aşama |
| `l86_gnss.c` | GNSS | ✅ `gnss_task.c` olarak port edildi, FreeRTOS task mimarisine uyarlandı |
| `uart_handler.c` | UART komut ayrıştırma | Sonraki aşama |

## Öncelikli yol haritası

1. ✅ FreeRTOS proje iskeletini doğrula.
2. ✅ `Application_Start()` üzerinden tek giriş noktası kur.
3. ✅ IMU pipeline: DRDY IRQ → DMA → parse → Mahony → snapshot.
4. ✅ Telemetry task: snapshot → UART2 DMA binary frame.
5. ✅ SEGGER SystemView Post-Mortem modu: ring buffer kaydı + GDB dump.
6. ✅ BME280 driver ve baro task (I2C3 DMA, 10 Hz).
7. ✅ Altitude Kalman filtresi (baro + IMU füzyonu, 10 Hz, baro_task içinde).
8. ✅ GNSS task (USART6 circular DMA, NMEA parse, snapshot).
9. ✅ Flight state machine (`flight_sm.c`, 7 faz, baro_task 10 Hz tetiklemeli).
10. ✅ SIT/SUT test altyapısı (cmd_task + sut_task, MODE_SUT).
11. ✅ **[ai branch]** ML faz dedektörü: `phase_ml.c` — ST Edge AI v4, int8 TFLite, 5 sınıf, 200-sample pencere.
12. ✅ **[ai branch]** SUT pipeline'ı ML modele bağlandı; raw veri → çıkarım → FSM bit flag map → PC response.

## AI branch — phase_ml mimarisi

### Çalışma prensibi

```
SUT paketi (PC → STM32)
  └─► phase_ml_push()   × count  (IMU örnekleri, raw — filtresiz)
  └─► phase_ml_push_baro()       (ham baro yüksekliği)
       │
       ▼ her 100 örnekte bir (500 ms @ 200 Hz)
  extract_features()   → 14 float özellik
  normalize()          → Z-score (FEATURE_MEAN / FEATURE_SCALE)
  quantize()           → int8[14]   (scale=0.1929, zp=−8)
  stai_network_run()   → int8[5]    argmax → phase (0-4)
       │
       ▼
  PHASE_TO_STATUS[phase]  → uint16_t bit flags (PC formatı)
  send_response(baro_t, raw_alt, 0, 0, 0, status)
```

### PHASE_TO_STATUS eşleşmesi

PC, `status` alanını `flight_sm.c`'nin ürettiği kümülatif `FSM_BIT_*` flag olarak okur.
ML modeli 0–4 tamsayı çıkardığından, ham değer doğrudan gönderilemez.

| ML Faz | Değer | Aktif FSM bitleri |
|--------|-------|-------------------|
| 0 PAD | `0x0000` | — |
| 1 BOOST | `0x0001` | LAUNCHED |
| 2 COAST | `0x0007` | LAUNCHED \| BURNOUT \| ARMED |
| 3 APOGEE | `0x0217` | + APOGEE \| VEL\_APOGEE |
| 4 DESCENT | `0x0237` | + DROGUE |

### ML model teknik özeti

| Parametre | Değer |
|-----------|-------|
| Mimari | Dense: 14 → 64 → 32 → 5 |
| Format | int8 TFLite (ST Edge AI v4) |
| Flash | 11.8 KB |
| RAM (aktivasyon) | 748 B |
| MACC | 3280 (~0.1 ms @ 180 MHz) |
| Test doğruluğu | %99.47 |
| Pencere | 200 IMU + 10 baro, her 100 örnekte çıkarım |

### CMake linkleme (ai branch)

```cmake
# cmake/stm32cubemx/CMakeLists.txt
MX_Include_Dirs  += Middlewares/phase_detector.tflite-NUCLEO-F446RE-code/Inc
                 += Middlewares/phase_detector.tflite-NUCLEO-F446RE-code
MX_Application_Src += Core/Src/phase_ml.c
                   += Middlewares/.../network.c
                   += Middlewares/.../network_data.c
MX_LINK_DIRS     += Middlewares/.../Lib
MX_LINK_LIBS     += :NetworkRuntime1200_CM4_GCC.a  m
```

Detaylı model bilgisi → `mlmodel.md`

## Şu anki kritik hatalar (old_project'ten gelen, yeni projede tekrar edilmeyecek)

### Çözüldü
* ~~Blocking boot akışı~~ — `vTaskDelay` kullanılıyor.
* ~~ISR içinde fazla iş~~ — ISR sadece notify gönderir.
* ~~Veri tutarsızlığı~~ — `xQueueOverwrite` ile atomik snapshot.
* ~~Stack güvenliği yok~~ — `configCHECK_FOR_STACK_OVERFLOW 2` + hook aktif.
* ~~Boot sırasında interrupt açmak~~ — IRQ'lar handle set edildikten sonra açılıyor.
* ~~IWDG yok~~ — `iwdg.c` modülü eklendi; prescaler/256, reload 249 → ~2 s timeout; `baro_task` her 100 ms besliyor.
* ~~Fatal handler infinite loop~~ — `Error_Handler()` artık `NVIC_SystemReset()` çağırıyor.

### Hâlâ açık
* **Gyro kalibrasyonu yok** — Mahony bias hatası ile başlıyor.

## Kod yazma kuralları

* **Adım adım ilerle.** Tek seferde büyük refactor yok.
* **Her adım doğrulanmadan bir sonrakine geçme.**
* **Tek sahipli modül yaklaşımı.** Bir modülün iç state'i başka task tarafından doğrudan yazılmaz.
* **Her yeni modül için önce arayüz, sonra implementasyon.**
* **Comment sadece gerekli yerde.** Görünmeyen nedeni açıkla; bariz olanı tekrar etme.
* **Identifier'lar İngilizce.** Açıklama Türkçe olabilir.
* **`OLD_Project/` gerekli yerlerde referans alınabilir.** Aynı algoritmanın temiz versiyonu hedeflenir, birebir kopyalanmaz.

## Halil hakkında

* Türkçe konuşur.
* Junior embedded geliştirici.
* Bitirme projesinde çalışan, anlaşılır ve jüriye anlatılabilir kod ister.
* Büyük refactor yerine küçük, doğrulanabilir adımlarla ilerlemek daha uygundur.

## Değişmez prensip

**Önce güvenilir ve anlaşılır çalışan basit sistem. Sonra genişletme.**

Spagetti yok. Kernel yazmak yok. Blocking yok. Tek sahipli veri akışı var. Adım adım ilerliyoruz.
