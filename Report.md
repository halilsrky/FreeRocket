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
