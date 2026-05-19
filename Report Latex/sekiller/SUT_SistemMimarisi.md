# Diyagram 7 — SUT (Sentetik Uçuş Testi) Sistem Mimarisi

Bölüm 3.8.2 için. Processor-in-the-Loop test sisteminin uçtan uca veri akışı.

```mermaid
flowchart LR
    subgraph PC["PC (Python)"]
        ROCKETPY["RocketPy\nSimülatör\n(.csv üretir)"]
        CSV["sim_data.csv\nsim_time, ax, ay, az,\ngx, gy, gz, altitude"]
        WORKER["SerialWorker\n(QThread)"]
        PLOT["PyQtGraph\nPlotWidget\nirtifa + faz çizgileri"]
        GUI["PyQt5 GUI\nprogress bar\nfaz etiketleri"]

        ROCKETPY --> CSV
        CSV --> WORKER
        WORKER --> PLOT
        WORKER --> GUI
    end

    subgraph PACKET_DN["PC → STM32: SUT_COMBINED (0xAD)"]
        direction TB
        P1["Header: 0xAD"]
        P2["imu_count: N"]
        P3["IMU batch:\nN × [sim_t + gx+gy+gz]"]
        P4["baro: altitude + pressure"]
    end

    subgraph STM32["STM32 (sut_task)"]
        RECV["UART2 RX\npaket al"]
        BATCH["Mahony × N batch\nquaternion güncelle"]
        KF["alt_kalman_update()\nirtifa + hız hesapla"]
        SM["flight_sm_update()\nfaz geçiş kontrol"]
        SEND["UART2 TX\nSUT_RESPONSE"]
    end

    subgraph PACKET_UP["STM32 → PC: SUT_RESPONSE (0xAE)"]
        direction TB
        R1["Header: 0xAE"]
        R2["sim_time"]
        R3["filtered_alt"]
        R4["roll, pitch, yaw"]
        R5["flight_status (FSM_BIT)"]
        R6["checksum"]
    end

    subgraph SUT_SLEEP["SUT Modunda Uyuyan Görevler"]
        direction TB
        S1["imu_task → vTaskDelay(200ms)"]
        S2["baro_task → vTaskDelay(200ms)"]
        S3["gnss_task → vTaskDelay(200ms)"]
        S4["telemetry_task → vTaskDelay(200ms)"]
    end

    WORKER -- "SUT_COMBINED paket\nUSB-UART" --> RECV
    RECV --> BATCH
    BATCH --> KF
    KF --> SM
    SM --> SEND
    SEND -- "SUT_RESPONSE\nUSB-UART" --> WORKER

    PACKET_DN -.-> RECV
    SEND -.-> PACKET_UP

    style PC fill:#e8f4fd,stroke:#0066cc
    style STM32 fill:#fff8e8,stroke:#cc6600
    style SUT_SLEEP fill:#ffeeee,stroke:#cc4444
```

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
    Note over PY: 51.5 s uçuş < 2 s'de tamamlanır\n(gerçek zamanlı pacing yok)
```

> **Gerçek zamanlı pacing yoktur:** Python her pencereyi gönderir ve cevap bekler. STM32 gerçek C algoritmalarını çalıştırdığından algoritma doğruluğu garanti altındadır; ancak zamanlama STM32 işlem hızına bağlıdır.
