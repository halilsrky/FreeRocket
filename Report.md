# Report.md — Bitirme Projesi II — Final Rapor Yazım Kılavuzu

> Bu dosya yapay zeka destekli rapor yazımı için hazırlanmış bir iskelet + TODO listesidir.
> Her başlık altında: ne yazılacağı, hangi kaynak koddan alıntı yapılacağı ve hangi şekil/grafik/diyagramın ekleneceği belirtilmiştir.
> Şablon: İZÜ LaTeX 5-Bölüm formatı.

---

## ÖZET

**Ne yazılacak:**
- Projenin amacı: STM32F446 üzerinde FreeRTOS tabanlı model roket uçuş bilgisayarı
- Kullanılan yöntemler: BMI088 + BME280 + L86 GNSS, Mahony AHRS, Kalman irtifa filtresi, 7-faz FSM
- Doğrulama yöntemi: SIT (gerçek sensör) + SUT (RocketPy verisinden Processor-in-the-Loop)
- Temel bulgular: bmiTask %2.39 CPU, sistemin hard real-time kısıtları karşıladığı, SUT'ta uçuş fazlarının doğru geçtiği
- Anahtar Kelimeler: FreeRTOS, STM32F446, Kalman Filtresi, Mahony AHRS, Uçuş Durum Makinesi, Processor-in-the-Loop

---

## 1. BÖLÜM I — GİRİŞ (YAPILDI)

### 1.1. AMAÇ

**Ne yazılacak:**
- Model roket aviyoniklerinde güvenilir, deterministik ve test edilebilir bir yazılım mimarisi kurmanın önemi
- Çalışmanın hedefleri: donanım sürücüleri, sensör füzyonu, uçuş algoritması ve PIL doğrulama zincirinin tamamlanması
- Birinci rapordan bu yana eklenen modüller: BME280, alt_kalman, gnss_task, flight_sm, IWDG, SUT

### 1.2. GİRİŞ

**Ne yazılacak:**
- Model roket sistemlerinin artan yazılım karmaşıklığı
- Bare-metal yaklaşımın sınırları vs. RTOS tabanlı mimari avantajları (determinizm, preemption, görev izolasyonu)
- Linux'un gömülü kritik sistemler için neden uygun olmadığı (jitter, determinizm eksikliği)
- Bu çalışmada uygulanan yaklaşım özeti
- Raporun bölüm yapısı hakkında kısa yol haritası

---

## 2. BÖLÜM II — GENEL KAVRAMLAR VE LİTERATÜR (YAPILDI - Alt başlık 2.5 LİTERATÜR TARAMASI Tamamlandı)

### 2.1. Model Roket Aviyonik Sistemleri

**Ne yazılacak:**
- Tipik aviyonik sistem bileşenleri: IMU, barometrik sensör, GNSS, telemetri, recovery sistemi
- Uçuş sırasında zamansal kısıtlar: sensör okuma, durum hesaplama, recovery tetikleme
- Determinizm ve zamanlama garantisinin kritikliği (örn. apogee tespitinde 100 ms gecikme → yanlış paraşüt açılımı)

### 2.2. Gerçek Zamanlı Sistemler ve Determinizm

**Ne yazılacak:**
- Hard vs. soft gerçek zamanlı sistem tanımı
- Jitter ve latency kavramları
- Deadline miss'in sonuçları (roket örneği üzerinden)
- Preemptive scheduling ve öncelik tabanlı zamanlama (Liu & Layland referansı)

### 2.3. STM32F446 Mikrodenetleyici Mimarisi

**Ne yazılacak:**
- ARM Cortex-M4 + FPU: sensör füzyonu için önemi
- NVIC, DMA, I2C, USART çevresel birimleri
- 180 MHz @ 512 KB Flash, 128 KB RAM: kaynak kısıtlarının mimari kararlarla ilişkisi
- CubeMX ile HAL üretimi ve FreeRTOS entegrasyonu

> **TODO — Şekil:** STM32F446 donanım blok diyagramı (I2C1→BMI088, I2C3→BME280, USART6→GNSS, USART2→Telemetri bağlantıları). Draw.io veya CubeMX pin görünümünden oluşturulacak.

### 2.4. Sensörler ve Ölçüm Altyapısı

