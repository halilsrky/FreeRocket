# Diyagram 3 — FreeRTOS Görev Mimarisi

Bölüm 3.3 için. Görev öncelikleri, tetikleme mekanizmaları ve aralarındaki iletişim kanalları.

```mermaid
flowchart TD
    subgraph ISR["ISR Katmanı (Kesme Bağlamı)"]
        EXTI3["EXTI3_IRQHandler\nPB3 — ACC DRDY"]
        EXTI4["EXTI4_IRQHandler\nPB4 — GYRO DRDY"]
        DMA_CB["HAL_I2C_MemRxCpltCallback\nI2C1 DMA Done"]
        ERR_CB["HAL_I2C_ErrorCallback\nI2C1 Hata"]
    end

    subgraph TASKS["FreeRTOS Görevleri"]
        IMU["IMU Task\nosPriorityHigh\nStack: 512×4 B\nTetik: xTaskNotifyWait"]
        BARO["Baro Task\nosPriorityBelowNormal\nStack: 512×4 B\nTetik: vTaskDelayUntil 100ms"]
        GNSS["GNSS Task\nosPriorityBelowNormal\nStack: 512×4 B\nTetik: DMA HT/TC Callback"]
        TEL["Telemetry Task\nosPriorityBelowNormal\nStack: 256×4 B\nTetik: vTaskDelay 20ms"]
        SUT["SUT Task\nosPriorityBelowNormal\nStack: 512×4 B\nTetik: UART RX paket"]
    end

    subgraph ALGO["Algoritma Servisleri (Task içinde çalışır)"]
        MAHONY["mahony_update()\nQuaternion AHRS"]
        KALMAN["alt_kalman_update()\nIrtifa Kalman"]
        FSM["flight_sm_update()\n7-Faz FSM"]
    end

    subgraph SNAP["Snapshot Queues (depth=1, QueueOverwrite)"]
        IMU_Q["imu_snapshot_q\nimu_snapshot_t"]
        BARO_Q["baro_snapshot_q\nbaro_snapshot_t"]
        ALT_Q["alt_snapshot_q\nalt_snapshot_t"]
        GNSS_Q["gnss_snapshot_q\ngnss_snapshot_t"]
        FSM_Q["flight_snapshot_q\nflight_snapshot_t"]
    end

    EXTI3 -- "xTaskNotifyFromISR\nNOTIFY_ACC_DRDY" --> IMU
    EXTI4 -- "xTaskNotifyFromISR\nNOTIFY_GYRO_DRDY" --> IMU
    DMA_CB -- "xTaskNotifyFromISR\nNOTIFY_DMA_DONE" --> IMU
    ERR_CB -- "xTaskNotifyFromISR\nNOTIFY_I2C_ERROR" --> IMU

    IMU --> MAHONY
    MAHONY --> IMU_Q

    BARO -- "I2C3 DMA read\nbme280_read()" --> KALMAN
    BARO -- "imu_snapshot_peek()" --> IMU_Q
    IMU_Q -.-> BARO
    KALMAN --> ALT_Q
    BARO --> BARO_Q
    BARO --> FSM
    FSM --> FSM_Q
    BARO -- "iwdg_feed()" --> IWDG[("IWDG\n~2s timeout")]

    GNSS -- "circular DMA RX\nNMEA parse" --> GNSS_Q

    TEL -- "xQueuePeek ×3" --> IMU_Q
    TEL -- "xQueuePeek" --> ALT_Q
    TEL -- "xQueuePeek" --> GNSS_Q
    TEL -- "UART2 DMA TX\n50 Hz" --> PC[("PC / Python GUI")]

    SUT -- "UART2 RX\nSUT_COMBINED" --> MAHONY
    SUT --> KALMAN
    SUT --> FSM
    SUT -- "UART2 TX\nSUT_RESPONSE" --> PC

    style ISR fill:#ffdddd,stroke:#cc0000
    style TASKS fill:#ddeeff,stroke:#0055cc
    style ALGO fill:#ddffdd,stroke:#007700
    style SNAP fill:#fff8dd,stroke:#cc8800
```

> **SUT modu:** `sys_mode_get() == MODE_SUT` olduğunda IMU, Baro, GNSS ve Telemetri görevleri `vTaskDelay(200ms)` döngüsüne girer. Algoritmalar (Mahony, Kalman, FSM) yalnızca `sut_task` üzerinden çalışır.
