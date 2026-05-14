# SUT (Software Under Test) — Mimari ve Uygulama Kılavuzu

## 1. Amaç

SUT testinin tek amacı **Kalman filtresi ve flight state machine algoritmasını** simüle edilmiş uçuş verisiyle doğrulamaktır. Donanım sürücüleri (BMI088, BME280, GNSS) bu testle ilgisizdir; onları çalıştırmaya gerek yoktur.

---

## 2. Mevcut Mimari Tanısı

### 2.1 STM32 Tarafı — Sorunlar

| Sorun | Açıklama |
|-------|----------|
| **Çok task çakışması** | SUT modunda imu_task + baro_task + telemetry_task hâlâ ayakta. Her biri `sys_mode_get()` kontrolü yapıp kendi dalına giriyor. Aralarındaki queue koordinasyonu gereksiz karmaşıklık. |
| **Baro task blocking bekle** | `sys_mode_sut_baro_receive(&sut_baro, 200U)` baro_task'ı bloke ediyor. imu_task batch'i işleyip snapshot yayınlıyor, baro_task IMU snapshot'ını peek ile okuyor. Bu gevşek çift-queue akışı zamanlama garantisi vermiyor. |
| **Telemetry task gereksiz çalışıyor** | 50 Hz UART2 telemetri SUT modunda sadece algoritma çıktısını kirletiyor; Python tarafı bu formatı zaten ayrıştırmak zorunda kalıyor. |
| **Ayrı mahony + kalman tetikleme** | IMU batch → imu_task → snapshot → baro_task → kalman → flight_sm zinciri. Tek görev yapsa bir adım olacak şey üç task üzerinden geçiyor. |

### 2.2 Python Tarafı — Sorunlar

| Sorun | Kök Neden |
|-------|-----------|
| **Lock contention** | `_lock`, sender (`_send_command`) ve receiver (`_sut_receiver_loop`) tarafından paylaşılıyor. Receiver her 10 ms'de `with self._lock: serial.read(...)` yapıyor; bu Windows'ta GIL + mutex kombinasyonu. Sender'ın busy-wait döngüsü sırasında lock alınırsa hedef anı kaçırıyor. |
| **Yavaş timer** | `time.sleep(0.01)` Windows'ta ~15 ms uyuyabiliyor. Sender `timeBeginPeriod(1)` çağırıyor ama Python GIL paylaşımında bu sadece sleep çağrısını yapan thread'i etkiliyor. |
| **GUI backpressure** | `root.after(0, ...)` her callback'te Tkinter event kuyruğuna iş ekliyor. Tkinter main loop ağırsa kuyruk birikir, simülasyon yavaşlar. |
| **Tkinter plot yavaş** | matplotlib + tkinter canvas kombinasyonu, her frame'de tüm grafiği yeniden render ediyor. 51 saniye × 10 Hz = 510 güncelleme → belirgin lag. |
| **İki thread bir serial nesne** | pyserial Windows'ta read/write thread-safe değil; lock olmadan eş zamanlı çağrı UB. Lock varsa contention. |

---

## 3. Yeni Mimari

### 3.1 Temel Felsefe

> **Tek yönlü, tek sorumlu boru hattı.**  
> PC veriyi gönderir → STM32 hesaplar → STM32 sonucu gönderir → PC kaydeder.  
> Hız baskısı yoktur: PC mümkün olduğunca hızlı gönderir, STM32 işleyip hemen cevaplar.  
> Gerçek zamanlı pacing KALDIRILDI.

### 3.2 STM32 — Yeni SUT Task Akışı

```
cmd_task  ──notify──►  sut_task
(paket parse)           ├─ Mahony × N  (IMU batch, sim_time dt)
                        ├─ Kalman      (baro, sim_time dt)
                        ├─ flight_sm_update()
                        └─ SUT_RESPONSE paketi gönder (UART2 DMA)
```

**Aktif task:** sadece `cmd_task` + `sut_task`  
**Diğer taskler (imu, baro, telemetry, gnss):** SUT modunda `xTaskNotifyWait(portMAX_DELAY)` içinde uyurlar — CPU kullanmazlar.

```
sys_mode_set(MODE_SUT) tetiklenince:
  imu_task     → mode kontrolü → portMAX_DELAY ile notify bekler
  baro_task    → mode kontrolü → portMAX_DELAY ile notify bekler
  telemetry_task → mode kontrolü → portMAX_DELAY ile notify bekler
  gnss_task    → mode kontrolü → portMAX_DELAY ile notify bekler
  sut_task     → uyandı, loop başlıyor
```

