# Report.md — Bitirme Projesi Notları

Bu dosya bitirme projesi raporunda kullanılacak teknik notları,
karşılaşılan zorlukları ve çözümleri toplar. Her madde rapor için
genişletilebilir bir taslaktır: problem → teşhis → kök neden → çözüm.

---

## 1. FPU Extended Exception Frame ↔ MiROS PendSV Çatışması

**Tarih:** 2026-05-11
**Etkilenen modüller:** `Cube/Core/Src/miros.c` (PendSV), `Cube/Core/Src/bmi088.c`
(float parse), `Cube/Core/Src/app.c` (fix uygulanan yer).

### Belirti

`imuThread`'i flash'layıp debugger ile takip ettiğimde HardFault'a
düştü. Davranış **intermittent** idi: aynı binary'i iki kez başlatınca
bir seferinde fault geldi, diğerinde gelmedi.

Debugger'da yakalanan değerler:

| Register | Değer |
| --- | --- |
| `hf_pc` | `0x20000EA0` |
| `cfsr`  | `0x00020000` |
| `sp`    | `stack_imu+1916` (`0x20000E38`) |

### Teşhis süreci

1. `stm32f4xx_it.c` içindeki `HardFault_Handler`'a `naked` wrapper
   yazıp `MSP/PSP` ayrımı yapan kısa bir assembly stub ekledim
   (`TST lr,#4` → `MRS r0,MSP/PSP`). Stack frame'i C tarafında
   `HardFault_Handler_C` parse ediyor, `hf_pc/hf_lr/hf_cfsr` global'leri
   debugger'dan okunabiliyor.
2. `0x20000EA0` adresinin Flash (`0x08000000`) değil SRAM
   (`0x20000000`) bölgesinde olduğunu görünce kodun veri bölgesine
   branch attığını anladım — ciddi bir corruption işareti.
3. `CFSR=0x00020000` → bit 17 → `UFSR.INVSTATE`. ARM ARM'a göre bu
   bayrak "EPSR.T temizlenmiş halde instruction execute edildi" demek;
   Cortex-M Thumb-only olduğu için PC'nin LSB=0 değeri yüklendiğinde
   tetiklenir. `0x20000EA0` çift sayı, yani PC değerinin LSB'i 0.

### Kök neden

STM32F446 Cortex-M4 + FPU. CubeMX'in ürettiği `SystemInit` CPACR'ı
açık bırakıyor (CP10, CP11 = `0b11`). Bu durumda iki ekstra HW davranışı
default olarak **enabled**:

- `FPCCR.ASPEN = 1`: FPU instruction execute edildiğinde `CONTROL.FPCA`
  bit'i otomatik set edilir.
- `FPCCR.LSPEN = 1`: Lazy stacking.

Çalışan senaryo:

1. `imuThread` ilk DMA tamamlandıktan sonra `bmi088_parse_accel` çağırır.
   Bu fonksiyon `float` aritmetik içeriyor → VLDR/VMUL/VSTR instruction'ları
   emit edilir → `FPCA=1`.
