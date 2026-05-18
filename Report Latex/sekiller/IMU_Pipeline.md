# Diyagram 4 — IMU Pipeline

Bölüm 3.3 ve 3.4.1 için. DRDY kesmesinden snapshot publish'e kadar tam IMU veri akışı.

```mermaid
flowchart TD
    A([imu_task başlar]) --> B{bmi088_init\n× 3 deneme}
    B -- Başarısız --> C([vTaskDelete])

    B -- Başarılı --> D["EXTI3/4 etkinleştir\nmahony_init()"]
    D --> WAIT["xTaskNotifyWait\n500 ms timeout"]

    WAIT -- "NOTIFY_ACC_DRDY\n(PB3 EXTI)" --> LA["bmi088_start_accel_dma()\ndma_state = READING_ACC"]
    WAIT -- "NOTIFY_GYRO_DRDY\n(PB4 EXTI)" --> LG["bmi088_start_gyro_dma()\ndma_state = READING_GYRO"]

    LA -- "NOTIFY_DMA_DONE" --> PA["bmi088_parse_accel()\nax,ay,az → m/s²\nacc_fresh = true"]
    LG -- "NOTIFY_DMA_DONE" --> PG["bmi088_parse_gyro()\ngx,gy,gz → rad/s\ngyro_fresh = true"]

    LA -- "NOTIFY_I2C_ERROR" --> WAIT
    LG -- "NOTIFY_I2C_ERROR" --> WAIT

    PA --> CHK{acc_fresh\n&& gyro_fresh?}
    PG --> CHK

    CHK -- Hayır --> WAIT
    CHK -- Evet --> F["mahony_update(ax,ay,az, gx,gy,gz, dt)\n→ quaternion, euler, θ_tilt"]
    F --> G["xQueueOverwrite(snapshot_q)\nacc_fresh = gyro_fresh = false"]
    G --> WAIT

    style C fill:#ffcccc,stroke:#cc0000
    style G fill:#ccffcc,stroke:#007700
```

> **Öncelik:** `acc_pending` her zaman `gyro_pending`'den önce işlenir; her iki DRDY aynı anda gelirse ivme önce okunur, böylece Mahony her iterasyonda güncel bir çift alır.
