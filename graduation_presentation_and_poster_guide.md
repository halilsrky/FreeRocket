# SKYRTOS: Bitirme Projesi Sunumu, Posteri ve Fiziksel Gösterim Kılavuzu

Bu kılavuz, **SKYRTOS: FreeRTOS Tabanlı Gömülü Uçuş Bilgisayarı** bitirme projenizin jüri önündeki savunması için hazırlanmıştır. Jüriyi etkileyecek profesyonel bir sunum taslağı, akademik poster şablonu ve fiziksel kart ile yapacağınız **SIT** ve **SUT** testlerinin canlı gösterim senaryolarını içermektedir.

---

# 1. BÖLÜM: Slayt Slayt Sunum Rehberi (Jüriye Anlatım Stratejisi)

Sunumunuzun **15 slayt** civarında olması idealdir. Her slayt için ne yazılması gerektiği, hangi görselin (LaTeX klasöründeki şekiller) kullanılacağı ve jüri karşısında kurmanız gereken **teknik cümleler** aşağıda detaylandırılmıştır.

---

### SLAYT 1: Kapak ve Proje Kimliği
*   **Başlık:** SKYRTOS: FreeRTOS Tabanlı Model Roket Uçuş Bilgisayarı Aviyonik Yazılımı ve İşlemci-İçi-Döngü (PIL) Doğrulama Mimarisi
*   **Alt Başlık:** Bitirme Projesi II — Final Savunması
*   **Hazırlayan:** Halil Sarıkaya
*   **Danışman:** [Danışman Hocanızın Adı]
*   **Kurum:** İstanbul Sabahattin Zaim Üniversitesi, Mühendislik ve Doğa Bilimleri Fakültesi, Bilgisayar Mühendisliği Bölümü
*   **Görsel:** İZÜ Logosu ve kartınızın şık bir fotoğrafı veya CAD çizimi.
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Değerli hocalarım, bitirme projem kapsamında; model roketlerin milisaniye seviyesindeki katı gerçek zamanlı kısıtlarını karşılayan, deterministik ve yüksek güvenilirlikli bir gömülü uçuş yazılımı geliştirdim. Sunumumda bu yazılımın mimarisini, sensör füzyon algoritmalarını ve bunu doğrulamak için kurduğum özgün test altyapısını aktaracağım."*

---

### SLAYT 2: Problem Tanımı ve Amaç (Neden RTOS?)
*   **Slayt İçeriği:**
    *   **Model Roket Aviyoniklerinde Kritik Zamanlama:** Apogee (tepe noktası) tespitinde 100 ms'lik gecikmenin bile roketin aşırı hızla yere çakılmasına veya yanlış kurtarma fazına yol açması.
    *   **Bare-Metal (Geleneksel) Yaklaşımın Sınırları:** Tek bir döngüde (`while(1)`) çalışan kodda, GNSS veya I2C gibi bir sensörün okuma gecikmesinin tüm sistemi bloke etmesi (blocking/jitter riski).
    *   **Neden Linux Değil?:** Genel amaçlı OS'lerin deterministik olmaması, context switch sürelerinin öngörülememesi.
    *   **Projenin Hedefleri:** Deterministik görev önceliklendirmesi, sensörlerin işlemciyi engellemeden (non-blocking) okunması ve gerçek uçuş koşullarının laboratuvarda fiziksel kart üzerinde test edilmesi.
*   **Görsel:** Roketin fırlatılışını veya olası bir kurtarma hatasını gösteren sembolik bir çizim.
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Aviyonik sistemlerde en büyük düşmanımız zamanlama belirsizliğidir (jitter). Geleneksel bare-metal yapılarda bir sensörün milisaniyeler süren gecikmesi, kurtarma paraşütünün açılmamasına yol açar. Bu projede, bu problemi aşmak için donanımsal kesme (Interrupt) ve DMA mimarisini FreeRTOS'un öncelik tabanlı preemptive scheduler'ı ile birleştirdim."*

---

### SLAYT 3: Sistem Mimarisi ve Katmanlı Yapı
*   **Slayt İçeriği:**
    *   **4 Katmanlı Yazılım Mimarisi (Solid Architecture):**
        1.  *Donanım Katmanı:* STM32F446, BMI088, BME280, L86 GNSS.
        2.  *HAL (Hardware Abstraction Layer):* CubeMX ile üretilen standart API'ler (kodlara el ile müdahale edilmeden korunmuştur).
        3.  *Sürücü/Middleware:* FreeRTOS çekirdeği ve sensörlerin düşük seviyeli register sürücüleri.
        4.  *Uygulama Katmanı:* Durum kestirimleri (Mahony, Kalman) ve 7-Fazlı Uçuş Durum Makinesi (FSM).
    *   **Tek Yönlü Bağımlılık İlkesi:** Modülerlik ve taşınabilirlik.
*   **Görsel:** `sekiller/KatmanlıYazılımMimarisi(4 katman).png` (Rapor Şekil 3.1)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Geliştirdiğim yazılım tamamen modüler ve katmanlı bir mimariye sahiptir. CubeMX tarafından üretilen HAL kodlarına kesinlikle dokunmadım; tüm sürücüleri ve algoritmaları temiz kullanıcı dosyalarında soyutlayarak yazdım. Bu sayede donanım değişse bile üst seviyedeki uçuş algoritmalarımız aynen korunabilmektedir."*

