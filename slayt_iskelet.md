---
marp: true
theme: default
paginate: true
---

# SKYRTOS
## STM32F446 Üzerinde FreeRTOS Tabanlı Model Roket Uçuş Bilgisayarı ve Processor-in-the-Loop Doğrulama Platformu

* **Hazırlayan:** Halil Sarıkaya
* **Danışman:** [Danışman Adı]
* **Kurum:** İstanbul Sabahattin Zaim Üniversitesi, Bilgisayar Mühendisliği Bölümü

---

## Neden Gerçek Zamanlı Bir Sistem Gereklidir?

* **Apogee Tespiti ≤ 100 ms:** Tepe noktasında 100 ms'lik bir gecikme, 50–100 m/s inen bir rokette 5–10 metrelik yükseklik hatasına dönüşür.
* **Bare-Metal Problemi:** Klasik `while(1)` döngüsünde L86 GNSS gibi yavaş bir aygıt (9600 bps) tüm CPU'yu bloklar; IMU ve baro ölçümleri beklemeye girer.
* **Çözüm:** Donanım kesmelerine (EXTI + DMA) dayalı, preemptive öncelikli FreeRTOS task mimarisi — her sensör kendi görev bağlamında, CPU boşa harcanmadan çalışır.

<!-- note:
Roket uygulamalarında zaman kritikliğini somutlaştırmak gerekiyor. 100 ms, bir insan için göz kırpma süresi gibi görünebilir ama 50-100 m/s inen bir rokette bu, 5-10 metrelik apogee hatasına dönüşür. Bare-metal döngüde bir sensörü beklemek diğer sensörlerin zamanında okunamamasına yol açar.
-->

---

## Donanım Altyapısı

| Peripheral | Pin | Görev |
| ---------- | --- | ----- |
| ARM Cortex-M4 @ 168 MHz + FPU | — | Ana işlemci |
| I2C1 Fast Mode | PB8/PB9 | BMI088 IMU — EXTI PB3/PB4 |
| I2C3 Standard | PA8/PC9 | BME280 Barometre |
| USART6 Circular DMA | — | L86 GNSS |
| USART2 DMA TX/RX | — | Telemetri / SUT |

![w:650 center](Report%20Latex/sekiller/STM32F446DonanımBlokDiyagramı.png)

<!-- note:
Donanım seçiminde iki temel kriter: FPU'su olan Cortex-M4 (Mahony ve Kalman için floating-point) ve yeterli peripheral sayısı. STM32F446 NUCLEO bu ikisini de karşıladı. I2C1 ve I2C3'ün ayrı fiziksel bus'larda olması, BMI088 ve BME280'in birbirini bloklamadan eşzamanlı DMA okuyabilmesini sağlıyor.
-->

---

## Yazılım Mimarisi — 4 Katman

* **Donanım:** STM32F446, BMI088, BME280, L86 GNSS
* **HAL/BSP:** CubeMX üretimi `i2c.c`, `dma.c`, `usart.c` — dokunulmaz
* **Sürücü/RTOS:** `bmi088.c`, `bme280.c`, `mahony.c`, `alt_kalman.c`, FreeRTOS middleware
* **Uygulama:** `imu_task`, `baro_task`, `gnss_task`, `telemetry_task`, `flight_sm`

![w:700 center](Report%20Latex/sekiller/KatmanlıYazılımMimarisi(4%20katman).png)

<!-- note:
Katmanlı mimari bakım kolaylığı sağlar. HAL katmanı dokunulmaz tutulursa üst katmanları başka bir STM32 modeline taşımak yalnızca sürücü katmanındaki ince uyarlamalar gerektirir. CubeMX üretilen dosyalara "dokunma" kuralı, CubeMX yeniden çalıştırıldığında bizim değişikliklerimizin kaybolmamasını garantiler.
-->

---

## FreeRTOS: Lock-free Veri İletişimi

**ISR → Task:** `EXTI` kesmesi → `xTaskNotifyFromISR` → görev uyanır — CPU asla verinin hazır olmasını beklemez