#### sut_task iç döngüsü (pseudo-code)

```c
for (;;) {
    // cmd_task'tan notify bekle (portMAX_DELAY)
    xTaskNotifyWait(0, UINT32_MAX, &bits, portMAX_DELAY);

    if (!(bits & NOTIFY_SUT_BATCH)) continue;

    sut_combined_t pkt;
    if (!sut_queue_receive(&pkt)) continue;

    // 1. Mahony: batch içindeki her IMU örneği
    for (int i = 0; i < pkt.imu_count; i++) {
        float dt = sim_time_delta(pkt.imu[i].sim_time, &prev_sim_time);
        mahony_update(&mah, pkt.imu[i].gx, pkt.imu[i].gy, pkt.imu[i].gz,
                      0.0f, 0.0f, 0.0f, dt);  // gyro-only SUT modunda
    }

    // 2. Kalman: baro verisiyle
    float dt_baro = sim_time_delta(pkt.baro.sim_time, &prev_baro_time);
    float alt_rel  = pkt.baro.altitude - alt_ref;
    float filtered = alt_kalman_update(&kf, alt_rel, 0.0f, dt_baro);

    // 3. flight_sm
    alt_snapshot_t alt_snap = { .altitude_rel = filtered, .velocity = kf_vel };
    imu_snapshot_t imu_snap = { .euler = euler_from_mahony(&mah) };
    flight_sm_update(&alt_snap, &imu_snap);

    // 4. Cevap paketi gönder
    sut_send_response(pkt.baro.sim_time, filtered, roll, pitch, yaw, flight_status);
}
```

#### Diğer task'larda mod kontrolü (mevcut kod yeterli, küçük ekleme)

```c
// imu_task, baro_task, telemetry_task, gnss_task içinde:
if (sys_mode_get() == MODE_SUT) {
    xTaskNotifyWait(0, UINT32_MAX, NULL, portMAX_DELAY);
    // Notify hiç gelmez; sadece sys_mode_set(MODE_NORMAL) sonrası
    // task silinir/yeniden başlatılır (reset sequence)
    continue;
}
```

### 3.3 Yeni SUT Response Paketi (STM32 → PC)

```
Offset  Boyut  İçerik
──────  ─────  ────────────────────────────────
[0]     1      Header  = 0xAE
[1-4]   4      sim_time (float BE) — hangi pencereye ait
[5-8]   4      filtered_alt (float BE, m AGL)
[9-12]  4      roll  (float BE, derece)
[13-16] 4      pitch (float BE, derece)
[17-20] 4      yaw   (float BE, derece)
[21]    1      flight_status HIGH byte
[22]    1      flight_status LOW byte
[23]    1      checksum (sum [0..22] mod 256)
[24]    1      0x0D
[25]    1      0x0A
──────────────────────────────────────────────
Toplam: 26 byte
```

> **Not:** Eski 54-byte 0xAB telemetri paketi SUT modunda gönderilmeyecek. sut_task doğrudan bu küçük paketi DMA ile gönderir.

### 3.4 SUT_COMBINED Paketi (PC → STM32) — KORUNUYOR

```
0xAD | count(1) | count×[sim_t(4)+gx(4)+gy(4)+gz(4)] | alt(4)+press(4)+baro_t(4) | chk | CR LF
```

count ≤ 20 (100 ms pencere / 5 ms = 20 IMU örneği)

---

## 4. Python Yeni Mimarisi

### 4.1 Teknoloji Seçimi

| Bileşen | Eski | Yeni | Neden |
|---------|------|------|-------|
| GUI framework | tkinter + customtkinter | **PyQt5** | Qt event loop serial I/O ile entegre çalışır |
| Plot | matplotlib canvas | **PyQtGraph** | OpenGL tabanlı, 100 fps destekler |
| Serial thread | threading.Thread × 2 | **QThread × 1** | Tek thread → sıfır contention |
| Thread iletişimi | root.after() | **Qt signals** | Thread-safe, kuyruk garantili |

### 4.2 Modül Yapısı

```
SIT_SUT/sut_tool/
├── main.py          — QApplication başlatır, MainWindow oluşturur
├── protocol.py      — paket encode/decode (saf Python, Qt bağımlılığı yok)
├── serial_worker.py — QThread: CSV gönder + response al, sinyaller emit eder
├── plot_widget.py   — PyQtGraph canlı alt + roll/pitch/yaw + faz çizgileri
└── main_window.py   — QMainWindow: port/dosya seçimi, başlat/durdur, plot
```