---

### SLAYT 4: Donanım ve Çevresel Birim Haritası
*   **Slayt İçeriği:**
    *   **Mikrodenetleyici:** ARM Cortex-M4 @ 180 MHz, FPU (Floating Point Unit) desteği (sensör füzyon hızını katlayan kritik donanım).
    *   **Çevresel Birim Dağılımı:**
        *   *I2C1:* BMI088 IMU (Veri Hazır kesmeleri EXTI3 ve EXTI4 pinlerinden asenkron tetiklenir).
        *   *I2C3:* BME280 Barometre (10 Hz).
        *   *USART6 (Circular DMA RX):* L86 GNSS (1 Hz konum verisi).
        *   *USART2 (DMA TX/RX):* Telemetri ve SIT/SUT Test Arayüzü (115200 bps).
        *   *IWDG:* Bağımsız Donanımsal Watchdog (2 saniye timeout).
*   **Görsel:** Peripheral -> Pin -> Protokol tablosu (Rapordaki Tablo 3.1)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Uçuş bilgisayarımızda ARM Cortex-M4 işlemcisinin donanımsal FPU (kayan nokta birimi) özelliğinden sonuna kadar yararlandık. Tüm haberleşmeyi DMA kanalları üzerinden yaparak işlemcinin (CPU) sensör okumak için boşa dönmesini engelledik. Örneğin, GNSS verilerini işlemciye hiç yük olmadan dairesel (circular) DMA ile arka planda tampon belleğe aktarıyoruz."*

---

### SLAYT 5: FreeRTOS Görev (Task) Mimarisi ve İletişim
*   **Slayt İçeriği:**
    *   **Görevlerin Öncelik Sıralaması:**
        *   `imu_task` (`osPriorityHigh`): En yüksek öncelikli, anlık yönelim hesabı.
        *   `baro_task` (`osPriorityBelowNormal`): 10 Hz irtifa hesabı + Kalman + FSM.
        *   `gnss_task` (`osPriorityBelowNormal`): 1 Hz circular DMA parse.
        *   `telemetry_task` (`osPriorityBelowNormal`): 50 Hz telemetri paket yayını.
    *   **Zamanlama ve Senkronizasyon:**
        *   *Kesmeden Göreve (ISR -> Task):* `xTaskNotifyFromISR` ve `portYIELD_FROM_ISR` kalıbı (DRDY pini tetiklenir tetiklenmez `imu_task` anında çalışır).
        *   *Görevden Göreve (Task -> Task):* `Queue depth=1` + `xQueueOverwrite` ile atomik ve kilitsiz (lock-free) **Snapshot** mekanizması. Veri tutarsızlığı (race condition) tamamen engellenmiştir.
*   **Görsel:** `sekiller/FreeRTOSGörevMimarisi.png` (Rapor Şekil 3.2)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Görev mimarimizde preemptive zamanlama kullandık. IMU verisi geldikçe en yüksek öncelikli görevimiz diğer tüm görevleri yarıda keserek yönelim günceller. Görevler arası veri paylaşımında ise mutex veya karmaşık kilitler yerine, derinliği 1 olan kuyruklar üzerinden çalışan 'Snapshot' mekanizmasını kurdum. Bu sayede hiçbir görev veri yarışına (race condition) girmeden en güncel sensör verisini okur."*

---

### SLAYT 6: Sensör Sürücüleri ve Veri İşleme Hattı (Pipeline)
*   **Slayt İçeriği:**
    *   **BMI088 IMU Sürücüsü:** I2C tabanlı, asenkron okuma. İvmeölçer ve Jiroskop veri hazır kesmeleriyle tetiklenen DMA okuma durum makinesi (DMA State Machine: IDLE -> READING_ACC -> READING_GYRO).
    *   **BME280 Barometre Sürücüsü:** `vTaskDelayUntil` ile tam 100 ms periyot garantisi. Bosch kompanzasyon formülleri ve başlangıçtaki ilk N örnek ortalamasıyla `alt_ref` (yer sıfırı) kalibrasyonu.
    *   **GNSS Modülü:** NMEA `GPRMC` ve `GPGGA` cümlelerinin hızlı sscanf ayrıştırması. Uydudan fix yoksa telemetride geçersiz kılınma koruması.
*   **Görsel:** `sekiller/IMU_Pipeline.png` ve `sekiller/Baro TaskAnaDöngüsü_veKalman_entegrasyonu.png` (Rapor Şekil 3.3 ve 3.4)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Sensör sürücülerimizi yazarken donanımsal özellikleri sonuna kadar kullandık. BMI088 için yazdığım sürücü, kesme geldiğinde sırasıyla ivme ve jiroskop verilerini asenkron DMA ile çeken bir Durum Makinesi işletir. Barometre sürücümüz ise açılışta ilk saniyelerde ortam basıncını örnekleyerek deniz seviyesinden bağımsız olarak rokete göreceli bir yer sıfır noktası (alt_ref) kalibre eder."*

---

