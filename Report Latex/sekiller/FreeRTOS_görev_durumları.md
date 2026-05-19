# Diyagram 10 — FreeRTOS Görev Durumları ve Snapshot Queue Mekanizması

Bölüm 2.5.1 ve 3.3 için. FreeRTOS durum geçişleri ile ISR→Task ve Task→Task iletişim kalıpları.

## FreeRTOS Görev Durum Makinesi

```mermaid
stateDiagram-v2
    [*] --> Ready : osThreadNew() / xTaskCreate()

    Ready --> Running : Zamanlayıcı seçer\n(en yüksek öncelik)

    Running --> Ready : Preemption\n(daha yüksek öncelikli görev hazır)\nVEYA taskYIELD()

    Running --> Blocked : vTaskDelay()\nVEYA xTaskNotifyWait()\nVEYA xQueuePeek(timeout)\nVEYA vTaskDelayUntil()

    Blocked --> Ready : Zaman doldu\nVEYA xTaskNotifyFromISR()\nVEYA xQueueOverwrite() + notify

    Running --> Suspended : vTaskSuspend()
    Suspended --> Ready : vTaskResume()

    Running --> [*] : vTaskDelete(NULL)\n(init başarısız ise)

    note right of Running
        Her an yalnızca 1 görev Running.
        bmiTask (High) aktifken tüm
        BelowNormal görevler Blocked/Ready
        kalır — preemption anında olur.
    end note
```

## ISR → Task Notification Kalıbı

```mermaid
sequenceDiagram
    participant HW as Donanım (BMI088)
    participant ISR as EXTI3_IRQHandler (ISR)
    participant KERN as FreeRTOS Kernel
    participant TASK as imu_task (Blocked)

    HW->>ISR: ACC DRDY sinyali (PB3 EXTI)
    ISR->>KERN: xTaskNotifyFromISR(handle, NOTIFY_ACC_DRDY, eSetBits, &woken)
    Note over KERN: Görev bildirim biti set edilir
    ISR->>KERN: portYIELD_FROM_ISR(woken)
    Note over KERN: Context switch planlanır
    KERN->>TASK: xTaskNotifyWait() döner, bits & NOTIFY_ACC_DRDY
    TASK->>TASK: bmi088_start_accel_dma()
```

## Task → Task Snapshot Queue Kalıbı (Mailbox)

```mermaid
sequenceDiagram
    participant PROD as imu_task (Producer)
    participant Q as Queue (depth=1)
    participant CONS as baro_task (Consumer)

    Note over PROD,Q: Her Mahony güncellemesinde
    PROD->>Q: xQueueOverwrite(snapshot_q, &snap)
    Note over Q: Eski değer silinir, yeni değer yazılır\n(hiçbir zaman bloklanmaz)

    Note over Q,CONS: 100 ms döngüde baro_task okur
    CONS->>Q: xQueuePeek(snapshot_q, &imu_snap, 0)
    Note over Q: Değer kuyrukta kalır (destructive değil)\nTimeout=0: yoksa NULL döner (baro-only mod)
    Q-->>CONS: imu_snap (veya pdFALSE → imu_ptr = NULL)
```

## Güvenilirlik Mekanizmaları Özeti

```mermaid
flowchart LR
    subgraph RELIABILITY["Güvenilirlik Katmanı"]
        direction TB
        IWDG["IWDG Watchdog\nbaro_task her 100ms besler\n~2s timeout → sistem reset"]
        SOF["Stack Overflow Hook\nconfigCHECK_FOR_STACK_OVERFLOW=2\nher context switch'te kontrol\n→ vApplicationStackOverflowHook → reset"]
        ERR["Error_Handler\nNVIC_SystemReset()\n(infinite loop değil)"]
        BARO_ONLY["Baro-only Degraded Mod\nbmi088_init × 3 başarısız\n→ imu_task silinir\n→ imu_snapshot_peek = false\n→ FSM + Kalman NULL koruması"]
    end
```

> **Neden depth=1 queue yeterli:** Producer (imu_task) her zaman en güncel veriyi yazar; consumer (baro_task, telemetry_task) her okumada en güncel değeri alır. Eski değerlerin birikmesi anlamsız olduğundan `xQueueOverwrite` + `xQueuePeek` kombinasyonu lock-free mailbox işlevi görür.
