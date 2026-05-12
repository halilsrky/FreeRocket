# SKYRTOS — STM32F446 NUCLEO Flight Computer

Halil'in bitirme projesi. BMI088 IMU + BME280 baro + GNSS + LoRa  içeren roket flight computer yazılımı.

**Karar değişti:** Kendi mini RTOS kernel'ımızı yazmayacağız.

* Neden: stack overflow, hard fault, race condition, scheduler edge-case ve ISR/priority hataları proje hızını ve güvenilirliğini bozar.
* Hedef: **STM32CubeIDE ile eklenmiş FreeRTOS tabanlı, temiz, okunabilir, adım adım ilerleyen bir mimari**.
* Bu proje artık “kernel yazma” projesi değil; **uçuş bilgisayarı mimarisi ve güvenilirlik projesi**.

## Klasör yapısı

```
FreeRtos_project/       ← aktif proje (STM32CubeIDE .ioc + HAL + FreeRTOS)
├── OLD_Project/        ← SKYRTOS arşivi — referans, driver ve algoritma portu için
├── Report.md           ← bitirme raporu için teknik notlar
└── CLAUDE.md           ← bu dosya
```

**`old_project/`**** asla silinmeyecek.** Buradan algoritma ve sürücü mantığı port edilecek; doğrudan kopyala-yapıştır değil, okuyup sadeleştirerek uyarlanacak.

**`Report.md`** — bitirme raporu için ham teknik notlar deposu. Sadece rapor değeri olan olaylar yazılır: kök neden, teşhis, çözüm, tekrar etmemesi için alınan önlem. Sıradan refactor notları buraya girmez. Bu dosya bitirme projesi raporunda kullanılacak teknik notları, karşılaşılan zorlukları ve çözümleri toplar kısa kısa notlarla madde madde sıralar.

## Aktif proje (`FreeRtos_project/`)

Aktif proje içinde şu an **sadece CubeMX ile üretilen peripheral init dosyaları** ve **FreeRTOS middleware** var. Sensör driver'ları henüz eklenmiş değil; önce sağlam iskelet kurulacak, sonra tek tek driver eklenecek.

| Dosya                                                   | Görev                                    | Not                                        |
| ------------------------------------------------------- | ---------------------------------------- | ------------------------------------------ |
| `Core/Src/main.c`                                       | HAL init + `Application_Start()` çağrısı | Sadece USER CODE bloğuna tek giriş noktası |
|                                                         |                                          |                                            |
| `Core/Src/stm32f4xx_it.c`                               | IRQ vektörleri                           | Cube üretir                                |
| `Core/Src/gpio.c`, `i2c.c`, `dma.c`, `usart.c`, `tim.c` | Peripheral init                          | Cube üretir; mümkün olduğunca dokunma      |
| `Middlewares/Third_Party/FreeRTOS/`                     | FreeRTOS middleware                      | Hazır kullanılır                           |

### Şu an kapsam dışı

* BMI088 sürücüsü yok
* BME280 sürücüsü yok
* GNSS yok
* LoRa telemetry yok
* Flight algorithm yok
* Kalman / Mahony henüz port edilmemiş olabilir

Önce proje iskeleti, task modeli ve temiz giriş noktası kurulacak. Sensörler daha sonra eklenir.

## Mimari kararlar

* **FreeRTOS kullanılıyor.** Kendi kernel'ımız yok.
* **AO / QP / ikinci bir scheduler yok.** Tek yürütme modeli FreeRTOS.
* **Mimari tam event-driven olmak zorunda değil.** Bu projede öncelik deterministik ve okunabilir FreeRTOS akışı. Gerekli yerlerde event/flag kullanılabilir ama her şeyi event-driven yapmaya zorlamayacağız.
* **Blocking çağrılar minimumda tutulacak.** `HAL_Delay`, polling loop ve sonsuz beklemeler yok. Bu aşamada ADC/SD olmadığı için `f_write` gibi başlıklar da kapsam dışı.
* **ISR içinde iş minimum olacak.** ISR sadece event set eder, DMA başlatır veya kısa state güncellemesi yapar.
* **Watchdog (IWDG) şart.** Flight computer reset alabilmeli, kilitlenmemeli.
* **Veri sahipliği tek kaynaklı olacak.** Aynı sensör verisi birden fazla task tarafından farklı snapshot olarak tüketilmeyecek.
* **Synchronization primitive kullanımı kontrollü olacak:**

  * ISR → task iletişimi için öncelikli mekanizma `task notification`
  * Queue yalnızca event, command veya immutable snapshot taşımak için kullanılacak
  * Aynı sensör verisi birden fazla task tarafından ayrı snapshot olarak tüketilmeyecek
  * Mutex yalnızca gerçekten kaçınılmaz paylaşılan kaynak varsa kullanılacak
  * Semaphore kullanımı minimum tutulacak; notification tercih edilecek
* **Mode geçişleri explicit state machine ile yönetilecek.** Gizli global flag karmaşası olmayacak.
* old_project/``** referans olarak kullanılacak.** Oradaki gerekli driver ve mantıklar port edilecek; eski mimari aynen taşınmayacak.


## Şu anki kritik hatalar

Bu başlık, old_project'teki SKYRTOS döneminden gelen hataların nedenlerinden oluşur. Yeni projede bunlar tekrar edilmeyecek.

### Kritik