### SLAYT 7: Sensör Füzyonu I — Ataletsel Yönelim (Mahony AHRS)
*   **Slayt İçeriği:**
    *   **Euler Açıları ve Gimbal Lock Problemi:** Klasik Euler açılarının 90 derecede kilitlenme riski -> Çözüm: **Quaternion (Dörtlü) Temsili**.
    *   **Mahony AHRS Mekanizması:**
        *   Tahmini yerçekimi doğrultusunun quaternion üzerinden hesabı.
        *   İvmeölçer ve tahmini yerçekimi arasındaki vektörel çarpım hatasının ($\mathbf{e} = \hat{\mathbf{g}} \times \mathbf{a}_{norm}$) PI kontrolcü ile jiroskop driftini düzeltmek için geri beslenmesi.
    *   **Optimizasyon:** İşlemci FPU'suna rağmen ek yükü azaltmak için Quake III hızlı karekök tersi (`inv_sqrt`) algoritması kullanımı.
*   **Görsel:** Mahony Hata ve Quaternion Diferansiyel Denklemleri (LaTeX)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Roketin 3D uzaydaki yönelimini hesaplarken Gimbal Lock kilitlenmesinden kaçınmak için Quaternion matematiği kullandık. Mahony AHRS filtresi, jiroskobun zamanla biriken kaymasını (drift), ivmeölçerden aldığı yerçekimi vektörüyle anlık karşılaştırıp bir PI kontrolcü üzerinden düzelterek çalışır. Matematiksel işlemleri hızlandırmak için ise ünlü Quake III hızlı karekök tersi algoritmasını gömdük."*

---

### SLAYT 8: Sensör Füzyonu II — Yükseklik Kestirimi (Kalman Filtresi)
*   **Slayt İçeriği:**
    *   **Neden Filtre?:** Barometrenin gürültülü yapısı ve motor ateşlendiğindeki ani basınç dalgalanmaları (gürültü).
    *   **3-Durumlu Kalman Filtresi (3-State Altitude KF):**
        *   *Durum Vektörü:* $\mathbf{x} = [\text{irtifa}, \text{hız}, \text{ivme\_bias}]^T$
        *   *Sistem Geçiş Modeli ($F$):* Sabit ivmeli hareket modeli.
        *   *Gözlem Modeli ($H$):* Sadece Barometre İrtifası ve IMU İvmesi.
    *   **Filtre Tuning:** $r\_alt = 5.0$ (baro varyansı), $r\_acc = 10.0$ (ivme varyansı).
*   **Görsel:** $F$, $H$, $Q$ matris tanımları ve `sekiller/KalmanFiltresi.png` (Rapor Şekil 3.5)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Barometre verileri kalkış sırasındaki yüksek şok, titreşim ve aerodinamik basınçtan dolayı aşırı gürültülüdür. Bu gürültüyü sönümleyip dikey hızı doğru kestirmek amacıyla 3 durumlu bir Kalman filtresi tasarladım. Filtre, durum vektöründe sadece irtifayı değil, dikey hızı ve ivmeölçerin bias (sapma) değerini de tahmin ederek rokete pürüzsüz bir irtifa ve sıfır gecikmeli dikey hız profili sunar."*

---

### SLAYT 9: Uçuş Durum Makinesi (7-Faz FSM)
*   **Slayt İçeriği:**
    *   **7 Fazlı Deterministik Yapı:** IDLE -> BOOST -> COAST -> APOGEE -> DROGUE_DESCENT -> MAIN_DESCENT -> LANDED.
    *   **Kritik Geçiş Koşulları:**
        *   *BOOST (Fırlatma):* IMU İvmesi $> 45\ m/s^2$ (veya IMU hatasında baro hızı $> 15\ m/s$).
        *   *COAST (Motor Yanma Sonu):* Kalman dikey ivmesinin aralıksız 5 örnek ($500\ ms$) boyunca $< 0$ olması veya acil durum $8\ s$ zaman aşımı.
        *   *APOGEE (Tepe Noktası - Çift Güvenlik):* Kalman dikey hızının $< 0$ olması ve eğim açısının (tilt) $> 70^\circ$ olması (Drogue paraşüt tetiklenir).
        *   *MAIN DESCENT (Ana Paraşüt):* Göreli irtifanın $300\ m$ altına inmesi.
        *   *LANDED (İniş):* Dikey hızın 3 saniye boyunca $|v| < 2\ m/s$ olması.
*   **Görsel:** `sekiller/UçuşDurumMakinesi.png` (Rapor Şekil 3.6)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Uçuş durum makinemiz tamamen deterministiktir. Güvenliği en üst düzeye çıkarmak için Apogee (tepe noktası) tespitinde çift kanallı bir koruma uyguladım: Hem Kalman dikey hızının sıfırın altına düştüğünü kontrol ediyoruz, hem de Mahony'den gelen eğim açısının 70 dereceyi aştığını görerek acil durum tetiklemesi yapabiliyoruz. Bu sayede roket tepe noktasını asla kaçırmıyor."*

---

### SLAYT 10: Güvenilirlik Mekanizmaları ve Degraded Mod
*   **Slayt İçeriği:**
    *   **IWDG (Independent Watchdog):** 2 saniye timeout. En kritik görev olan `baro_task` (100 ms) tarafından beslenir. Görev kilitlenirse sistem 2 sn içinde donanımsal reset atarak kurtarma durumuna geçer.
    *   **Stack Overflow Koruması:** `configCHECK_FOR_STACK_OVERFLOW 2` ile RAM canarisi aktiftir; taşma anında `vApplicationStackOverflowHook` tetiklenerek sistem güvenli moda resetlenir.
    *   **IMU Başarısızlığında Baro-Only (Kısıtlandırılmış/Degraded) Mod:**
        *   Açılışta IMU arızalanır veya I2C hattı koparsa, 3 denemeden sonra `imu_task` tamamen silinir.
        *   Sistem uçuşu iptal etmez; barometre Kalman hızıyla fırlatmayı tespit eder ($> 15\ m/s$) ve uçuş FSM'ini baro-only modda başarıyla tamamlar (Tilt-based apogee devre dışı kalır ama hız-bazlı apogee aktif kalır).