2. Sonraki SysTick (her 10 ms'de bir) tetiklendiğinde HW, `FPCA=1`
   olduğu için **extended exception frame** push'lar:
   - Basic frame: 8 word (r0-r3, r12, lr, pc, xpsr) = 32 byte
   - Extended frame: + 18 word (s0-s15, FPSCR, reserved) = +72 byte
   - **Toplam 104 byte (lazy ile 26 word yer ayrılır, s0-s15 yazımı ertelenir)**
3. SysTick `OS_sched`'i çağırır → idle'a geçiş için `PENDSVSET`.
4. PendSV tail-chain ile çalışır. LR (`EXC_RETURN`) = `0xFFFFFFE9`
   (Thread mode, MSP, **extended** frame).
5. MiROS'un PendSV'si yalnız `r4-r11` (8 word) save/restore eder, FPU
   register'larından ve EXC_RETURN frame-type bit'inden habersizdir.
6. Idle thread'e switch yaparken `BX lr` ile dönüş yapıldığında HW,
   idle'ın saklı SP'sinden 26 word unstack etmeye çalışır. Ama idle'ın
   `OSThread_start` ile kurulan initial frame'i sadece 8 word basic
   frame içerir.
7. HW frame sınırının dışına taşar, idle stack'i bittikten sonraki
   SRAM içeriğini PC olarak yükler. Yüklenen değer çift sayı çıkınca
   `INVSTATE` UsageFault, escalate ederek HardFault.

**Intermittent olmasının sebebi:** `bmi088_init` sırasında polled I/O
var, float yok. Eğer init başarısız olup thread sonsuz delay döngüsüne
girerse parse hiç çağrılmaz, `FPCA=1` olmaz, fault gelmez. Init geçtikten
sonra ilk parse + sonraki context switch arasında oluşur.

### İlk hızlı çözüm (denendi, sonra kaldırıldı)

Bug'ı bulduktan hemen sonra geçici çözüm olarak `Application_Start`'a
şunu koymuştuk:

```c
FPU->FPCCR &= ~(FPU_FPCCR_ASPEN_Msk | FPU_FPCCR_LSPEN_Msk);
```

`ASPEN=0` HW'in `FPCA` bit'ini otomatik set etmesini durdurur, frame
her zaman basic (32 byte) olur, MiROS'un basic-only assumption'ı
geçerli kalır.

**Bu çözümün geçerliliği üç koşula bağlıydı** (hepsi aynı anda):

1. Yalnız tek bir user thread float kullanır (`imuThread`).
2. Hiçbir ISR float / DSP çağırmaz.
3. `arm_math.h` / CMSIS-DSP çağıran callback yok.

Koşullardan biri ihlal edilince s0-s15 / FPSCR register'ları context
switch arasında corrupt olabilirdi. `flightTask` portu, Mahony /
Kalman entegrasyonu, ileride CMSIS-DSP kullanımı — hepsi bu koşulları
zorlayacak. Bu yüzden geçici fix kaldırılıp proper fix uygulandı.

### Proper fix (uygulandı — 2026-05-11)

İki dosya değişti: `Cube/Core/Src/miros.c` (PendSV + OSThread_start) ve
`Cube/Core/Src/app.c` (geçici FPCCR satırı kaldırıldı).

**Kritik kavramsal düzeltme:** problem yalnız "FPU register'larını
kaydetmemek" değil — esas mesele **frame-type bilgisinin (basic vs
extended) thread başına saklanması**. HW, exception return'de `BX lr`
ile gelen `EXC_RETURN` değerinin 4. bit'ine bakıp basic mi extended mi
pop edeceğine karar veriyor. Bu bit context switch boyunca thread'le
beraber taşınmazsa, FPU kullanan A thread'inden FPU kullanmayan B
thread'ine geçişte HW yine extended pop etmeye çalışır → crash.

**Çözüm:**

1. `OSThread_start`: initial stack frame'e bir word eklendi:
   `*(--sp) = 0xFFFFFFF9U;` — yeni thread henüz FPU kullanmadığı için
   basic frame EXC_RETURN değeri. Frame artık 16 değil 17 word
   (sw frame 9 word: r4-r11 + EXC_RETURN; hw frame 8 word).

2. `PendSV_Handler`: `PUSH/POP {r4-r11}` → `PUSH/POP {r4-r11, lr}`
   yapıldı. lr (yani EXC_RETURN) artık thread context'inin parçası.
   Bunun etrafına FPU register save/restore eklendi:

   ```
   ; save outgoing thread:
   TST lr, #0x10              ; bit 4 = 0 → extended frame kullanıldı
   IT  EQ
   VSTMDBEQ sp!, {s16-s31}    ; HW zaten s0-s15 + FPSCR'i koruyor
   PUSH {r4-r11, lr}

   ; restore incoming thread:
   POP {r4-r11, lr}           ; bu thread'in kendi EXC_RETURN'ü
   TST lr, #0x10
   IT  EQ
   VLDMIAEQ sp!, {s16-s31}
   BX lr                      ; HW basic/extended doğru pop ediyor
   ```