**Task → Task (snapshot):** Derinlik-1 kuyruk + `xQueueOverwrite` / `xQueuePeek`
* Üretici her zaman yazar (eski değerin üzerine yazar)
* Tüketici bloke olmadan en güncel değeri okur
* Mutex veya priority inversion riski sıfır

![w:700 center](Report%20Latex/sekiller/FreeRTOSGörevMimarisi.png)

<!-- note:
Bu iki kalıbın birleşimi projenin omurgasıdır. ISR'lar sadece bir notify gönderir, başka iş yapmaz. Snapshot kuyruğu sayesinde hiçbir görev diğerinin verisini beklemez. Depth=1 queue ile xQueueOverwrite: tüketici en kötü ihtimalle bir döngü önceki veriyi okur — bu kabul edilebilir bir stale-ness, hız kazancı çok daha büyük.
-->

---

## Asenkron Sensör Okuma — Sıfır CPU Bloklaması

* **BMI088 DMA Durum Makinesi:** `DMA_IDLE → DMA_READING_ACC → DMA_READING_GYRO` — ivme ve jiroskop sıralı, non-blocking I2C DMA aktarımlarıyla okunur
* **BME280:** `vTaskDelayUntil` ile tam 100 ms periyot garantisi; boot'ta ilk N ölçüm ortalamasıyla `alt_ref` zemin kalibrasyonu
* **L86 GNSS:** Circular DMA RX — CPU müdahalesi olmadan NMEA tamponu dolar; `sscanf` ile GPRMC/GPGGA ayrıştırılır; fix yoksa `is_valid=false`

![w:700 center](Report%20Latex/sekiller/IMU_Pipeline.png)

<!-- note:
BMI088 sürücüsünde kritik bir tasarım detayı var: iki sensörün (ivme + jiroskop) farklı I2C adresleri var ama aynı DMA kanalını paylaşıyorlar. Durum makinesi bu koordinasyonu sağlar: biri bitince diğeri başlar, CPU tek döngü beklemez. GNSS içinse circular DMA, CPU uyanmadan tamponu doldurmaya devam eder; task sadece yeni satır geldiğinde parse eder.
-->

---

## Mahony AHRS — Gimbal Lock'suz 3D Yönelim

* **Quaternion temsili:** Euler açılarının 90°'de yaşadığı Gimbal Lock'u matematiksel olarak ortadan kaldırır
* **Algoritma:** $\mathbf{e} = \hat{\mathbf{g}} \times \mathbf{a}_{norm}$ hata vektörü → PI kontrolcü ile jiroskop drift düzeltmesi → quaternion güncelleme → roll/pitch/yaw çıkarımı
* **Optimizasyon:** Quake III `inv_sqrt` hızlı ters karekök — ARM FPU mevcut olsa da normalize adımlarını ~3× hızlandırır

![w:700 center](Report%20Latex/sekiller/IMU_300_saniye.png)

<!-- note:
Mahony filtresinin drift düzeltme mekanizması: ivmeölçerden hesaplanan yerçekimi yönü ile quaternion tahmini arasındaki açısal hatayı PI kontrolcüye geri besler. Sonuç: jiroskop drifti baskılanmış, kısa dönem gürültü yumuşatılmış. 300 saniye boyunca sıfır derece kayma görüntüde görülüyor.
-->

---

## 3-Durumlu Baro-IMU Yükseklik Kalman Filtresi

* **Durum vektörü:** $\mathbf{x} = [\text{irtifa},\ \text{hız},\ \text{ivme\_bias}]^T$ — yalnızca irtifayı değil, dikey hızı ve ivme sapmasını da kestirmek
* **Füzyon:** $F$ sabit ivmeli hareket modeli; $H$ barometreyi (irtifa) + IMU'yu (ivme) birleştirir; $r_{alt}=5.0$, $r_{acc}=10.0$
* **Sağlamlık:** $dt \leq 0$ guard → boot sırasında bölme hatası yok; SUT modunda $r_{acc}=5000$ → saf baro Kalman

