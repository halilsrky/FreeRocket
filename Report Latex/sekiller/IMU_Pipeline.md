# Diyagram 4 — IMU Pipeline: DMA Durum Makinesi

Bölüm 3.3 ve 3.4.1 için. DRDY kesmesinden snapshot publish'e kadar tam IMU veri akışı.

```mermaid
stateDiagram-v2
    [*] --> DMA_IDLE : bmi088_init() başarılı

    DMA_IDLE --> DMA_READING_ACC : acc_pending=true\nbmi088_start_accel_dma() == HAL_OK
    DMA_IDLE --> DMA_READING_GYRO : gyro_pending=true && !acc_pending\nbmi088_start_gyro_dma() == HAL_OK

    DMA_READING_ACC --> DMA_IDLE : NOTIFY_DMA_DONE\nbmi088_parse_accel() → ax,ay,az\nacc_fresh = true

    DMA_READING_GYRO --> DMA_IDLE : NOTIFY_DMA_DONE\nbmi088_parse_gyro() → gx,gy,gz\ngyro_fresh = true

    DMA_READING_ACC --> DMA_IDLE : NOTIFY_I2C_ERROR\nacc_pending = true (retry)
    DMA_READING_GYRO --> DMA_IDLE : NOTIFY_I2C_ERROR\ngyro_pending = true (retry)
```

```mermaid
flowchart TD
    A([Task başlangıcı]) --> B{bmi088_init}
    B -- "Başarısız × 3" --> C[vTaskDelete\nGörev silinir]
    B -- Başarılı --> D[bmi088_config\nEXTI3/4 etkinleştir\nmahony_init]

    D --> WAIT["xTaskNotifyWait\n(500 ms timeout)"]

    WAIT --> E{Bildirim\ntürü?}

    E -- "NOTIFY_ACC_DRDY\nPB3 EXTI" --> F[acc_pending = true]
    E -- "NOTIFY_GYRO_DRDY\nPB4 EXTI" --> G[gyro_pending = true]
    E -- "NOTIFY_DMA_DONE\nI2C1 DMA CB" --> H{dma_state?}
    E -- "NOTIFY_I2C_ERROR\nI2C1 Hata CB" --> ERR[acc/gyro_pending = true\ndma_state = IDLE]

    H -- DMA_READING_ACC --> I["bmi088_parse_accel()\nax,ay,az → m/s²\nacc_fresh = true\ndma_state = IDLE"]
    H -- DMA_READING_GYRO --> J["bmi088_parse_gyro()\ngx,gy,gz → rad/s\ngyro_fresh = true\ndma_state = IDLE"]

    F --> K{dma_state\n== IDLE?}
    G --> K
    I --> K
    J --> K
    ERR --> K

    K -- "Evet && acc_pending" --> L["bmi088_start_accel_dma()\ndma_state = READING_ACC"]
    K -- "Evet && gyro_pending\n&& !acc_pending" --> M["bmi088_start_gyro_dma()\ndma_state = READING_GYRO"]
    K -- Hayır --> N{acc_fresh\n&& gyro_fresh?}
    L --> N
    M --> N

    N -- Hayır --> WAIT
    N -- Evet --> O["dt hesapla\nmahony_update(gx,gy,gz,ax,ay,az,dt)"]
    O --> P["imu_snapshot_t doldur\n(ts, accel, gyro, q, euler, theta)"]
    P --> Q["xQueueOverwrite(snapshot_q)"]
    Q --> R["acc_fresh = false\ngyro_fresh = false"]
    R --> WAIT

    style C fill:#ffcccc,stroke:#cc0000
    style Q fill:#ccffcc,stroke:#007700
```

> **Öncelik notu:** `acc_pending` her zaman `gyro_pending`'den önce işlenir — her iki sensörde DRDY aynı anda gelirse ivme önce okunur, ardından jiroskop. Bu sayede Mahony her iterasyonda güncel bir çift alır.