3. Bu değişiklikle birlikte M0 (ARMv6-M) branch'leri kaldırıldı —
   PendSV artık yalnız Cortex-M4F üzerinde çalışıyor. IT/VSTMDB/VLDMIA
   instruction'ları M0'da yok. M0 portu gerekirse ileride FPU
   olmayan bir versiyon yazılır.

4. `app.c`'deki geçici `FPCCR` satırı kaldırıldı; CPACR (Cube
   tarafından `SystemInit`'te set ediliyor) ve `FPCCR.ASPEN/LSPEN`
   default değerlerinde, MiROS extended frame'leri kendisi yönetiyor.

**Doğrulama:** build temiz, flash atıldı, `bmi088_parse_accel` float
yolundan geçildikten sonra context switch'lerde HardFault gelmiyor.

### Rapor için çıkarımlar

- Cortex-M exception frame iki form alır (basic / extended). RTOS
  context switch yazarken her iki formu da bilmek şarttır.
- HW'in stack üzerinde tuttuğu metadata (`EXC_RETURN`'ün frame-type
  bit'i) thread context'inin **bir parçasıdır**; salt callee-saved
  register'ları (r4-r11) kaydetmek yetmez. Bu nokta birçok hobi RTOS
  port'unda gözden kaçar.
- Intermittent bug'lar genelde "ilk kez X yapıldıktan sonra Y olunca"
  pattern'inden gelir. INVSTATE + SRAM-domain PC kombinasyonu RTOS'larda
  klasik FPU/PendSV smell'idir; doğru ilk hipotez bu olmalı.
- Cube/HAL'in default'ları (FPU on, ASPEN/LSPEN on) bare-metal/single-
  threaded uygulama için tasarlanmış; custom kernel ile birleşince
  hidden assumption oluyor.
- "Geçici fix" + "proper fix" ikilisi: önce minimal değişiklikle
  semptomu durdur, sonra kök nedeni doğru çöz. Geçici fix'in geçerlilik
  koşullarını yazılı bırakmak, ne zaman yetmeyeceğini bilmemizi sağladı.

### Rapor için çıkarımlar

- Cortex-M exception frame iki form alır (basic / extended). RTOS
  context switch yazarken her iki formu da bilmek şarttır.
- Hardware'in stack üzerinde tuttuğu metadata (EXC_RETURN'ün frame-type
  bit'i) thread context'inin bir parçasıdır; salt callee-saved
  register'ları (r4-r11) kaydetmek yetmez.
- Intermittent bug'lar genelde "ilk kez X yapıldıktan sonra Y olunca"
  pattern'inden gelir. INVSTATE + SRAM-domain PC kombinasyonu RTOS'larda
  klasik FPU/PendSV smell'idir.
- Cube/HAL'in default'ları (FPU on, ASPEN/LSPEN on) bare-metal/single-
  threaded uygulama için tasarlanmış; custom kernel ile birleşince
  hidden assumption oluyor.

---

## 2. imuThread Stack Taşması → OSThread TCB Bozulması → INVPC HardFault

