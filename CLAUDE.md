# mini_rtos — STM32F446 NUCLEO Flight Computer

Halil'in bitirme projesi. BMI088 IMU + BME280 baro üzerinden çalışan
roket flight computer'ı. Kendi yazdığı **MiROS** kernel'i + STM32 **HAL**.
Eski FreeRTOS tabanlı SKYRTOS projesi MiROS'a port ediliyor.

## Klasör yapısı

```
mini_rtos/
├── Cube/         ← aktif proje (STM32CubeIDE .ioc + HAL + MiROS)
├── OLD_Project/  ← SKYRTOS arşivi — referans, driver portu için ilham
├── Report.md     ← bitirme raporu için teknik notlar (zorluklar, teşhis, çözüm)
└── CLAUDE.md     ← bu dosya
```

**`OLD_Project/`'i ASLA silme.** Kalman, Mahony quaternion, BME280, GPS,
sensor fusion, flight algorithm gibi modüller buradan port edilecek
(kopya değil — okuyup MiROS+HAL'e uyarlayarak port).

**`Report.md` — bitirme raporu için ham not deposu.** Ciddi teknik
zorluklar (kök neden + teşhis süreci + uygulanan çözüm + proper fix
TODO'ları) buraya kaydedilir. Sıradan refactor / küçük bug fix
kaydedilmez — rapora yazılacak değerde, öğretici olaylar için.
Her giriş kendi başlığı altında, tarihli; rapor yazımında bu notlar
genişletilip akademik dile çevrilecek. Önemli bir hata teşhisi /
mimari karar / non-obvious bug çözümü olduğunda buraya ekle.

## Aktif proje (`Cube/`)

CubeMX `.ioc` üzerinden peripheral init üretiliyor; biz **sadece**
`Core/Src/app.c` (uygulama katmanı) ve `Core/Src/bmi088.c` (HAL bazlı
sürücü) yazıyoruz. Cube'un ürettiği dosyalara minimum dokunuş:

| Dosya | Görev | Cube üretti mi? |
| --- | --- | --- |
| `Core/Src/main.c` | HAL init + `Application_Start()` çağrısı | Evet — USER CODE 2'ye tek satır eklenmiş |
| `Core/Src/app.c` | MiROS init, task'lar, HAL callback override'ları | Hayır |
| `Core/Src/bmi088.c` | BMI088 sürücü (polled init + DMA read + parse) | Hayır |
| `Core/Src/mahony.c` | 6DOF AHRS (gyro+accel füzyon, OLD'dan port) | Hayır |
| `Core/Src/miros.c` | Kernel (priority scheduler + event flags + PendSV asm) | Hayır |
| `Core/Inc/app.h` | Event mask define'ları, `Application_Start()` proto | Hayır |
| `Core/Inc/bmi088.h`, `bmi088_regs.h` | Driver API + register map | Hayır |
| `Core/Inc/mahony.h` | AHRS API (quat/euler/gyro-only flag) | Hayır |
| `Core/Inc/miros.h`, `qassert.h` | Kernel API + assertion helper | Hayır |
| `Core/Src/stm32f4xx_it.c` | IRQ vektörleri | Evet — **PendSV ve SysTick stubları silindi** (MiROS sağlıyor) |
| `Core/Src/i2c.c`, `gpio.c`, ... | Peripheral init | Evet, dokunma |
| `Core/Src/stm32f4xx_hal_timebase_tim.c` | HAL tick TIM6'da | Evet, dokunma |
| `cmake/stm32cubemx/CMakeLists.txt` | Cube source listesi | Evet, dokunma — user source `Cube/CMakeLists.txt`'e eklenir |

## Mimari kararlar (SABİT — tekrar tartışılmaz)

- **MiROS** kullanılıyor: priority scheduler (1..31, prio = index), per-thread
  event flags (`OS_evtWait` LSB-first tek bit consume eder),
  ISR-safe `OS_evtSignal_FromISR` (PRIMASK save/restore, PendSV pend).
- **PendSV FPU-aware** (Cortex-M4F): her thread context'i `r4-r11 + EXC_RETURN`
  + (varsa) `s16-s31` saklar. Initial frame'de `EXC_RETURN=0xFFFFFFF9`.
  Detay için bkz. [Report.md](Report.md) §1. ARMv6-M (M0) portu kaldırıldı.
- **HAL** kullanılıyor. Önce bare-metal denedik (i2c1.c register-level,
  EXTI manuel setup) — Halil için karmaşık geldi, **2026-05-11 pivot**
  ile HAL'e geçtik. Register-level kod silindi.
- **FreeRTOS yok, AO/QP yok.** İki kernel paralel çalışmaz. Düz event
  queue + dispatcher mantığı kullanılır.
- **Watchdog (IWDG)** her senaryoda eklenmeli (bitirme projesi de olsa).
- **Donanımdan ayrı main:** `main.c` HAL init yapar, sonra
  `Application_Start()` çağırır (USER CODE 2 bloğu — regenerate-safe).
  MiROS init, task'lar, callback'ler `app.c`'de.
- **HAL tick TIM6'da**, SysTick MiROS'a serbest.
- **PendSV/SysTick handler çakışması:** `stm32f4xx_it.c`'deki boş Cube
  stubları silindi. MiROS kendi versiyonlarını sağlıyor. CubeMX yeniden
  generate ederse stublar geri gelir → tekrar silmek gerek (dosyada uyarı
  yorumu var).

## Coding kuralları

- **Busy-wait / blocking polling yasak.** Sensör okuma her zaman
  `HAL_I2C_Mem_Read_DMA` + DMA complete event üzerinden. `HAL_Delay`
  yerine `OS_delay(ticks)`.
- **HAL weak callback override'ları** event POST için kullanılır:
  - `HAL_GPIO_EXTI_Callback(GPIO_Pin)` — EXTI3/EXTI4 dispatch
  - `HAL_I2C_MemRxCpltCallback(hi2c)` — DMA complete
  - `HAL_I2C_ErrorCallback(hi2c)` — bus error
- **ISR'lardan** `OS_evtSignal_FromISR(&thread, EVT_BIT)` kullanılır.
- Comment yazma kuralı: WHY non-obvious ise yaz (datasheet quirk, hidden
  invariant), WHAT'i kod söylüyor. Tek satır docstring/comment yeter.
- Türkçe açıklama OK ama kodda identifier'lar İngilizce.

## Donanım haritası (.ioc'ten)

| Peripheral | Pin | Görev |
| --- | --- | --- |
| I2C1 (DMA1 Stream0 Ch1) | PB8 SCL, PB9 SDA | BMI088 (acc 0x18, gyro 0x68) |
| I2C3 | PA8 SCL, PC9 SDA | BME280 (sonra) |
| EXTI3 | PB3 | BMI088 ACC INT1 (DRDY, rising) |
| EXTI4 | PB4 | BMI088 GYRO INT3 (DRDY, rising) |
| USART2 | DMA1 Stream6 (TX), Stream5 (RX) | telemetry — Mahony çıktısı (scaled int, 100 Hz) |
| USART6 / UART4 | DMA | (gelecek) |
| SPI1, SPI3 | — | (gelecek: SD, W25 flash) |
| TIM2 | — | (gelecek) |
| TIM6 | dahili | HAL tick (1 ms) |

`Cube/Core/Src/gpio.c`, `i2c.c`, `dma.c` Cube tarafından üretildi,
detaylar oradadır.

## Event yapısı (şu anki tek task: imuThread)

```c
// Cube/Core/Inc/app.h
#define EVT_ACCEL_DRDY  (1U << 0)
#define EVT_GYRO_DRDY   (1U << 1)
#define EVT_DMA_DONE    (1U << 2)
#define EVT_I2C_ERROR   (1U << 3)
```

`imuThread` (prio 5):
```
EXTI3 (ACC DRDY)  →  HAL_GPIO_EXTI_Callback  →  POST EVT_ACCEL_DRDY
EXTI4 (GYRO DRDY) →  HAL_GPIO_EXTI_Callback  →  POST EVT_GYRO_DRDY
imuThread.evtWait → DRDY → HAL_I2C_Mem_Read_DMA(6 bytes)
DMA1_Stream0 IRQ  →  HAL_I2C_MemRxCpltCallback  →  POST EVT_DMA_DONE
imuThread.evtWait → DMA_DONE → parse → s_have_{accel,gyro} fresh flag
   if both fresh → mahony_update(dt=1/400 s) → telem decimator (÷4)
                   → UART2 DMA TX (drop on busy, ~100 Hz)
```

Tek I2C bus + tek DMA stream var → DRDY'ler serileştirilir.
`app.c`'deki `s_phase` (IDLE / READ_ACCEL / READ_GYRO) state machine
in-flight transfer'i izler, `s_pending_accel/gyro` bekleyen DRDY'leri
tutar, `imu_kick_next()` DMA done'da bekleyeni başlatır.

Telemetri formatı (ASCII, scaled int — float-printf bağımlılığı yok):
`q,<w*10000>,<x*10000>,<y*10000>,<z*10000>,e,<r*100>,<p*100>,<y*100>,m,<gmode>\r\n`
NUCLEO ST-LINK VCP üzerinden 115200 baud (PA2/PA3 → ST-LINK USB).
Önceki TX hala uçuyorsa o örnek düşürülür (block etmek imuThread'i
durdurur, telemetri için kabul edilemez).

## Build

```powershell
# Cube/ klasöründen
cmake --preset Debug
cmake --build build/Debug
```

Çıktı: `Cube/build/Debug/mirtos.elf` (+ .hex + .bin tarafından otomatik).
Size kontrolü için `arm-none-eabi-size build/Debug/mirtos.elf`.

## OLD_Project'ten port edilecek modüller

Yol: `OLD_Project/Core/Src/` (working directory altında — relative path).
Kopyalamak yerine algoritmik mantığı çıkarıp MiROS+HAL'e uyarla.

| Dosya | İçerik | Port önceliği |
| --- | --- | --- |
| ~~`queternion.c`~~ | Mahony quaternion update | **Port edildi 2026-05-11** → `mahony.c`. Tek versiyon vardı (typo'lu). FPU-friendly versiyona temizlendi, BMI_sensor coupling kaldırıldı, gyro-only mode + adaptive Kp/Ki korundu |
| `kalman.c` | Altitude Kalman filter | Yüksek — flightTask için |
| `bme280.c` | Basınç/sıcaklık sürücü (I2C3) | Yüksek — flightTask DRDY'si |
| `flight_algorithm.c` | Uçuş fazı state machine (boost/coast/apogee/descent/touchdown) | Orta |
| `sensor_fusion.c` | Sensor fusion + accel failure detection | Orta — Mahony + Kalman entegrasyon |
| `e22_lib.c` | LoRa telemetry (E22 radyo) | Düşük |
| `l86_gnss.c` | L86 GPS | Düşük |
| `data_logger.c` | SD kart write buffering | Düşük |
| `uart_handler.c` | UART2 RX command dispatcher | Düşük |
| `freertos.c` | Eski task tanımları (MiROS karşılığı `app.c`) | Sadece referans — port etme |

**Bilinen SKYRTOS bug'ları** (port sırasında düzelt):
- `sensor_fusion.c` ↔ `flight_algorithm.c` circular dependency
- Mahony gyro-only path
- `get_offset()` blocking

## Halil hakkında

- Junior embedded geliştirici, Türkçe konuşur.
- Bitirme projesi → jüri etkisi önemli, çalışan + okunabilir kod tercih.
- Adım adım gitmeyi sever ("ben de anlayalım") — büyük tek seferlik
  refactor yerine küçük doğrulanabilir parçalar.
- Eski bare-metal denemesinden vazgeçti; HAL + MiROS hibrit
  yaklaşımıyla devam ediyor.

## Sonraki adımlar (yapılacaklar)

1. ~~Board'a flash → `imuThread`'in `bmi088_init`'ten geçtiğini doğrula.~~
2. ~~DRDY → DMA → parse zinciri ve Mahony çalıştığını UART üzerinden gör.~~
3. ~~Mahony quaternion port + `EVT_DMA_DONE` zincirine bağlama.~~
4. **Mahony doğrulaması:** ST-LINK VCP @ 115200 baud, terminal aç,
   "q,...,e,...,m,0" satırlarının aktığını gör; board'u el ile döndür,
   roll/pitch/yaw mantıklı değişmeli. Yüksek-g (sallayarak) `m,1`
   olmalı (gyro-only mode kicked in).
5. Microsecond timer (TIM2) — gerçek dt için. Şu an `MAHONY_DT_S` sabit
   1/400 s, gerçek inter-sample süre değil. ODR drift'ten etkilenir.
6. BME280 (I2C3) sürücüsü + flightTask iskeleti.
7. Kalman altitude filter (OLD/kalman.c port) — flightTask için.
8. Watchdog (IWDG) refresh — imuThread içinden periyodik kick.