**Ne yazılacak:**
- **BMI088:** 6-eksenli IMU, 24g ivme dayanımı, titreşim altında kararlı jiroskop. Roket uygulamaları için neden seçildi (datasheet referansı [11])
- **BME280:** Barometrik basınç sensörü, irtifa hesabı için hypsometric formülü. Tepki süresinin IMU'ya göre yavaş olmasının füzyon kararına etkisi ([12])
- **L86 GNSS:** NMEA ayrıştırma, 1 Hz konum verisi, baud hızı geçişi

### 2.5. LİTERATÜR TARAMASI

#### 2.5.1. FreeRTOS Mimarisi ve Görev Yönetimi

**Ne yazılacak:**
- FreeRTOS task durumları: Ready, Running, Blocked, Suspended
- pxReadyTasksLists ile zamanlama mekanizması
- Preemptive multitasking: yüksek öncelikli görev her zaman düşük öncelikliyi keser
- ISR'dan task notification: `xTaskNotifyFromISR` + `portYIELD_FROM_ISR` kalıbı
- Queue mailbox pattern: `xQueueOverwrite` (producer) + `xQueuePeek` (consumer) — neden depth=1 yeterli

> **TODO — Kod alıntısı:** `imu_task.c` içindeki ISR callback + notify zinciri (HAL_GPIO_EXTI_Callback → xTaskNotifyFromISR → xTaskNotifyWait)

#### 2.5.2. Sensör Füzyonu Yöntemleri (Mahony, Kalman)

**Ne yazılacak:**
- Jiroskop drift problemi ve ivmeölçer gürültüsü — neden tek sensör yetmez
- **Tamamlayıcı filtre:** formül θt = α(θt−1 + ωΔt) + (1−α)θacc, basit ama yüksek dinamikte yetersiz
- **Mahony filtresi:** quaternion tabanlı, PI kontrolcülü geri besleme. Formüller: q̇ = ½q⊗(ωgyro − b + Kp·e + Ki·∫e dt), e = ĝ × anorm. Gömülü sistemlerde düşük maliyet ([8])
- **Kalman filtresi:** durum vektörü [irtifa, hız, ivme_bias], sabit-ivme geçiş modeli F, gözlem matrisi H. Q ve R parametrelerinin anlamı

> **TODO — Denklem:** Mahony hata vektörü ve quaternion entegrasyonu (LaTeX)
> **TODO — Denklem:** Kalman tahmin (F, Q) ve güncelleme (K, H, R) denklemleri (LaTeX)

#### 2.5.3. Uçuş Durum Makinesi Yaklaşımları

**Ne yazılacak:**
- FSM'in aviyonikte tercih edilme nedeni: deterministik geçişler, test edilebilirlik, izlenebilirlik
- IDLE → BOOST → COAST → APOGEE → DROGUE_DESCENT → MAIN_DESCENT → LANDED yapısının literatürdeki benzerleri
- Çoklu apogee tespiti: hız bazlı (Kalman velocity < 0) + tilt bazlı (theta > 70°) — güvenilirlik için iki bağımsız kanal

---

## 3. BÖLÜM III — SİSTEM MİMARİSİ VE YÖNTEM (YAPILDI - Alt başlık 3.4, 3.5, 3.6, 3.7, 3.8)

### 3.1. Katmanlı Yazılım Mimarisi

**Ne yazılacak:**
- 4 katman: Donanım → HAL → Sürücü/Middleware → Uygulama
- Her katmanın sorumluluğu ve tek yönlü bağımlılık ilkesi
- CubeMX üretilen dosyalara neden dokunulmadığı

> **TODO — Şekil:** Katmanlı mimari diyagramı. İlk rapordaki Şekil 1'i güncellenmiş haliyle kullan (SIT/SUT tool'u da ekle). Mevcut: HAL + FreeRTOS + Driver layer + Application layer + SUT Python tool.

### 3.2. Donanım Haritası ve Çevresel Birimler

**Ne yazılacak:**
- Pin haritası: I2C1 (PB8/PB9) → BMI088, I2C3 (PA8/PC9) → BME280, USART6 → L86, USART2 → Telemetri/SIT, EXTI3/EXTI4 → DRDY sinyalleri, PB14 → Buzzer
- DMA kanalları ve öncelik seviyeleri
- IWDG: prescaler/256, reload 249 → ~2 s timeout