**Tarih:** 2026-05-12
**Etkilenen modüller:** `Cube/Core/Src/app.c` (fix uygulanan yer),
`Cube/Core/Src/miros.c` (OSThread_start + PendSV — doğrudan değişmedi,
root cause'u anlamak için incelendi).

### Belirti

Sistem flash'landı ve UART2 üzerinden düzgün telemetri akışı başladı.
Yaklaşık 800–900 satır "q,...,e,...,m,..." çıktısından sonra sistem
HardFault'a düştü ve dondu. Davranış **deterministik** idi: her
çalışmada aynı bölgede (800–900 örnek) crash geldi.

`HardFault_Handler_C` tarafından yakalanan register değerleri:

| Register | Değer (hex) | Notlar |
| --- | --- | --- |
| `hf_r0`   | `0x0000000F` (15) | Corrupt stack'ten okunan r0 |
| `hf_r1`   | `0x00010002` | — |
| `hf_r2`   | `0x10000000` | — |
| `hf_r3`   | `0xE000E100` | NVIC ISER base adresi — garbage |
| `hf_lr`   | `0x200003DC` | `&huart2` SRAM adresi — geçersiz EXC_RETURN |
| `hf_pc`   | `0x080085EE` | `OS_evtWait` içinde bir adres |
| `hf_cfsr` | `0x00040000` | UFSR.INVPC (bit 18) set |
| `hf_hfsr` | `0x40000000` | HFSR.FORCED — eskalasyon |

### Teşhis süreci

**Adım 1 — CFSR ve hf_lr'yi oku.**

`CFSR=0x00040000` → `UFSR.INVPC` (bit 18). ARM ARM tanımı: *"BX lr ile
exception return yaparken lr (EXC_RETURN) geçersiz."* EXC_RETURN'ün
geçerli değerleri `0xFFFFxxxx` aralığındadır.

`hf_lr = 0x200003DC` — geçersiz EXC_RETURN değeri.
`0x200003DC` adresi `mirtos.map`'te `&huart2` (72 byte handle) olarak
görünür. Yani PendSV `BX lr` yaparken lr register'ında bir UART handle
adresi vardı.

**Adım 2 — hf_pc ile crash noktasını bul.**

`hf_pc = 0x080085EE` → `mirtos.map`'te `OS_evtWait` içinde
(0x0800859C–0x08008648). Bu `imuThread`'in uyku state'inden uyandıktan
sonra dönmesi gereken adres; corrupt exception frame'den unstacked olan PC.

**Adım 3 — BSS layout'u map dosyasından kontrol et.**

`mirtos.map` çıktısı (orijinal binary):

```
.bss.imuThread    0x20000730   0x14   ← OSThread TCB (20 byte)
.bss.stack_imu    0x20000744   0x800  ← stack_imu[512] (2048 byte)
```

`imuThread.sp` (TCB'nin ilk field'ı, offset 0) adres `0x20000730`.
`stack_imu[0]` (stack'in en alt kelimesi) adres `0x20000744`.

TCB, stack'in altından (`stack_imu[0]`) yalnız 20 byte önce, **daha
düşük** bir adreste oturuyor. Cortex-M stack yukarıdan aşağı büyüdüğü
için stack taşması tam olarak TCB üzerine yazar.

**Adım 4 — Taşmanın kaç byte olduğunu anla.**

Efektif stack alanı: `0x20000744 + 0x800 - 20 = 0x20000F30` (top).
`OSThread_start` initial frame'i 17 word (68 byte) push eder, SP başlar
`0x20000EEC`'den. `mahony_update → update_gains_from_acc_error → acosf →
__ieee754_acosf → atanf` libm zinciri + `telem_try_send → snprintf →
_svfprintf_r` newlib frame'leri + her SysTick'te PendSV FPU context save
(`PUSH {r4-r11, lr}` 36 byte + `VSTMDB {s16-s31}` 64 byte + HW extended
frame 104 byte = 204 byte) birleşince 2048 byte sınırı aşıldı.

**Adım 5 — Neden 800–900 örnekte?**

İlk saniyeler boyunca Mahony yakınsama aşamasında: `err_deg` büyük,
`acosf` argümanı değişken, stack derinliği dalgalı. Yaklaşık 8–9 saniye
(800–900 örnek @ ~100 Hz telem rate) sonra filtre yakınsıyor, `err_deg`
küçük ve kararlı, `update_gains_from_acc_error` her seferinde aynı derin
`acosf → atanf` yolunu izliyor. Yığılan stack derinliği tutarlı olarak
sınırı aşıyor. Bu yüzden crash deterministik ama gecikmeli.

**Adım 6 — Taşmanın TCB etkisini doğrula.**

`imuThread.sp` (offset 0) debugger'da `0x0000000F` (15) değerini
taşıyordu — `OSThread_start`'ın 0xDEADBEEF canary'si silinmiş, TCB
üzerine küçük bir tamsayı yazılmış. PendSV: `LDR r0, [imuThread, #0]` →
r0 = 15, `MOV sp, 15`, `POP {r4-r11, lr}` adres ~15'ten (boot ROM /
aliased flash). lr'a `0x200003DC` (&huart2 SRAM adresi) düştü.
`BX 0x200003DC` → INVPC → HFSR.FORCED → HardFault.

### Kök neden

İki birbirini kötüleştiren sorun:

1. **Yanlış tanımlama sırası (BSS layout).** `app.c`'de `imuThread`
   TCB'si `stack_imu` dizisinden önce tanımlanmıştı. Bağlayıcı BSS'i
   tanımlama sırasıyla yerleştirince TCB, stack'in hemen altına (daha
   düşük adrese) oturdu. Her taşma doğrudan `imuThread.sp`'yi bozuyordu.

2. **Yetersiz stack boyutu.** 512 word (2048 byte), Mahony libm
   çağrıları + newlib snprintf + PendSV FPU context yükü (≈204 byte/IRQ)
   için yetmiyordu.

### Fix (uygulandı — 2026-05-12)

`Cube/Core/Src/app.c`'de iki değişiklik:

```c
/* Önce (hatalı): TCB stack'in altında, taşma TCB'ye gidiyor */
static OSThread  imuThread;
static uint32_t  stack_imu[512];

/* Sonra (düzeltilmiş): stack TCB'nin altında, taşma BSS boşluğuna gider */
static uint32_t  stack_imu[1024];  /* 512 → 1024 word (2 kB → 4 kB) */
static OSThread  imuThread;
```

Yeni BSS layout (`mirtos.map`, fix sonrası):

```
.bss.stack_imu    0x20000730   0x1000  ← stack_imu[1024] (4096 byte)
.bss.imuThread    0x20001730   0x14    ← TCB şimdi stack'in üzerinde
```

Build sonrası RAM: 4.2 kB → 8.2 kB (%6.3 / 128 kB) — yeterli boşluk var.

### Rapor için çıkarımlar

- Stack overflow ile INVPC HardFault arasındaki zincir doğrudan değil:
  taşma → `OSThread.sp` bozulması → PendSV geçersiz SP ile pop → lr'da
  çöp → `BX` ile INVPC. Cortex-M'de MPU olmadan taşmanın nerede durduğu
  bilinmez; semptom uzaktan ve gecikmeli gelir.
- BSS tanımlama sırası C standardı garantisi değildir — GCC ve Clang
  nesne dosyası içindeki tanımlama sırasına göre yerleştirir. Stack–TCB
  bitişikliği bu garantisiz davranışa dayanıyorsa kırılgandır. Defensive
  sıralama (stack önce, TCB sonra) taşmayı en azından TCB'den uzaklaştırır.
- "Deterministik ama gecikmeli" crash pattern'i filtre yakınsama süresiyle
  stack derinliğinin etkileştiğini gösteriyor. Bare-metal stack profiling
  (0xDEADBEEF high-water mark taraması) boyutlandırma için şart.
- Uygulama seviyesinde `Q_ASSERT(stack_imu[0] == 0xDEADBEEFU)` canary
  kontrolü `EVT_DMA_DONE` handler'ına eklenebilir; taşma olduğunda
  kontrollü assertion ile durulur, debug bilgisi kaybolmaz.

---
