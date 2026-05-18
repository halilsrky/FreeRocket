# Diyagram 7 — SUT (Sentetik Uçuş Testi) Sistem Mimarisi

Bölüm 3.8.2 için. Processor-in-the-Loop test sisteminin uçtan uca veri akışı.

```mermaid
sequenceDiagram
    participant PY as Python SerialWorker
    participant STM as STM32 sut_task

    PY->>STM: SUT_COMBINED [0xAD, N, batch_imu, baro]
    Note over STM: Mahony × N adım güncelle
    Note over STM: Kalman 1 adım güncelle
    Note over STM: flight_sm_update()
    STM-->>PY: SUT_RESPONSE [0xAE, sim_t, alt, rpy, status, chk]
    PY->>STM: SUT_COMBINED [sonraki pencere]
```

> **Gerçek zamanlı pacing yoktur:** Python her pencereyi gönderir ve cevap bekler. STM32 gerçek C algoritmalarını çalıştırdığından algoritma doğruluğu garanti altındadır; ancak zamanlama STM32 işlem hızına bağlıdır.