![w:700 center](Report%20Latex/sekiller/KalmanFiltresi.png)

<!-- note:
Bu filtrenin tasarım felsefesi şudur: barometre gürültülü ama yavaş; ivmeölçer hızlı ama bias'lı. İkisini birleştirince tepe noktasındaki kritik dikey hız kestirimi hem anlık hem güvenilir olur. r_acc'yi yükselterek ivme kanalını susturabiliyorum — SUT modunda bilerek yapılan bir şey, çünkü simülasyonda gerçek ivme verisi beslenmez.
-->

---

## Uçuş Durum Makinesi — 7 Faz

* **Faz zinciri:** IDLE → BOOST → COAST → APOGEE → DROGUE_DESCENT → MAIN_DESCENT → LANDED
* **Çift kanallı apogee:** Kalman dikey hızı < 0 (5 örnekte = 500 ms) **VE/VEYA** Mahony tilt açısı > 70° — tek sensör hatasına karşı bağımsız ikinci kanal
* **Degraded mod:** BMI088 arızalanırsa FSM baro Kalman hızı (> 15 m/s) ile BOOST tespiti yapar; tilt apogee devre dışı, hız apogee aktif

![w:700 center](Report%20Latex/sekiller/UçuşDurumMakinesi.png)

<!-- note:
Çift güvenlik tasarımı kritik: hız kanalı önce devreye girer. Roket 70 dereceyi aştıysa (takla atmış demektir) tilt apogee acil tetikleme yapar. Bu, motor arızası veya aşırı rüzgarda kurtarma sistemini tetikler. İki bağımsız kanal = iki bağımsız arıza toleransı. Degraded mod ise IMU arızasında sistemin tamamen çökmesini engeller.
-->

---

## Güvenilirlik — Donanım ve Yazılım Güvenlik Katmanları

* **IWDG (Bağımsız Watchdog):** Prescaler/256, reload 249 → ~2 s timeout; `baro_task` 100 ms'de bir besler — herhangi bir görev kilitlenirse donanım sistemi sıfırlar
* **Stack Overflow Koruması:** `configCHECK_FOR_STACK_OVERFLOW 2` her context switch'te RAM canary kontrolü; taşmada `vApplicationStackOverflowHook` → `NVIC_SystemReset()`
* **Error_Handler:** Artık sonsuz döngüde kalmıyor — `NVIC_SystemReset()` ile rokete kurtarma dönemine girme şansı veriliyor

![w:600 center](Report%20Latex/sekiller/TaskNotificationKalıbı.png)

<!-- note:
Watchdog'u baro_task'ın beslemesi bilinçli bir seçim: baro_task besleyebiliyorsa FreeRTOS scheduler çalışıyordur. Scheduler çalışıyorsa yüksek öncelikli IMU görevi de preemptive olarak çalışıyordur. IMU görevi kilitlenip scheduler'ı tıkarsa baro_task çalışamaz, Watchdog beslenmez, reset gerçekleşir. Bu sayede tüm görev hiyerarşisi tek Watchdog noktasından izleniyor.
-->

---

## Test Metodolojisi — SIT + SUT

* **SIT (Sensör İzleme):** Gerçek donanımdan 50 Hz canlı telemetri → Python GUI 3D yönelim görselleştirmesi; sürücü ve iletişim katmanını doğrular
* **SUT / PIL (Processor-in-the-Loop):** RocketPy CSV → UART → `sut_task` → gerçek Mahony + Kalman + FSM → 26-byte `SUT_RESPONSE`; **51.5 s uçuşu < 2 s'de test eder**
* **7 Gürültü Senaryosu:** Az/çok irtifa gürültüsü, az/çok ivme gürültüsü, altitude zip, nadir anomali — Kalman ve FSM dayanıklılığı stres-test edildi

![w:700 center](Report%20Latex/sekiller/SUT_SistemMimarisi.png)