> **TODO — Tablo:** Peripheral → Pin → Görev → Protocol tablosu (CLAUDE.md'deki donanım haritasından genişletilecek)

### 3.3. FreeRTOS Görev Mimarisi

**Ne yazılacak:**
- 4 görev: IMU (osPriorityHigh), Baro (osPriorityBelowNormal), GNSS (osPriorityBelowNormal), Telemetri (osPriorityBelowNormal)
- SUT modunda IMU/Baro/Telemetri/GNSS görevleri neden uyuyor (MODE_SUT kontrolü)
- ISR → task notification zinciri: DRDY pin → EXTI → xTaskNotifyFromISR → imu_task uyanır → DMA başlatır → DMA complete callback → notify → parse → snapshot
- Snapshot mekanizması: depth=1 queue + xQueueOverwrite + xQueuePeek (lock-free okuma)
- Stack boyutları ve configCHECK_FOR_STACK_OVERFLOW=2

> **TODO — Şekil:** FreeRTOS görev mimarisi diyagramı (ilk rapordaki Şekil 2'yi güncelle: gnss_task, flight_sm, SUT modu uyku akışı eklendi)
> **TODO — Kod alıntısı:** `imu_task.c` içinde DMA state machine (DMA_IDLE / DMA_READING_ACC / DMA_READING_GYRO enum)
> **TODO — Tablo:** Görev → Öncelik → Stack → Tetikleme → Periyot tablosu

### 3.4. Sensör Sürücüleri

#### 3.4.1. BMI088 IMU Sürücüsü (I2C + DMA + DRDY IRQ)

**Ne yazılacak:**
- I2C1 üzerinde ACC ve GYRO ayrı adresli (BMI088 mimarisi)
- DRDY pinlerinden kesme tabanlı tetikleme: EXTI3 (ACC PB3), EXTI4 (GYRO PB4)
- DMA ile non-blocking okuma: `bmi088_start_accel_dma` / `bmi088_start_gyro_dma`
- Ham veri parse: `bmi088_parse_accel` (12G range → m/s²), `bmi088_parse_gyro` (2000 dps → rad/s)
- Init başarısız olursa görev silinir, sistem baro-only modda devam eder

> **TODO — Kod alıntısı:** `imu_task.c` init + retry döngüsü (satır 48–63)
> **TODO — Kod alıntısı:** `imu_task.c` DMA state machine: acc/gyro DRDY → DMA → parse → Mahony güncelleme (satır 83–151)

#### 3.4.2. BME280 Barometrik Basınç Sürücüsü

**Ne yazılacak:**
- I2C3 üzerinde, 10 Hz periyodik okuma
- Oversampling konfigürasyonu ve kompanzasyon formülleri (Bosch datasheet)
- Ham basınçtan irtifaya dönüşüm: hypsometric formül, referans irtifa kalibrasyonu (boot'ta ilk N örnek ortalaması)
- baro_task: 100 ms vTaskDelayUntil → okuma → parse → baro_snapshot → Kalman tetikleme → flight_sm_update

> **TODO — Kod alıntısı:** `baro_task.c` ana döngü (irtifa hesabı + kalman çağrısı + flight_sm tetikleme)

#### 3.4.3. L86 GNSS Modülü (Circular DMA, NMEA)

**Ne yazılacak:**
- USART6 üzerinde circular DMA RX — CPU kesintisiz buffer dolduruyor
- Başlangıçta baud hızı geçişi (9600 → 115200)
- NMEA GPRMC / GPGGA cümleleri ayrıştırma: lat, lon, fix, HDOP, hız
- gnss_snapshot: 1 Hz güncelleme, telemetri paketine ekleniyor
- GNSS fix yoksa snapshot geçersiz olarak işaretleniyor

> **TODO — Kod alıntısı:** `gnss_task.c` circular DMA okuma ve NMEA parse tetikleme mantığı

### 3.5. Sensör Füzyonu ve Durum Kestirimi

#### 3.5.1. Mahony AHRS Filtresi (6DOF Quaternion)

**Ne yazılacak:**
- Quaternion temsili ve gimbal lock'tan kaçınma
- Mahony algoritması adım adım: normalize ivme → tahmin edilen yerçekimi (quaternion'dan) → cross-product hata → PI düzeltme → quaternion entegrasyonu → normalize
- Parametreler: Kp=0.5, Ki=0.0 (gyro-only SUT modunda Ki anlamsız)
- `inv_sqrt` (Quake III fast inverse sqrt) — neden kullanıldı: FPU olsa da performans tasarrufu
- Euler açılarına dönüşüm: roll/pitch/yaw ve tilt açısı theta

> **TODO — Denklem:** Hata hesabı e = ĝ × anorm, quaternion türevi q̇ = ½q⊗ω_korr (LaTeX)
> **TODO — Kod alıntısı:** `mahony.c` — `mahony_update` fonksiyonu (satır 26–91), `mahony_get_euler` (satır 107–120)

#### 3.5.2. Yükseklik Kalman Filtresi (Baro + IMU)

**Ne yazılacak:**
- 3-boyutlu durum vektörü: x = [irtifa, hız, ivme_bias]
- Sabit-ivme hareket modeli: F matrisi (1, dt, dt²/2 satırları)
- Process noise Q: dt kuvvetlerine bağlı ölçekleme
- Gözlem matrisi H: sadece irtifa (baro) ve ivme (IMU) gözlemleniyor
- r_alt=5.0 (baro gürültüsü), r_acc=10.0 (ivme gürültüsü) — tuning mantığı
- Kalman kazancı K hesabı: PHᵀ(HPHᵀ + R)⁻¹ → 2×2 S matrisinin analitik tersi (det tabanlı)
- `dt <= 0` guard: init döneminde ilk örnekte bölme hatası önleniyor

> **TODO — Denklem:** F, Q, H matrislerinin tam formu ve Kalman kazancı formülü (LaTeX)
> **TODO — Kod alıntısı:** `alt_kalman.c` — `alt_kalman_update` satır 42–157 (tahmin + güncelleme ayrımı net gösterilecek)

### 3.6. Uçuş Durum Makinesi (7 Faz FSM)

**Ne yazılacak:**
- 7 faz: IDLE → BOOST → COAST → APOGEE → DROGUE_DESCENT → MAIN_DESCENT → LANDED
- Her geçişin koşulları ve eşik değerleri (flight_sm.c'deki #define'lardan):
  - IDLE→BOOST: total_accel > 45 m/s² (IMU var) veya velocity > 15 m/s (baro-only)
  - BOOST→COAST: Kalman dikey ivme < 0 (5 örnekte) veya 8 s timeout
  - COAST→APOGEE: velocity < 0 (5 örnekte, armed ise) veya theta > 70° (acil)
  - APOGEE: drogue tetiklenir, hemen DROGUE_DESCENT'e geçer
  - DROGUE→MAIN: altitude_rel < 300 m
  - MAIN→LANDED: |velocity| < 2 m/s (30 örnekte = 3 s)
- FSM_BIT status word: launched, burnout, armed, drogue, main_alt, main, vel_apogee, tilt_emerg, apogee, landed
- Degraded mode: IMU yoksa baro Kalman hızıyla BOOST tespiti, tilt apogee devre dışı

> **TODO — Şekil:** FSM durum geçiş diyagramı. Her ok üzerinde geçiş koşulu ve eşik değeri yazılacak. Degraded mod ayrı renkle gösterilecek.
> **TODO — Kod alıntısı:** `flight_sm.c` — `flight_sm_update` fonksiyonu (IDLE ve COAST case'leri + hız bazlı apogee bloğu)
> **TODO — Tablo:** Faz → Giriş Koşulu → Çıkış Koşulu → Tetiklenen Eylem tablosu

### 3.7. Güvenilirlik Mekanizmaları

#### 3.7.1. IWDG Watchdog

**Ne yazılacak:**
- Prescaler/256, reload 249 → ~2 s timeout
- baro_task her 100 ms'de besleme yapıyor (`HAL_IWDG_Refresh`)
- Herhangi bir görev 2 s kilitlenirse sistem resetlenir
- Error_Handler artık `NVIC_SystemReset()` çağırıyor (infinite loop yerine)

> **TODO — Kod alıntısı:** `iwdg.c` veya `baro_task.c` içindeki besleme satırı

#### 3.7.2. Stack Overflow Koruması

**Ne yazılacak:**
- `configCHECK_FOR_STACK_OVERFLOW 2`: FreeRTOS her context switch'te stack canary'yi kontrol eder
- `vApplicationStackOverflowHook`: sistemi resetler
- SystemView ile stack high-water mark izleme

#### 3.7.3. IMU Başarısızlığında Baro-Only Degraded Mod

**Ne yazılacak:**
- bmi088_init 3 denemede başarısız olursa imu_task silinir
- `imu_snapshot_peek` → false: flight_sm ve kalman NULL imu ile çalışır
- Hangi özellikler kaybolur, hangisi devam eder (CLAUDE.md degraded mod tablosu)

> **TODO — Tablo:** IMU var / IMU yok karşılaştırma tablosu (CLAUDE.md'den alınacak)

### 3.8. Test ve Doğrulama Metodolojisi

#### 3.8.1. SIT — Sensör İzleme Testi

**Ne yazılacak:**
- Gerçek sensörden canlı veri okuma + UART2 üzerinden Python'a gönderme
- Test kurulumu: STM32 Nucleo + USB-UART + Python GUI (customtkinter)
- IMU statik testleri: Z ekseni ≈ −9.81 m/s², X/Y ≈ 0 → sensör kalibrasyonu
- Barometrik irtifa: Halkalı/Küçükçekmece atmosferik koşullarıyla uyum
- Mahony çıktısı: roll/pitch/yaw ve 3D rocket orientation görselleştirmesi
- SIT'in amacı: sürücü ve iletişim katmanını doğrulama; algoritma doğrulaması değil

> **TODO — Şekil:** SIT Python GUI ekran görüntüsü (sensör verileri + 3D yönelim görselleştirmesi — ilk rapordaki Şekil 5)
> **TODO — Tablo:** SIT test senaryoları ve beklenen/gerçek sonuç tablosu

#### 3.8.2. SUT — Sentetik Uçuş Testi (Processor-in-the-Loop)

**Ne yazılacak:**
- **Temel Felsefe:** PC simüle edilmiş uçuş verisini gönderir → STM32 gerçek algoritmalarını çalıştırır → sonuçları geri gönderir → PC'de görselleştirme
- Gerçek zamanlı pacing yok: her pencere gönder → cevap bekle → sonraki pencere. 51.5 s uçuş < 2 s'de tamamlanıyor
- SUT modunda hangi task'lar uyuyor: IMU, Baro, Telemetri, GNSS → MODE_SUT kontrolü → portMAX_DELAY bekle
- sut_task iç döngüsü: paket al → Mahony × N batch → Kalman → flight_sm → 26 byte cevap gönder
- SUT_COMBINED paketi (PC→STM32): 0xAD header, IMU batch (count × [sim_t+gx+gy+gz]), baro veri
- SUT_RESPONSE paketi (STM32→PC): 0xAE header, sim_time, filtered_alt, roll, pitch, yaw, flight_status, checksum
- Python tarafı: PyQt5 + PyQtGraph, tek QThread (kilit yok), progress bar, faz geçiş çizgileri

> **TODO — Şekil:** SUT sistem diyagramı: RocketPy CSV → Python SerialWorker → UART → STM32 sut_task → UART → Python PlotWidget
> **TODO — Kod alıntısı:** `SUT.md` içindeki sut_task pseudo-code (bölüm 3.2 sut_task iç döngüsü)
> **TODO — Tablo:** SUT_COMBINED ve SUT_RESPONSE paket formatları (offset, boyut, içerik)

#### 3.8.3. RocketPy Simülasyon Verisi ve Test Altyapısı

**Ne yazılacak:**
- RocketPy: Python tabanlı açık kaynak roket uçuş simülatörü
- Simülasyon parametreleri: motor tipi, kütleler, Cd, roket boyutları
- Üretilen CSV yapısı: sim_time, ax, ay, az, gx, gy, gz, altitude, pressure sütunları
- 51.5 saniyelik uçuş profili: boost, coast, apogee, descent
- Neden RocketPy: gerçek uçuş verisi olmadan algoritma doğrulamanın tek yolu

> **TODO — Şekil:** RocketPy simülasyonundan alınan irtifa-zaman ve ivme-zaman grafikleri
> **TODO — Tablo:** Simülasyon parametreleri (motor, roket boyutları, hava koşulları)

---

## 4. BÖLÜM IV — BULGULAR (YAPILDI - Alt başlık 4.2, 4.3, 4.4 tamamlandı. 4.1 bekleniyor.)

### 4.1. Gerçek Zamanlılık ve Görev Performansı (SystemView)

**Ne yazılacak:**
- SEGGER SystemView Post-Mortem modu: JTAG olmadan RAM ring buffer → GDB dump → desktop analizi
- 100 ms kesitteki görev aktivasyon sayıları ve toplam çalışma süreleri
- Tablo: bmiTask → 331 aktivasyon, %2.39 CPU; bmeTask → 18 aktv, %0.47; gnssTask → %0.06; Idle → %96.55
- Bu sonuçların anlamı: Bitirme II kapsamındaki tüm algoritmalar eklenmiş halde bile işlemci büyük ölçüde boşta — mimarinin doğruluğunun kanıtı
- Preemption gözlemi: bmiTask aktif olduğunda diğer taskların beklediği timeline'da görülüyor

> **TODO — Şekil:** SystemView Timeline ekran görüntüsü (ilk rapordaki Şekil 4 — mevcut, ama açıklama derinleştirilecek)
> **TODO — Tablo:** Görev → Aktivasyon → Total Run Time → CPU Load tablosu (SystemView verilerinden)

### 4.2. IMU Sensör Füzyonu Sonuçları

**Ne yazılacak:**
- Durağan konumda ölçümler: az ≈ −9.69g, ax/ay ≈ 0 → sensör düzgün çalışıyor
- Mahony çıktısı: roll ≈ 0.4°, pitch ≈ 1.9°, yaw ≈ −0.1° (statik test)
- Zaman içinde kararlılık: birkaç dakika boyunca drift gözlemlenmedi (Ki=0 nedeniyle bias düzeltme yok, ancak kabul edilebilir)
- Gyro kalibrasyonu yapılmadığından küçük statik sapmanın kaynağı

> **TODO — Şekil:** Mahony roll/pitch/yaw zaman serisi grafiği (SIT Python GUI'den ekran görüntüsü veya UART log)
> **TODO — Şekil:** 3D rocket orientation görselleştirmesi (ilk rapordaki Şekil 5)

### 4.3. Yükseklik Kalman Filtresi Sonuçları

**Ne yazılacak:**
- Kalman çıktısının ham baroya göre daha pürüzsüz olduğu SUT verisinden gösterme
- r_alt ve r_acc parametrelerinin etkisi: r_acc büyük → baroya daha çok güven
- SUT modunda ivme kanalı susturulmuş (r_acc=5000), saf baro Kalman olarak çalışıyor

> **TODO — Grafik:** Ham baro irtifa vs. Kalman filtered irtifa karşılaştırma grafiği (SUT Python PlotWidget çıktısından)
> **TODO — Grafik:** Kalman hız tahmini zaman serisi (boost → coast → iniş)

### 4.4. SUT Doğrulama Sonuçları

**Ne yazılacak (üç alt konu):**

**Faz geçiş doğruluğu:**
- RocketPy simülasyonundaki beklenen faz zamanları vs. STM32 flight_sm'in tespit ettiği zamanlar
- IDLE→BOOST: simülasyondaki fırlatma anı ile STM32 tespiti arasındaki fark (ms cinsinden)
- COAST→APOGEE: hız-bazlı tespit zamanı
- DROGUE→MAIN: 300 m irtifa eşiği geçiş zamanı
- MAIN→LANDED: 3 s kararlı iniş koşulu

> **TODO — Tablo:** Faz → Beklenen Zaman (RocketPy) → STM32 Tespiti → Fark tablosu
> **TODO — Grafik:** SUT Python GUI faz geçiş grafiği (irtifa + faz geçiş dikey çizgileri)

**Kalman yakınsama:**
- Apogee irtifasının ham baro ile uyumu (±%5 hedef)
- Boost fazında ivme gürültüsünün filtreye etkisi
- Descent'te hız tahmininin kararlılığı

**Mahony kararlılığı:**
- SUT modunda gyro-only (ivme = 0), yaw drift gözlemi
- Roll/pitch tahmininin RocketPy verisindeki gerçek açılarla karşılaştırması

> **TODO — Grafik:** Roll, Pitch, Yaw zaman serisi (SUT çıktısından)

---

## 5. BÖLÜM V — TARTIŞMA VE SONUÇLAR

### 5.1. Tartışma

**Ne yazılacak:**
- Elde edilen bulgular literatürdeki benzer çalışmalarla karşılaştırma
- FreeRTOS'un bare-metal alternatiflere üstünlüğünün bu projede doğrulanması: %96.55 idle → gerçek zamanlılık sağlandı, hiçbir görev deadline kaçırmadı
- DMA tabanlı sensör okumanın CPU yükü üzerindeki etkisi: bmiTask 331 aktivasyon ama sadece %2.39
- PIL test metodolojisinin değeri: gerçek uçuş olmadan fiziksel olarak anlam ifade eden bir doğrulama
- Sınırlılıklar:
  - Gyro kalibrasyonu yapılmadı → Mahony'de küçük statik offset
  - SUT'ta ivme kanalı susturuldu → gerçek boost fazında Kalman daha gürültülü olabilir
  - Gerçek uçuş testi yapılmadı

### 5.2. Sonuçlar

**Ne yazılacak:**
- Modüler FreeRTOS mimarisinin 180 MHz Cortex-M4 üzerinde hard real-time kısıtları karşıladığı gösterildi
- BMI088 + Mahony + BME280 + Kalman zinciri çalışıyor, statik testlerde beklenen değerler elde edildi
- 7-faz FSM SUT testinde RocketPy simülasyonu ile uyumlu faz geçişleri üretti
- Mimari; GNSS, telemetri, IWDG güvenilirlik mekanizmaları ile uçuşa hazır altyapı sunuyor
- İşlemci kapasite fazlası (%96.55 idle) gelecekte LoRa, SD kart, daha gelişmiş füzyon için alan bırakıyor

### 5.3. Öneriler

**Ne yazılacak:**
- Gyro offset kalibrasyonu: boot'ta N örnek ortalaması, Mahony init'e bias çıkarma
- Gerçek uçuş testi: düşük irtifadan başlayarak aşamalı
- LoRa (E22) entegrasyonu: UART4 DMA TX hazır, sürücü yazılacak
- SD kart kaydı: flight log binary format
- Mahony yerine EKF: daha yüksek dinamik doğruluk, ancak daha yüksek hesaplama maliyeti değerlendirmesi
- HIL (Hardware-in-the-Loop): gerçek roket donanımıyla entegre test

---

## KAYNAKÇA

**Mevcut kaynaklar (ilk rapordan devralınan, güncellenecek):**
- [1] Liu & Layland, 1973 — scheduling algoritmaları
- [2]–[5] FreeRTOS resmi dokümantasyonu
- [6] Stanford IMU notları
- [7] AHRS complementary filter
- [8] Madgwick, 2011 — gradient descent füzyon
- [9] Welch & Bishop — Kalman filter
- [10] AHRS EKF dokümantasyonu
- [11] Bosch Sensortec BMI088 datasheet
- [12] Bosch Sensortec BME280 datasheet

**Eklenecek kaynaklar:**
- [ ] RocketPy GitHub/paper referansı
- [ ] SEGGER SystemView dokümantasyonu
- [ ] FreeRTOS Stack Overflow detection referansı

---

## EK NOTLAR — Yapay Zekaya Rehberlik

> Bu notlar raporu yazdıran yapay zeka için direktiftir.

- **Dil:** Türkçe. Teknik terimler (Quaternion, DMA, ISR, deadlock vb.) Türkçe açıklamasıyla birlikte kullanılabilir.
- **Kod alıntıları:** LaTeX `lstlisting` ortamında, C dili renklendirmeli. Türkçe karakter yok.
- **Denklemler:** `equation` ortamında, numaralı.
- **Şekiller:** `figure` + `\caption` + `\label` formatında. Her şekle metin içinde atıf yapılacak.
- **Tablolar:** `table` + `tabular` + `\caption` + `\label`. Başlık tablo üstünde.
- **Kaynak atıfları:** `\cite{key}` formatında, her iddia kaynaklı.
- **Her bölümde birinci rapordaki ilgili içerikten faydalanılabilir** ama direkt kopyalama değil, geliştirilmiş versiyon yazılacak.
- **Şekil sayıları:** Şablon sıralı numara istiyor. Raporun tamamında tutarlı şekil/tablo sırası koru.