### 4.3 serial_worker.py — Akış

```
QThread.run():
  1. CSV yükle, yüzde hesapla
  2. serial.write(SUT_CMD)
  3. time.sleep(3.0)          ← STM32'nin hazır olması için
  4. for pencere in csv_windows:
       a. pkt = build_combined_packet(pencere)
       b. serial.write(pkt)   ← kilit yok, tek writer
       c. resp = read_response(timeout=0.5)  ← okuma, aynı thread
       d. if resp: emit result_signal(resp)
  5. emit finished_signal()
```

**Kilit yok:** Gönderme ve alma aynı thread'de sıralı olarak yapılıyor.  
**Gerçek zamanlı pacing yok:** Her pencere gönderilir, cevap beklenir, sonraki gönderilir. Hız STM32'nin işleme hızıyla sınırlı (~birkaç ms).  
**Sonuç:** 51.5 saniyelik simülasyon < 2 saniyede tamamlanabilir.

### 4.4 Qt Signals

```python
class SerialWorker(QThread):
    result_ready = pyqtSignal(float, float, float, float, float, int)
    # args: sim_time, alt, roll, pitch, yaw, status

    progress = pyqtSignal(int)   # 0-100
    error    = pyqtSignal(str)
    finished = pyqtSignal()
```

### 4.5 PyQtGraph Plot

- **Alt grafiği:** X=sim_time, Y=filtered_alt — yeşil çizgi
- **Euler grafiği:** roll (mavi), pitch (turuncu), yaw (kırmızı)
- **Faz geçiş çizgileri:** flight_status değişince dikey kesikli çizgi + etiket
- **Güncelleme:** `result_ready` sinyali her response için → `plot.update()` → O(n) değil O(1) (sadece son nokta eklenir)

---

## 5. STM32 Uygulama Adımları

### Adım 1 — `sut_task.h` / `sut_task.c` oluştur

- `sut_task_create()` — `app.c` içinde `sys_mode_set(MODE_SUT)` tetiklenince çağrılacak
- `sut_task_notify(const sut_combined_t *pkt)` — cmd_task'tan çağrılır
- İçeride: mahony_t, alt_kalman_t, flight_sm — hepsi local state

### Adım 2 — `cmd_task.c` güncelle

- `SUT_COMBINED` parse → `sut_task_notify()` (artık imu_task_notify_sut_batch yok)
- `CMD_SUT` → `sys_mode_set(MODE_SUT)` + `sut_task_create()`
- `CMD_STOP` → `sys_mode_set(MODE_NORMAL)` + `sut_task_delete()` (vTaskDelete)

### Adım 3 — Diğer taskler