<!-- note:
SIT ile 'kart doğru çalışıyor mu?' sorusunu yanıtladım; SUT ile 'algoritmalar gerçek uçuşta doğru karar veriyor mu?' sorusunu yanıtladım. Bunu fiziksel kart üzerinde, gerçek algoritmalarla, tek satır kod değiştirmeden yaptım. Processor-in-the-Loop'un özü budur: gerçek donanım, gerçek kod, simüle edilmiş ortam.
-->

---

## Bulgular — SystemView CPU Yük Analizi

* Toplam kayıt: **~984 ms** — event atlanması veya buffer taşması: **0**
* `bmiTask`: **%2.39 CPU** (331 aktivasyon / 100 ms)
* `bmeTask`: **%0.47 CPU** | `gnssTask`: **%0.06 CPU**
* **System Idle: %96.55** — Mahony + Kalman + FSM tüm hesabına rağmen işlemci büyük oranda boşta

![w:500 center](Report%20Latex/sekiller/Context.png)

<!-- note:
%96.55 Idle oranı gelecekteki genişleme için muazzam alan bırakıyor: LoRa telemetri, SD kart loglama, EKF filtresi — bunlar eklenebilir, hâlâ real-time kalınabilir. Bu rakamlar SEGGER SystemView post-mortem kaydından alınmıştır; tahmin değil, ölçüm.
-->

---

## Bulgular — Mikrosaniye Preemption Kanıtı

* Yüksek öncelikli `bmiTask`, çalışan `bmeTask`'ı durdurarak (preempt) acil hesabı tamamlayıp görevi iade etti
* Bu davranış SEGGER SystemView **donanımsal trace olaylarıyla** mikrosaniye çözünürlükte kanıtlandı
* FreeRTOS preemptive önceliklendirme: teorik değil, ölçülmüş

![w:600 center](Report%20Latex/sekiller/event_list_imu_baro.png)

<!-- note:
Bu slayt projenin deterministik gerçek zamanlılık iddiasının somut kanıtıdır. Preemption'ın gerçekleştiğini, tam süresini ve görevi kimin yaptığını trace loglarından okuyabiliyorum. Teoride "yüksek öncelikli görev öne geçer" diyebiliriz ama bunu donanım düzeyinde kanıtlamak çok farklı bir şey.
-->

---

## SUT Uçuş Simülasyonu Sonuçları

* **BOOST** fazı doğru tespit edildi (baro Kalman hızı > 15 m/s eşiği) ✅
* **APOGEE** doğru anda tespit edildi; dikey hız sıfırına Kalman füzyonuyla onaylandı ✅
* **DROGUE** ve **MAIN** paraşüt faz geçişleri irtifa eşiklerinde başarılı ✅
* Ana paraşüt fazında süzülme hızı: ~2 m/s (baro referans ölçümü)

![w:800 center](Report%20Latex/sekiller/SUT_testi_arayüz.png)

<!-- note:
Tüm 7 faz geçişi 7 farklı gürültü senaryosunda doğrulandı. En kritik sonuç: degraded modda (IMU olmadan) dahi FSM baro Kalman hızıyla BOOST tespiti yapabildi. 51.5 saniyelik bir uçuş simülasyonu 2 saniyeden kısa sürede tamamlandı — PIL'in hızını bu sayede görüyoruz.
-->

---

## Sonuçlar ve Metrikler

| Metrik | Değer | Kaynak |
|--------|-------|--------|
| `bmiTask` CPU | **%2.39** | SystemView post-mortem |
| `bmeTask` CPU | **%0.47** | SystemView post-mortem |
| System Idle | **%96.55** | SystemView post-mortem |
| Flash | **~75 KB / 512 KB (%14)** | cmake build çıktısı |
| RAM | **~66 KB / 128 KB (%50)** | cmake build çıktısı |
| SUT test süresi | **51.5 s uçuş < 2 s** | SUT test çıktısı |
| Kalman apogee toleransı | **±%5** | 7 senaryo ortalama |

**Gelecek çalışmalar:** Gyro kalibrasyonu · LoRa E22 (UART4 hattı hazır) · HIL piroteknik yük testi

