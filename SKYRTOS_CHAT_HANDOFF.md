# SKYRTOS → Mini RTOS Migration — Claude Chat Handoff

> **Bu dosyanın amacı:** Halil'in SKYRTOS (mevcut FreeRTOS-tabanlı roket flight computer) projesinin mevcut durumunu, tespit edilen sorunları, planlanan yeni mimariyi ve verilen kararları başka bir Claude oturumuna devretmek. Halil bu dosyayı kendi mini RTOS projesinde açacak ve oradan migration üzerinde çalışacak.

---

## 1. Kullanıcı Bağlamı

- **Kim:** Halil Sarıkaya (halilsarikaya070@gmail.com)
- **Proje:** SkyLord2 / SKYRTOS — STM32F446 tabanlı roket flight computer
- **Mevcut durum:** FreeRTOS 10.3.1 + CMSIS-RTOS V2 üzerinde çalışıyor
- **Hedef:** Bitirme projesi (öğrenme amaçlı, gerçek uçuş YOK). Kendi yazdığı **preemptive mini RTOS**'unu SKYRTOS'a entegre etmek istiyor.
- **Dil tercihi:** Türkçe konuşuyor.
- **Teknik seviye:** Junior düzeyde embedded geliştirici. 

---

## 2. SKYRTOS Mevcut Mimarisi (Kısa Özet)

### Donanım
- MCU: STM32F446 @ 168 MHz, FPU enabled
- IMU: BMI088 (I2C1, EXTI3 = accel DRDY, EXTI4 = gyro DRDY)
- Barometre/Env: BME280 (I2C3)
- GNSS: L86 (USART6, DMA RX)
- LoRa: E22 (UART4)
- Telemetri/Komut: USART2 (DMA RX/TX, idle line detection)
- ADC1/2/3: Magnetometer (HMC1021), Voltage, Current
- Storage: SD Card (SPI3 + FATFS)

### Yazılım Mimarisi (FreeRTOS)

**9 Task:**
| Task | Priority | Pattern | Sorumluluk |
|---|---|---|---|
| bmiTask | Realtime | Event-driven (thread flags) | BMI088 DMA done → Mahony → mailbox |
| bmeTask | High | Periodic 100ms (vTaskDelayUntil) | BME280 polling read |
| sensorFusionTask | AboveNormal | Hybrid (BME flag + 200ms timeout) | Kalman + flight algorithm |
| testModeTask | High | Periodic 100ms | NORMAL/SIT/SUT mode dispatcher |
| adcTask | Normal | Periodic 100ms (offset 20ms) | ADC polling read |
| gnssTask | Normal | Periodic 100ms (offset 40ms) | NMEA parse |
| telemetryTask | Normal | Periodic 100ms (offset 60ms) | Packet build + UART2 DMA |
| dataLoggerTask | BelowNormal | Periodic 100ms (offset 80ms) | SD card write |
| defaultTask | Normal | Idle | SystemView start |

**IPC:** 5 mailbox queue (depth=1, xQueueOverwrite + xQueuePeek pattern):
- bmiMailbox, bmeMailbox, gnssMailbox, adcMailbox, fusedMailbox

**Heap:** 15360 byte (heap_4)

---

## 3. Tespit Edilen Kritik Hatalar (Önceki Konuşma Bulguları)