* **Blocking boot akışı:** sensör init sırasında uzun `HAL_Delay`, timeout loop ve kalibrasyon bloklaması boot'u gereksiz uzatıyor.
* **ISR içinde fazla iş:** EXTI callback içinde sadece işaretleme değil, transfer başlatma ve ek mantık var.
* **Veri tutarsızlığı:** accel ve gyro ayrı anlarda işlenip tek sample gibi publish ediliyor.
* **Fatal handler:** `Error_Handler()` uçuşta sistemi sonsuz döngüde bırakmamalı.
* **Stack güvenliği yok:** stack overflow tespiti ve heap hatası kontrolü yoksa sessiz çöküş olur.
* **Mode karmaşası:** mode değişimi state machine yerine dağınık if blokları ile yönetilirse reset/re-init bug'ları doğar.

### Orta önem

* **Blocking UART / blocking polling** task jitter üretir.
* **Global değişkenlerle paylaşılan state** race condition üretir.
* **Boot sırasında interrupt açmak** initialization sırası problemleri yaratır.

### Sonuç

Bu hatalar gösterdi ki sorun “RTOS kullanmak” değil; **mimarinin dağınık olması**. Yeni projede bu yüzden kernel yazma yoluna gidilmeyecek.

## Kod yazma kuralları

* **Adım adım ilerle.** Tek seferde büyük refactor yok.
* **Önce çalışan minimal iskelet**, sonra tek sensör, sonra bir sonraki modül.
* **Her adım doğrulanmadan bir sonrakine geçme.**
* **Önce tek veri akışı, tek task.** Sonra genişlet.
* **Tek sahipli modül yaklaşımı kullan.** IMU, telemetry veya fusion state'i birden fazla task tarafından doğrudan değiştirilmeyecek.
* **Her yeni modül için önce arayüz, sonra implementasyon.**
* **`old_project/`**** gerekli yerlerde referans alınabilir.** Aynı algoritmanın sadeleştirilmiş ve temiz versiyonu hedeflenir.
* **Comment sadece gerekli yerde.** Özellikle görünmeyen neden varsa yaz; bariz şeyi tekrar etme.
* **Identifier'lar İngilizce kalacak.** Açıklama Türkçe olabilir.

## Donanım haritası

| Peripheral | Pin              | Görev            |
| ---------- | ---------------- | ---------------- |
| I2C1       | PB8 SCL, PB9 SDA | BMI088           |
| I2C3       | PA8 SCL, PC9 SDA | BME280           |
| EXTI3      | PB3              | BMI088 ACC DRDY  |
| EXTI4      | PB4              | BMI088 GYRO DRDY |
| USART2     | DMA              | Telemetry        |
|            |                  |                  |

## Başlangıç akışı

Bu aşamada hedef **event-driven mimariyi zorlamak değil**, deterministik ve temiz bir RTOS iskeleti kurmaktır.

* Main thread yalnızca `Application_Start()` çağıracak.
* Callback'ler minimum iş yapacak.
* İlk doğrulama hedefi: sistemin temiz açılması, task'ların çalışması ve sonraki modül için güvenli altyapının hazır olması.

## Build

```powershell
# FreeRtos_project/ klasöründen
cmake --preset Debug
cmake --build build/Debug
```

Çıktı: `FreeRtos_project/build/Debug/*.elf` + `.hex` + `.bin`

## old_project'ten port edilecek modüller

Yol: `old_project/Core/Src/`

| Dosya                | İçerik                   | Durum                          |
| -------------------- | ------------------------ | ------------------------------ |
| `bmi088.c`           | BMI088 driver            | düzenlenecek ve port edilecek  |
| `queternion.c`       | Mahony quaternion update | `mahony.c` içine port edilecek |
| `kalman.c`           | Altitude Kalman filter   | Sonraki aşama                  |
| `bme280.c`           | BME280 driver            | Sonraki aşama                  |
| `flight_algorithm.c` | Flight state machine     | Sonraki aşama                  |
| `sensor_fusion.c`    | Fusion mantığı           | Sonraki aşama                  |
| `e22_lib.c`          | LoRa telemetry           | Sonraki aşama                  |
| `l86_gnss.c`         | GNSS                     | Sonraki aşama                  |
| `data_logger.c`      | SD write buffering       | Sd kart bu projede yok         |
| `uart_handler.c`     | UART komut ayrıştırma    | Sonraki aşama                  |
| `freertos.c`         | Eski task listesi        | Referans only                  |

### Port sırasında düzeltilecek bilinen SKYRTOS problemleri

* `sensor_fusion.c` ↔ `flight_algorithm.c` bağımlılık sarmalı
* Blocking offset / calibration akışı
* Tainted snapshot problemi
* Mode geçişlerinde state reset eksikliği
* Logging ve telemetry snapshot uyumsuzluğu

## Halil hakkında

* Türkçe konuşur.
* Junior embedded geliştirici.
* Bitirme projesinde çalışan, anlaşılır ve jüriye anlatılabilir kod ister.
* Büyük refactor yerine küçük, doğrulanabilir adımlarla ilerlemek daha uygundur.

## Öncelikli yol haritası

1. FreeRTOS proje iskeletini doğrula.
2. `Application_Start()` üzerinden tek giriş noktası kur.
3. IMU driver akışını düzelt ve temiz pipeline'ı kur:

   * ACC DRDY IRQ → DMA başlat
   * GYRO DRDY IRQ → DMA başlat
   * DMA complete → parse
   * accel + gyro ikisi de güncellenince Mahony update
4. Task modeli ve stack boyutlarını temizle.
5. İlk modülü güvenli şekilde ekle.
6. Sonra sensör driver'lara geç.
7. Ardından füzyon ve flight mantığı gelir.
8. IWDG ekle ve sistem health modelini tamamla.

## Değişmez prensip

**Önce güvenilir ve anlaşılır çalışan basit sistem. Sonra genişletme.**

Spagetti yok. Kernel yazmak yok. Blocking yok. Tek sahipli veri akışı var. Adım adım ilerliyoruz.