<!-- note:
Metriklerin tamamı SEGGER SystemView post-mortem kaydı ve SUT test çıktılarından alınmıştır. Özellikle %96.55 Idle oranı, ilerleyen çalışmalarda sisteme eklenecek modüller için yeterli hesaplama kapasitesi bulunduğunu gösteriyor.
-->

---

# Teşekkürler

**SKYRTOS** — STM32F446 Üzerinde FreeRTOS Tabanlı Model Roket Uçuş Bilgisayarı

*Sorularınızı bekliyorum.*

---

---

# Ek Slaytlar — Olası Jüri Sorularına Hazırlık

---

## SORU 1 — Neden Mahony? Madgwick veya EKF daha doğru olmaz mıydı?

> *"Madgwick gradient descent, Mahony PI kontrolcü kullanır. Model roket dinamiklerinde PI kontrolcü tutarlı ve ayarlanabilir. EKF teorik üstünlük sunar ama Jacobian + kovaryans güncellemesi CPU yükünü 5–10× artırır. `bmiTask` zaten %2.39 CPU; EKF ile bu %20'yi geçebilir. Mahony bu proje için 'yeterince iyi ve hızlı' olan optimal seçimdir."*

---

## SORU 2 — SUT Gerçek Uçuşu Ne Kadar Doğru Temsil Ediyor?

> *"SUT'un amacı fiziksel uçuşu değil, algoritmaları belirli veri profilleri karşısında test etmektir. Bu Processor-in-the-Loop'un felsefesidir. RocketPy atmosferik model üzerinden hesaplar; gerçek türbülans, titreşim, aeroelastik etkiler tam temsil edilemez. Bu nedenle 7 farklı gürültü senaryosu tasarladım. Gerçek uçuş verisi olmadan PIL en sağlam alternatiftir."*

---

## SORU 3 — Mutex Yerine Neden Depth-1 Queue Tercih Ettiniz?

> *"Mutex tabanlı paylaşımda priority inversion riski var; FreeRTOS priority inheritance ek yük getirir. Depth-1 + `xQueueOverwrite`: üretici her zaman yazar, tüketici en kötü ihtimalle bir döngü önceki veriyi okur — kabul edilebilir bir stale-ness. Hiçbir görev diğerini bloke etmez, öncelik tersimi yaşanmaz. FreeRTOS'un atomic queue operasyonları veri tutarlılığını garanti eder."*

---

## SORU 4 — Watchdog'u baro_task Beslerse IMU Kilidi Gözden Kaçmaz mı?

> *"Hayır; bilinçli bir tasarım. baro_task besleyebiliyorsa FreeRTOS scheduler çalışıyordur. Scheduler çalışıyorsa yüksek öncelikli IMU görevi de preemptive olarak çalışıyordur. IMU görevi kilitlenip scheduler'ı tıkarsa baro_task çalışamaz, Watchdog beslenmez, reset olur. baro_task seçilmesinin nedeni: tüm görev hiyerarşisini tek Watchdog noktasından izlemek."*

---

## SORU 5 — Gerçek Uçuş İçin En Kritik Eksik Nedir?

> *"Üç eksik var. (1) Gyro offset kalibrasyonu — boot'ta N örnek bias çıkarma, birkaç saatlik iş. (2) LoRa E22 — UART4 DMA hattı donanımda hazır, sürücü henüz yazılmadı. (3) Gerçek ortam EMC testi — motor plumu, titreşim, RF paraziti laboratuvarda simüle edilemiyor. Mevcut PIL altyapısı bu testlere geçiş için güçlü bir temel oluşturuyor."*

---

> **Sunum Günü Checklist**
>
> - [ ] Nucleo şarjlı, ST-Link sürücüsü kurulu
> - [ ] `sut_tool/main.py` test edildi, COM port seçilebiliyor
> - [ ] En az 1 SUT senaryosu CSV önceden yüklendi
> - [ ] SystemView timeline ekran görüntüsü hazır
> - [ ] FLASH/RAM boyutları `cmake --build` çıktısından not alındı
> - [ ] Yedek USB kablo hazır