*   **Görsel:** degraded modu özetleyen IMU Var / IMU Yok karşılaştırma tablosu (CLAUDE.md'deki tablo)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Aviyonik yazılımda hata kaçınılmazdır, önemli olan hatayı tolere edebilmektir. Eğer uçuş öncesinde veya sırasında IMU sensörümüz tamamen bozulursa, yazılımımız bunu algılar, IMU görevini sonlandırır ve kendini 'Baro-Only Degraded' moda alır. Bu modda roket, fırlatmayı ve tepe noktasını sadece barometrik Kalman çıktısıyla tespit ederek görevini kısıtlı da olsa başarıyla tamamlayabilir."*

---

### SLAYT 11: Doğrulama Metodolojisi I — SIT (Sensör İzleme Testi)
*   **Slayt İçeriği:**
    *   **Amaç:** Sürücülerin, I2C kesmelerinin ve UART telemetri hattının fiziksel olarak doğrulanması.
    *   **Kurulum:** STM32 Nucleo Kartı + USB-UART Dönüştürücü + Python SIT GUI (customtkinter).
    *   **Fonksiyonlar:**
        *   Sensörlerin ham ve fiziksel birimdeki verilerinin canlı 50 Hz grafiği.
        *   Mahony çıktılarının Python'daki 3D Roket Yönelim modeli üzerinde anlık görselleştirilmesi.
    *   **Sonuç:** Statik testte yerçekimi ivmesinin Z ekseninde tam $-9.81\ m/s^2$ okunması, roll-pitch kararlılığının sıfır drift ile doğrulanması.
*   **Görsel:** `sekiller/SIT_testi_arayüz.png` (Rapor Şekil 3.7)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Doğrulama aşamasının ilk adımı olan Sensör İzleme Testi (SIT) ile donanımsal sürücülerimizi doğruladık. Yazdığımız Python GUI arayüzü sayesinde, kartı elimizde hareket ettirdiğimizde Mahony filtresinin hesapladığı Euler açılarını 3 boyutlu roket modeli üzerinde gecikmesiz ve akıcı bir şekilde görselleştirdik. Böylece donanım-yazılım haberleşmesini tamamen doğruladık."*

---

### SLAYT 12: Doğrulama Metodolojisi II — SUT (Sentetik Uçuş Testi / PIL)
*   **Slayt İçeriği:**
    *   **Temel Felsefe (Processor-in-the-Loop):** Uçuş algoritmalarını ve FSM'i simüle edilmiş gerçekçi uçuş verisiyle doğrulamak.
    *   **SUT Modu Akışı:**
        *   `sys_mode_set(MODE_SUT)` komutuyla gerçek sensör okuyan tüm görevler (IMU, Baro, GNSS) uyutulur (portMAX_DELAY). İşlemci tamamen `sut_task` modülüne kalır.
        *   PC simülasyon verisini (ivme batch + baro) UART üzerinden gönderir.
        *   STM32 gerçek Mahony, Kalman ve FSM algoritmalarını bu verilerle koşturur.
        *   Hesaplanan filtre ve faz sonuçlarını 26 byte'lık `SUT_RESPONSE` paketiyle PC'ye geri yollar.
    *   **RocketPy Simülasyonu:** Açık kaynak uçuş simülatöründen elde edilen 51.5 saniyelik gerçekçi uçuş CSV'si veri kaynağı olarak kullanılır.
    *   **Gerçek Zaman Pacing Kaldırılması:** Paket gönder-cevap al döngüsüyle 51.5 saniyelik uçuş testini fiziksel işlemci üzerinde **2 saniyeden kısa sürede** bitirme imkanı!
*   **Görsel:** `sekiller/SUT_SistemMimarisi.png` (Rapor Şekil 3.8)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Algoritmaları doğrulamak için en özgün çalışmamız Processor-in-the-Loop, yani Sentetik Uçuş Testi (SUT) altyapısıdır. Bu modda, RocketPy simülatöründen aldığımız gerçekçi uçuş verisini PC üzerinden seri haberleşmeyle mikrodenetleyiciye besliyoruz. STM32 kendi sensörlerini okumayı durdurup, tamamen bu simüle veriyi gerçek zamanlıymış gibi filtrelerinden ve uçuş durum makinesinden geçirir. Gerçek zaman sınırlamasını kaldırarak, 51 saniyelik uçuşu kart üzerinde 2 saniyenin altında koşturup tüm kararları test edebiliyoruz."*

---

### SLAYT 13: Bulgular I — Gerçek Zamanlılık ve Görev Performansı (SystemView)
*   **Slayt İçeriği:**
    *   **SEGGER SystemView Analizi:** JTAG/canlı bağlantı olmadan RAM ring buffer üzerinden Post-Mortem görev analizi.
    *   **100 ms'lik Kritik Kesit Sonuçları:**
        *   `bmiTask` (IMU): 331 aktivasyon, toplam çalışma süresi çok düşük, **%2.39 CPU Yükü**.
        *   `bmeTask` (Baro): 18 aktivasyon, **%0.47 CPU Yükü**.
        *   `gnssTask`: **%0.06 CPU Yükü**.
        *   `Idle Task` (İşlemcinin boşta kalma oranı): **%96.55**.
    *   **Sonuç:** Tüm algoritmalar devredeyken dahi işlemci neredeyse tamamen boştadır. Donanım ve yazılım optimizasyonunun (DMA + kesme tabanlı asenkron mimarinin) gücü kanıtlanmıştır. Hiçbir görev deadline kaçırmamıştır.
*   **Görsel:** SystemView Timeline ekran görüntüsü ve Rapor Tablo 4.1 (Görev CPU Yükleri Tablosu)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Gömülü sistemimizin gerçek zamanlı başarımını doğrulamak için SEGGER SystemView ile post-mortem analiz yaptık. Sonuçlar inanılmaz derecede başarılı: En yoğun çalışan IMU görevimiz bile CPU'nun sadece %2.39'unu tüketiyor. İşlemcimiz uçuş sırasında %96.55 oranında boştadır. Bu, yazdığımız asenkron DMA mimarisinin işlemciyi ne kadar rahatlattığının ve sistemin hard real-time kısıtları sıfır jitter ile karşıladığının en net kanıtıdır."*

---

### SLAYT 14: Bulgular II — Algoritma Başarımı ve SUT Çıktıları
*   **Slayt İçeriği:**
    *   **Kalman Filtresi Gürültü Sönümleme:** SUT testinde ham barometre verisindeki salınımların Kalman filtresi tarafından kusursuz pürüzsüzleştirilmesi ve sıfır gecikmeli dikey hız tahmini.
    *   **Mahony AHRS Kararlılığı:** 300 saniyelik statik testte Roll ve Pitch açılarında 0.0 derece drift. Mıknatıssız (manyetometresiz) tasarımdan dolayı Yaw açısındaki birikimli kaçınılmaz jiroskop kayması (drift) tespiti ve bunun jüriye dürüstçe sunulması.
    *   **7-Faz FSM Geçiş Tutarlılığı:** RocketPy'daki teorik fırlatma, tepe noktası, paraşüt açılma anları ile STM32 kartının bunları milisaniyelik hassasiyetle yakaladığı zaman serisi dikey çizgileri.
*   **Görsel:** `sekiller/sut_kalman_pürüssüz.png` (Rapor Şekil 4.3)
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"SUT test çıktılarında, kırmızı renkli dalgalı ham barometre irtifasının mavi renkli Kalman çıktısı tarafından nasıl pürüzsüzleştirildiğini görüyorsunuz. En önemlisi, FSM durum makinemizin RocketPy teorik verisiyle tam zamanlı örtüşmesidir. Kalkış ivmesi, burnout motor sönümlenmesi ve tepe noktasında dikey hızın sıfır olduğu an kart tarafından milisaniyelik farklarla yakalanmış ve paraşütler tam zamanında tetiklenmiştir."*

---

### SLAYT 15: Tartışma, Sonuç ve Gelecek Çalışmalar
*   **Slayt İçeriği:**
    *   **Genel Sonuçlar:**
        *   FreeRTOS mimarisiyle deterministik uçuş yazılım altyapısı kuruldu.
        *   DMA ve kesmelerle sıfır engellemeli (non-blocking) veri işleme hattı doğrulandı.
        *   PIL (SUT) altyapısı sayesinde algoritmalar uçuş öncesi %100 test edilebilir kılındı.
    *   **Gelecek Önerileri / Çalışmalar:**
        *   *Gyro Kalibrasyonu:* Açılışta ilk N örnekle jiroskop bias (sapma) değerlerinin otomatik çıkarılması (sıfırlanması).
        *   *LoRa Entegrasyonu:* Donanımda hazır olan UART4 DMA TX hattına LoRa E22 sürücüsünün yazılması.
        *   *HIL (Hardware-in-the-Loop):* Gerçek aktüatör ve piroteknik yüklerin dahil edildiği fiziksel donanım döngülü testler.
*   **Jüriye Söylenecek Anahtar Cümle:**
    > *"Sonuç olarak projemiz; sadece çalışan bir kart değil, aynı zamanda bu kartı laboratuvarda uçurabilen komple bir yazılımsal ve algoritmik doğrulama platformu sunmaktadır. İşlemcimizdeki %96'lık boş alan, gelecekte sisteme ekleyeceğimiz LoRa telemetri, SD kart loglama ve daha gelişmiş durum kestirim algoritmaları için bize muazzam bir esneklik tanımaktadır. Beni dinlediğiniz için teşekkür ederim, sorularınızı bekliyorum."*

---

# 2. BÖLÜM: Akademik Poster Yapısı (A0 Boyutu)

Posteriniz, jürinin ve fuardaki ziyaretçilerin **ilk bakışta projenin mühendislik kalitesini** anlayacağı şekilde tasarlanmalıdır. A0 boyutundaki bir poster için yerleşim planı ve içerik şeması şöyledir:

```
+-----------------------------------------------------------------------------------------+
|                                      POSTER BAŞLIĞI                                     |
|    SKYRTOS: FreeRTOS Tabanlı Gömülü Uçuş Bilgisayarı ve PIL (SUT) Doğrulama Platformu   |
|                      Halil Sarıkaya  -  Danışman: [Hocanın Adı]                         |
+------------------------------------+----------------------------------------------------+
|   SOL SÜTUN: MİMARİ VE YAZILIM      |   ORTA SÜTUN: ALGORİTMALAR VE FSM                  |
|                                    |                                                    |
|   1. GİRİŞ VE PROBLEM TANIMI       |   4. SENSÖR FÜZYONU (ATALETSEL YÖNELİM)            |
|   Model roket uçuş yazılımlarında   |   * Mahony AHRS (6DOF Quaternion) denklemleri.     |
|   determinizm ve sert gerçek       |   * Gimbal Lock probleminin aşılması.              |
|   zamanlılık kısıtları.            |   * inv_sqrt (Quake III) optimizasyonu.            |
|   Bare-metal kısıtları.            |   [GÖRSEL: Mahony Akış Şeması / IMU Pipeline]      |
|                                    |                                                    |
|   2. KATMANLI YAZILIM MİMARİSİ     |   5. DURUM KESTİRİMİ (YÜKSEKLİK KALMAN)            |
|   Donanım, HAL, Sürücüler ve       |   * 3-Durumlu Kalman Filtresi. Durum vektörü,      |
|   Uygulama katmanlarının           |     F, H ve Q matris tanımları.                    |
|   ayrımı. Tek yönlü bağımlılık.    |   * Baro ve IMU varyanslarının dinamik füzyonu.    |
|   [GÖRSEL: 4 Katmanlı Mimari Şekli] |   [GÖRSEL: Kalman Filtresi Çevrim Şeması]          |
|                                    |                                                    |
|   3. FreeRTOS GÖREV ZAMANLAMASI    |   6. UÇUŞ DURUM MAKİNESİ (7-FAZ FSM)               |
|   * Görev öncelikleri ve stack'ler. |   * IDLE -> BOOST -> COAST -> APOGEE ->            |
|   * ISR -> Task notification.       |     DROGUE -> MAIN -> LANDED geçiş kriterleri.      |
|   * Depth=1 Queue ile Snapshot.    |   * IMU arızasında Baro-Only Degraded Mod.         |
|   [GÖRSEL: Task İletişim Şeması]   |   [GÖRSEL: FSM Geçiş Diyagramı]                    |
+------------------------------------+----------------------------------------------------+
|   SAĞ SÜTUN: TEST, DOĞRULAMA VE BULGULAR                                                |
|                                                                                         |
|   7. SIT (SENSÖR İZLEME TESTİ) CANLI DOĞRULAMA                                          |
|   * Canlı donanım sensörlerinin 50 Hz telemetriyle Python GUI arayüzüne aktarılması.    |
|   * 3D Roket yönelim görselleştirmesi ile Mahony çıktılarının statik doğrulaması.       |
|   [GÖRSEL: SIT Python GUI Ekran Görüntüsü]                                              |
|                                                                                         |
|   8. SUT / PIL (İŞLEMCİ-İÇİ-DÖNGÜ) TEST METODOLOJİSİ                                    |
|   * RocketPy simülasyon verilerinin UART üzerinden STM32 işlemcisine beslenmesi.        |
|   * Gerçek algoritmaların koşturulup PyQtGraph canlı arayüzünde doğrulanması.           |
|   * 51.5 saniyelik uçuş profilinin kart üzerinde 2 saniyenin altında test edilmesi.     |
|   [GÖRSEL: SUT Sistem Mimarisi Şeması]                                                  |
|                                                                                         |
|   9. BULGULAR VE ANALİZLER                                                              |
|   * SystemView Analizi: İşlemci yükü sadece %3.45 (Idle: %96.55).                       |
|   * Kalman Sonucu: Ham barometrik gürültünün pürüzsüzleştirilmesi grafiği.              |
|   * SUT Sonucu: FSM faz geçişlerinin RocketPy ile tam zamanlı örtüşmesi.                |
|   [GÖRSEL: SystemView Timeline]  -  [GÖRSEL: Kalman Ham/Filtreli İrtifa Karşılaştırması] |
|                                                                                         |
|   10. SONUÇ VE GELECEK ÖNERİLER                                                         |
|   * RTOS mimarisinin havacılıkta güvenilirlik başarımı kanıtlandı.                      |
|   * Gelecek çalışmalar: LoRa entegrasyonu, otomatik Gyro kalibrasyonu ve HIL testleri.  |
+-----------------------------------------------------------------------------------------+
```

---

# 3. BÖLÜM: Canlı Fiziksel Gösterim ve Test Senaryoları (Demo Planı)

Jüri masasının üzerine **Nucleo kartınızı** koyup bilgisayara bağladığınızda yapacağınız **canlı gösterim (demo) senaryosu**, projenizin geçerliliğini kanıtlayacak en önemli aşamadır. Aşağıdaki 3 aşamalı planı birebir uygulayınız.

---

### HAZIRLIK AŞAMASI
1.  **Nucleo Kartını Bağlayın:** ST-Link USB kablosunu bilgisayarınıza takın. (USART2 ST-Link üzerinden sanal COM port olarak PC'ye bağlıdır).
2.  **Arayüzü Başlatın:** Bilgisayarınızda `SIT_SUT/sut_tool/main.py` dosyasını çalıştırın.
3.  **Portu Seçin:** Arayüzden Nucleo kartınızın bağlı olduğu COM portunu (örneğin COM3) seçin ve baudrate'i **115200** olarak ayarlayın.

---

### GÖSTERİM 1: SIT (Sensör İzleme Testi) — Canlı Sensör ve Yönelim Gösterimi
Jüriye kartın donanımsal olarak kusursuz çalıştığını ve anlık yönelim (Mahony) ürettiğini gösterin.

*   **Adım 1:** Arayüzden **"SIT Modu"** veya **"Start SIT"** butonuna basın. (Python kartınıza seri porttan `CMD_NORMAL` veya benzeri normal mod komutunu gönderir).
*   **Adım 2:** Kartın üzerindeki yeşil durum LED'inin yandığını ve arayüzdeki canlı grafiklerin akmaya başladığını jüriye gösterin.
*   **Adım 3 (Kartı Elinize Alın):**
    *   Kartı yavaşça **Roll ve Pitch eksenlerinde** eğip bükün.
    *   Arayüzdeki **3D Roket Görselleştirmesinin** elinizdeki kartla birebir, sıfır gecikmeyle döndüğünü jüriye izletin.
    *   *Jüriye Açıklama:* *"Hocalarım, şu an kartımız canlı olarak BMI088 IMU sensörünün kesme pinlerinden tetiklenip verileri DMA ile okuyor ve 180 MHz'lik STM32 işlemcimizde Mahony AHRS filtresini çalıştırıyor. Quaternion tabanlı yönelim anında hesaplanıp 50 Hz hızında arayüze aktarılıyor."*
*   **Adım 4 (Statik Test ve Yerçekimi):**
    *   Kartı düz bir şekilde masaya bırakın.
    *   Arayüzdeki ivme grafiklerinde Z ekseninin tam olarak $-9.81\ m/s^2$ (veya $-1g$) gösterdiğini, X ve Y eksenlerinin sıfıra yakın olduğunu jüriye işaret edin.
    *   *Jüriye Açıklama:* *"Kart masada dururken Roll ve Pitch açılarının 0 derecede tamamen sabit kaldığını, hiçbir sapma (drift) yapmadığını görebilirsiniz. Bu, Mahony filtresinin yerçekimi vektörünü kullanarak jiroskop sapmalarını başarıyla sönümlediğini gösterir."*
*   **Adım 5 (Barometre ve İrtifa):**
    *   Parmağınızı BME280 barometre sensörünün üzerine hafifçe bastırıp bırakın veya kartı masadan yukarı kaldırın.
    *   Arayüzdeki göreceli (relative) irtifanın değiştiğini gösterin.

---

### GÖSTERİM 2: SUT (Sentetik Uçuş Testi) — Processor-in-the-Loop Simülasyonu
Jüriye, uçuş algoritmalarınızın ve 7-Fazlı FSM'in gerçek uçuş koşullarında nasıl hatasız karar verdiğini kanıtlayın.

*   **Adım 1:** Arayüzden **"SUT Modu"** butonuna basın.
    *   *Jüriye Açıklama:* *"Şimdi en önemli doğrulama aşamamız olan Processor-in-the-Loop (SUT) moduna geçiyoruz. Bu komutla karttaki tüm fiziksel sensör okuma görevlerini askıya alıyoruz ve işlemciyi tamamen dışarıdan besleyeceğimiz simülasyon verilerine açıyoruz."*
*   **Adım 2:** Arayüzden **"Load CSV"** diyerek `SIT_SUT/scenario_01_az_altitude_gurultu.csv` (veya gürültülü senaryolardan birini) dosyasını yükleyin.
*   **Adım 3:** **"Run SUT Simulation"** butonuna basın.
*   **Adım 4 (Simülasyon Hızını Vurgulayın):**
    *   Ekrandaki progress bar'ın saniyeler içinde %100'e ulaştığını gösterin.
    *   *Jüriye Açıklama:* *"Şu an RocketPy'dan alınan 51.5 saniyelik uçuş verisi seri port üzerinden paket paket işlemciye beslendi, işlemci gerçek filtreleri koşturdu ve sonuçları geri gönderdi. Gerçek zaman sınırlamasını kaldırdığımız için, tüm bu uçuşu fiziksel işlemci üzerinde sadece 1.5 saniyede baştan sona simüle ettik ve doğruladık!"*
*   **Adım 5 (Grafik Üzerinde Algoritma Başarımı Gösterimi):**
    *   **Kalman Filtresi Başarımı:** Grafikteki kırmızı (Ham Baro) ve mavi (Kalman Filtreli İrtifa) çizgileri gösterin. Kırmızı çizginin ne kadar gürültülü dalgalandığını, mavi çizginin ise ne kadar pürüzsüz aktığını jüriye izah edin.
    *   **FSM Faz Geçişleri (Dikey Çizgiler):** Grafikteki dikey kesikli çizgileri (IDLE, BOOST, COAST, APOGEE, DROGUE, MAIN, LANDED) işaret edin.
    *   *Jüriye Açıklama:* *"Grafikte görebileceğiniz üzere; motor ateşlendiğinde BOOST fazına geçiş kararı, motor bittiğinde COAST kararı ve tam tepe noktasında (hızın sıfırlandığı an) APOGEE kararı milisaniyelik bir gecikmeyle yakalanmış ve drogue paraşüt tetiklenmiştir. İnişte ise roket yer hizasından tam 300 metreye ulaştığında ana paraşüt (MAIN) açılmış ve yere sıfır hızla indiğinde LANDED fazına girilerek uçuş başarıyla tamamlanmıştır."*
*   **Adım 6 (3D Replay):** Arayüzün 3D Roket Replay özelliğini kullanarak, simülasyon boyunca roketin 3D yöneliminin fırlatılış, tepe taklak olma ve iniş fazlarındaki hareketlerini animasyonlu olarak jüriye oynatın.

---

### GÖSTERİM 3: Güvenilirlik ve Hata Tolerans Testi (Degraded Mod - Sözel veya Simüle)
Jüriye, sisteminizin arıza anında nasıl hayatta kaldığını (Fail-Safe) anlatın.

*   *Jüriye Açıklama:*
    > *"Hocalarım, sistemin güvenilirliğini test etmek için donanımsal arıza senaryoları da kurguladık. Örneğin açılışta IMU sensör hattı koparsa, yazılımımız 3 kez dener, başarısız olduğunu anlar ve `imu_task` görevini sistemden siler. Sistem 'Baro-Only Degraded' modda çalışmaya devam eder. SUT testlerimizde sadece barometre verisi beslediğimizde dikey ivme 0 kabul edilir; durum makinesi fırlatmayı baro hızı $> 15\ m/s$ olduğunda algılar, tepe noktasını hız-bazlı olarak yine başarıyla bulur ve paraşütleri açar. Böylece sensör kaybına rağmen roketimiz kurtarılır."*

---

# Jüri Üyelerinin Sorabileceği Olası Zor Sorular ve Cevapları

1.  **Soru: Neden Mahony AHRS seçtiniz? Extended Kalman Filter (EKF) daha kararlı değil midir?**
    *   *Cevap:* "Evet hocam, EKF teorik olarak daha yüksek dinamik doğruluk sunar. Ancak EKF'in 6 eksende gerektirdiği matris çarpımları ve Jacobi matrisi hesap yükü, mikrodenetleyicimizin CPU yükünü aşırı derecede artıracaktır. Mahony filtresi ise PI kontrolcü yapısı sayesinde çok az hesaplama gücüyle (sadece %2.39 CPU yükü) neredeyse EKF'e yakın bir roll-pitch doğruluğu sunar. İşlemci kaynaklarımızı optimize kullanmak adına Mahony'yi tercih ettik ve Quake III hızlı karekök algoritmasıyla daha da hafiflettik."
2.  **Soru: Telemetri yayını 50 Hz iken neden Barometre okuması 10 Hz? Bu bir çelişki değil mi?**
    *   *Cevap:* "Çelişki değil aksine bilinçli bir mimari karardır hocam. Barometre sensörünün (BME280) fiziksel algılama ve tepki süresi IMU'ya göre oldukça yavaştır; 10 Hz'in üzerinde okuma yapmak sadece gürültüyü artırır. IMU ise kesme tabanlı asenkron olarak çok daha yüksek frekansta veri üretir. Telemetri görevimizin 50 Hz çalışması, IMU'dan gelen yüksek hızlı Mahony yönelim açılarını yer istasyonuna en akıcı ve kesintisiz şekilde ulaştırabilmek içindir."
3.  **Soru: SUT modunun normal yazılımdan farkı nedir? Test ederken gerçek algoritmayı değiştirmek zorunda kalmıyor musunuz?**
    *   *Cevap:* "Kesinlikle hayır hocam. SUT modunun en büyük gücü, **gerçek uçuş yazılımını ve algoritmalarını birebir koşturmasıdır**. Tek fark şudur: Normal modda Mahony ve Kalman filtrelerinin girdileri fiziksel I2C sürücülerinden gelirken, SUT modunda bu sürücü görevleri FreeRTOS tarafından uyutulur ve girdiler UART üzerinden gelen simüle paketlerden beslenir. Filtreler (`mahony_update`, `alt_kalman_update`) ve durum makinesi (`flight_sm_update`) tek bir satır dahi değiştirilmeden aynı kod segmentiyle çalıştırılır. Bu sayede test ettiğimiz şey kodun kendisidir."
4.  **Soru: Watchdog neden barometre görevi tarafından besleniyor? Neden daha yüksek öncelikli IMU görevi beslemiyor?**
    *   *Cevap:* "Çok kritik bir soru hocam. Watchdog'u en yüksek öncelikli görev besleseydi, düşük öncelikli bir görev (örneğin baro veya durum makinesi görevi) kilitlendiğinde veya stack overflow yaşadığında yüksek öncelikli görev çalışmaya devam edebilir ve sistem kilitlendiği halde Watchdog beslenmeye devam edebilirdi. Watchdog'u daha düşük öncelikli olan ve 100 ms periyotla çalışan `baro_task` içerisinden besleyerek, sistemdeki herhangi bir görevin veya planlayıcının kilitlenmesi durumunda Watchdog'un beslenememesini ve sistemin güvenli bir şekilde sıfırlanmasını garanti altına aldık."