Her task'ın döngü başında:
```c
if (sys_mode_get() == MODE_SUT) {
    xTaskNotifyWait(0, UINT32_MAX, NULL, portMAX_DELAY);
    continue;
}
```
Bu satırlar zaten var (imu_task'ta `continue` var); baro/telemetry/gnss için de aynı pattern uygulanır.

### Adım 4 — `sut_send_response()` implement et

`UART2 DMA TX` — 26 byte, mevcut telemetry_task'ın DMA callback'ini kullanmak yerine:  
sut_task kendi `HAL_UART_Transmit_DMA` çağrısını yapar ve TX_COMPLETE için task notification bekler.  
(Telemetry task SUT modunda suspend/sleep olduğundan callback çakışması yok.)

### Adım 5 — Kalman parametrelerini SUT için ayarla

```c
kf.r_acc = 5000.0f;   // ivme kanalı susturulmuş (gyro-only SUT)
kf.r_alt = 5.0f;      // baro varyansı
kf.q     = 0.01f;     // process noise
```

---

## 6. Python Uygulama Adımları

### Adım 1 — Bağımlılıklar

```
pip install pyqt5 pyqtgraph pyserial
```

### Adım 2 — `protocol.py`

```python
SUT_COMBINED_HEADER  = 0xAD
SUT_RESPONSE_HEADER  = 0xAE
SUT_RESPONSE_SIZE    = 26

def build_combined_packet(imu_rows, baro_row) -> bytes: ...
def parse_response(data: bytes) -> dict | None: ...  # sim_time, alt, roll, pitch, yaw, status
```

### Adım 3 — `serial_worker.py`

- `QThread` subclass
- `run()` → sıralı send/receive döngüsü (kilit yok)
- `stop()` → `self._running = False`
- Response parse: `parse_response(bytes)` — basit, 26 byte sabit boyut

### Adım 4 — `plot_widget.py`

```python
import pyqtgraph as pg

class SutPlotWidget(pg.GraphicsLayoutWidget):
    def __init__(self):
        self.alt_curve  = ...  # PlotCurveItem
        self.roll_curve = ...
        # vb.

    def append(self, sim_time, alt, roll, pitch, yaw, status):
        # self.t_data.append(sim_time) vs.
        # self.alt_curve.setData(self.t_data, self.alt_data)
        # Faz geçişi varsa InfiniteLinearRegionItem ekle
```

### Adım 5 — `main_window.py`

- Port seçimi (QComboBox, refresh butonu)
- CSV dosya seçimi (QFileDialog)
- Başlat / Durdur butonu
- Progress bar
- Log alanı (QPlainTextEdit)
- `SutPlotWidget` embed

---

## 7. Dikkat Edilmesi Gerekenler

### STM32

| Risk | Önlem |
|------|-------|
| sut_task stack taşması | Mahony + Kalman float işlemleri için 512 × 4 byte yeterli; flight_sm ek gereksinim yok |
| DMA TX çakışması | sut_task DMA başlatmadan önce önceki TX'in tamamlandığını beklemeli (NOTIFY_TX_DONE) |
| alt_ref sıfırlanması | SUT modu başlangıcında ilk paket `alt_ref` kurar, sonraki paketler bu referansa göre hesaplar |
| Mod geçiş güvenliği | `sys_mode_set` atomic değil; ancak tek writer (cmd_task) olduğundan sorun yok |
| sim_time süreksizliği | `dt <= 0 || dt > 0.5` koşulunda dt sabit 0.005f (IMU) veya 0.1f (baro) alınır |

### Python

| Risk | Önlem |
|------|-------|
| Response timeout | STM32 meşgulse response gelmeyebilir; 500 ms timeout → hata logla, sonraki pencereye geç |
| CSV sütun eksikliği | pressure sütunu artık CSV'de zorunlu değil (STM32 kullanmıyor); pressure yoksa 0.0 gönder |
| Büyük CSV | 10.000+ satır → önceden pandas ile 20'li gruplara böl, belleğe al |
| Port meşgulsa | pyserial exception → UI'ya hata mesajı, thread güvenli kapat |
| PyQtGraph kurulu değil | `ImportError` → net hata mesajı, kurulum komutu yaz |

### Protokol

| Risk | Önlem |
|------|-------|
| STM32 garbled response | Header 0xAE + footer CR LF + checksum ile doğrula |
| Byte sırası | Tüm float'lar big-endian (BE); hem encode hem decode `'>f'` |
| count > 20 | cmd_task parse'da zaten `cnt > SUT_IMU_BATCH_MAX` kontrolü var |

---

## 8. Test Doğrulama Kriterleri

SUT testinin başarılı sayılması için:

1. **Faz geçişleri doğru:** CSV verisi 51.5 s uçuş → flight_sm IDLE → ARMED → BOOST → COAST → APOGEE → DROGUE → MAIN → LANDED geçişleri beklenen sim_time'larda olmalı.
2. **Kalman alt yakınsıyor:** Filtered alt, ham baro'ya göre daha pürüzsüz; apogee yüksekliği ham baro ile uyumlu (±%5).
3. **Mahony kararlı:** Yaw drift olmaksızın roll/pitch ±2° doğrulukta.
4. **Python < 5s:** 51.5 saniyelik uçuş Python tarafında 5 saniyeden kısa sürer (gerçek zaman pacing yok).
5. **Sıfır kayıp:** Gönderilen pencere sayısı = alınan response sayısı (log ile doğrula).

---

## 9. Yapılacaklar Sırası

- [ ] `sut_task.c` / `sut_task.h` yaz (Adım 1)
- [ ] `cmd_task.c` routing güncelle (Adım 2)
- [ ] Diğer task'lara SUT sleep ekleme (Adım 3)
- [ ] `sut_send_response()` implement et (Adım 4)
- [ ] STM32 flash → gerçek donanımda el ile test (port, paket alınıyor mu?)
- [ ] `protocol.py` yaz + unit test (Python tarafında paket encode/decode doğrula)
- [ ] `serial_worker.py` yaz (QThread, send/receive döngüsü)
- [ ] `plot_widget.py` yaz (PyQtGraph)
- [ ] `main_window.py` yaz (bütünleştir)
- [ ] Uçtan uca SUT testi: CSV → Python → STM32 → Python → plot