### 🔴 Kritik (uçuş güvenliğini bozar)
1. **`data_logger_init()` yorum satırında** ([main.c:278](Core/Src/main.c#L278)) → SD logging hiç başlamıyor, sessizce fail.
2. **`flush_packet_buffer()` asla çağrılmıyor** → Power-off/abort anında son 7 paket kayıp.
3. **`Error_Handler()` `__disable_irq()` + `while(1)`** → Herhangi bir HAL hatasında sistem ölür, watchdog yok.
4. **`bmiTask` accel VEYA gyro flag'inde mailbox günceller** → Yarı stale veri gönderiyor (accel taze, gyro eski veya tersi).
5. **`sensor_fusion_init` imza uyumsuzluğu** → main.c'de `sensor_fusion_init(&BME280_sensor)`, .c'de `void`. K&R sebebiyle silently ignored.
6. **`get_offset()` boot'ta 10sn while(1) loop** → Scheduler henüz başlamamışken blocking.
7. **`HAL_I2C_MemRxCpltCallback`'tan `osThreadFlagsSet`** → Doğrusu ISR-safe API, ama `portYIELD_FROM_ISR` çağrılmıyor → context switch gecikmesi.

### 🟠 Orta
- `read_ADC()` 3× blocking poll (5ms timeout each)
- `read_ADC()` içinde LoRa modu değişimi → battery noise → sürekli flap
- `flight_algorithm_reset()` UART noise ile uçuşta tetiklenebilir → state sıfırlanır
- `sensorFusionTask` mode dispatcher busy-poll
- `errorLine` global'i hiç kontrol edilmiyor
- Stack overflow detection yok (configCHECK_FOR_STACK_OVERFLOW tanımsız)

---

## 4. Mimari Asıl Sorun (Halil'in Tespiti)

> *"FreeRTOS'u şimdi bare-metal süper döngü gibi kullanıyorum."*

Doğru tespit. 9 task'ın 6'sı `vTaskDelayUntil` ile periodic — yani while(1) + timer'ın task'a bölünmüş hali. RTOS'un asıl gücü olan **event-driven dispatch** kullanılmıyor.

**Kullanmama nedeni:** Mailbox depth=1 = mutexli global, vTaskDelayUntil = adam akıllı `delay()`. Producer-consumer queue, task notification, event group, software timer gibi gerçek event-driven primitive'ler kullanılmıyor.

---

## 5. Halil'in Yeni Sistem Tasarımı (Onaylanmış)

İki bağımsız faz dedektörü + redundancy:

### Algoritma 1 (IMU-only)
```
EXTI BMI_DRDY → I2C DMA başlat
I2C DMA done → Mahony (orientation update)
TIM 100ms → algoritma1 (sadece IMU verisiyle faz tespiti)
```
Avantaj: Barometre arızalansa bile çalışır.

### Algoritma 2 (Baro-based)
```
TIM 100ms → BME280 oku (DMA) → Kalman update → algoritma2
Tek event ile 3 işlem.
```
Avantaj: IMU arızalansa bile çalışır (Kalman IMU'yu kullanıyor ama tolere ediyor).

### Voting/Consensus Layer
İki algoritma farklı faz önerirse:
- **OR**: biri APOGEE derse APOGEE (yanlış pozitif riski)
- **AND + timeout**: ikisi de aynı şeyi söylerse veya 500ms üst üste tek başına derse geç (ÖNERİLEN)
- **3-out-of-2 voting**: GNSS altitude trend'i üçüncü kanıt olarak ekle

### GNSS + Telemetry: Timer-based (interrupt yok, periyodik)
ADC: silinecek (gereksiz).

---

## 6. Önerilen 3-Task Event-Driven Layout

```
ISR DOMAIN (sadece flag/notify, hiç logic yok):
  EXTI BMI_DRDY (accel/gyro)  → DMA başlat
  I2C1 DMA done                → POST(EVT_IMU_RAW)
  TIM 100ms                    → POST(EVT_TICK_100MS)
  TIM 1000ms (sw timer)        → POST(EVT_TICK_1S)
  I2C3 DMA done (BME)          → POST(EVT_BME_RAW)
  USART2 idle                  → POST(EVT_CMD_RX)
  USART6 idle (GNSS)           → POST(EVT_GNSS_RX)
  UART4 TX done                → POST(EVT_LORA_TX_DONE)

TASK DOMAIN (3 task, hepsi event-driven, hiçbiri delay/poll yapmaz):

imuTask [Realtime]
  wait(EVT_IMU_RAW)
    parse_accel + parse_gyro
    mahony_update()
    atomic_store(shared_imu_state)

flightTask [High]
  wait(EVT_TICK_100MS | EVT_BME_RAW)
  if EVT_TICK_100MS:
    i2c_bme_start_dma()
    phase1 = algorithm1(shared_imu_state)
  if EVT_BME_RAW:
    kalman_update(bme_data, shared_imu_accel_z)
    phase2 = algorithm2(kalman_out)
    final_phase = consensus(phase1, phase2)
    deploy_gpio(final_phase)

commTask [Normal]
  wait(EVT_TICK_100MS | EVT_TICK_1S | EVT_CMD_RX | EVT_GNSS_RX | EVT_LORA_TX_DONE)
  EVT_TICK_100MS: build telem packet, UART DMA
  EVT_TICK_1S:    SD log flush
  EVT_CMD_RX:     parse, mode switch
  EVT_GNSS_RX:    NMEA parse
```

**Kritik tasarım kararları:**
- Mahony I2C DMA complete ISR'ında DEĞİL, task'ta çalışacak (ISR latency düşük, debug kolay)
- BME280 oku DA event-driven (DMA + complete event), polling yok
- Algoritma1 + Algoritma2 ayrı task değil, **aynı flightTask içinde** sırayla — voting senkron olsun
- Shared IMU state için **mutex değil, atomic snapshot** (Cortex-M4 32-bit yazma atomic)

---

## 7. Verilen Kararlar (Halil ile Hizalanmış)

| Karar | Sonuç | Sebep |
|---|---|---|
| Mini RTOS yazılacak mı? | **EVET** | Bitirme projesi, öğrenme amaçlı, jüri etkisi yüksek |
| FreeRTOS kalsın mı? | **HAYIR**, atılacak | İki kernel paralel çalışmaz; debugger karışır |
| Cooperative mi preemptive mi? | **PREEMPTIVE** | Halil zaten preemptive yazmış başka projede, onu kullanacak |
| AO/QP framework yazılsın mı? | **HAYIR** | Overkill, scope blowup. Düz event queue + dispatcher yeterli |
| Mevcut SKYRTOS'taki bug'ları FreeRTOS'ta düzeltelim mi? | **HAYIR** | Migration zaten yapılacak, çift iş |

---

## 8. Sıradaki Adım — Mini RTOS Migration

### Halil'in açması gereken kapı
Halil'in **kendi mini RTOS projesinin** yolunu Claude'a verecek. Workspace `c:\Users\Halil\STM32CubeIDE\workspace_1.14.1\` altında. Glob ile bakıldığında olası adaylar:
- `FreeRTOS_01`, `PayLordFreeRTOS`, `SKYRTOSVS`, veya başka bir custom proje

Halil bunu netleştirip path verince Claude şunları yapmalı:
1. Mini RTOS'un kernel API'sini oku ve özetle (task creation, event posting, scheduler, ISR API)
2. Mini RTOS'un mevcut özelliklerini bu handoff'taki "Önerilen 3-Task Layout"a göre değerlendir
3. Eksik primitive var mı tespit et (örn. event queue, software timer)
4. SKYRTOS migration'ını adım adım planla

### Migration adım planı (önerilen sıra)
1. **Boot kodunu mini RTOS'a port et** — sensör init'leri scheduler ÖNCESİ değil, init AO/task'ında yap
2. **`imuTask`'i kur** — EXTI + I2C DMA + Mahony zinciri çalışsın
3. **`flightTask`'i kur** — algoritma1 + algoritma2 + voting
4. **`commTask`'i kur** — telem + UART RX + GNSS + SD
5. **Watchdog ekle (IWDG)** — her task döngüsünde refresh
6. **SystemView veya alternatif tracer** ekle (kendi mini RTOS için custom hook)

---

## 9. Diğer Claude için Talimatlar

Bu dosyayı okuduktan sonra:

1. **Halil Türkçe konuşur**, sen de Türkçe yanıtla.
2. **Senior seviye teknik dil** kullan, basitleştirmeye gerek yok.
3. **Önce mini RTOS'un kodunu oku**, sonra SKYRTOS'a uyarlama planla. Aksini varsayma.
4. SKYRTOS kodu bu konuşmada görüldü, sende olmayabilir — Halil sana SKYRTOS path'ini de açabilir veya sadece bu özetle çalışman istenebilir.
5. **Üç prensibe uy:**
   - Hiçbir task'ta `HAL_Delay`, `HAL_*_Poll*`, `f_write` blocking olmamalı
   - Tüm I/O DMA + complete callback ile event post etmeli
   - Shared state atomic snapshot, mutex'e mümkün olduğunca girme
6. **Halil "bare-metal süper döngü" anti-pattern'inden kaçınmak istiyor**. Periodic task ekleme önerme — software timer + event post tercih et.
7. **Watchdog (IWDG) ekle** her senaryoda — bitirme projesi de olsa.

---

## 10. Hızlı Bağlam: Önemli SKYRTOS Dosya Yolları

```
SKYRTOS workspace: c:\Users\Halil\STM32CubeIDE\workspace_1.14.1\SKYRTOS

Core/Src/main.c                   — Boot + 8 task implementation
Core/Src/freertos.c                — Task definitions, MX_FREERTOS_Init
Core/Src/sensor_mailbox.c          — 5x mailbox IPC
Core/Src/sensor_fusion.c           — Kalman + accel failure detection
Core/Src/flight_algorithm.c        — Flight phase state machine (HSM benzeri)
Core/Src/bmi088.c                  — IMU driver, DMA chain
Core/Src/uart_handler.c            — UART2 RX, mode dispatcher
Core/Src/test_modes.c              — SIT/SUT test mode handlers
Core/Src/data_logger.c             — SD write buffering
Core/Src/packet.c                  — Telemetry packet build
Core/Src/queternion.c              — Mahony quaternion (typo: queternion)
Core/Inc/FreeRTOSConfig.h          — heap=15KB, prio=56, tick=1kHz
Core/Inc/sensor_mailbox.h          — Sample struct definitions
```

---

**Son not:** Bu dosya Halil tarafından bir Claude Code oturumuna fed edildiğinde, Claude konuşmaya bu özetten devam etmeli — geriye dönüp "konuşmayı baştan başlatalım" demeden. Halil zaman kaybetmek istemiyor; doğrudan mini RTOS migration'ına geçmek istiyor.
