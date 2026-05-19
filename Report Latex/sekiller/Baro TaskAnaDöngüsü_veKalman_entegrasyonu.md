# Diyagram 5 — Baro Task Ana Döngüsü ve Kalman Entegrasyonu

Bölüm 3.4.2 ve 3.5.2 için. 10 Hz barometrik ölçüm → Kalman güncelleme → FSM tetikleme zinciri.

```mermaid
flowchart TD
    A([baro_task başlar]) --> B{bme280_init?}
    B -- Başarısız --> C[vTaskDelete]
    B -- Başarılı --> D["bme280_config()\nalt_kalman_init()\nref_set = false"]

    D --> LOOP_START

    subgraph LOOP["Ana Döngü — 10 Hz (100 ms)"]
        LOOP_START{sys_mode\n== SUT?} -- Evet --> SUT_SLEEP["iwdg_feed()\nvTaskDelay(200ms)"]
        SUT_SLEEP --> LOOP_START

        LOOP_START -- Hayır --> DELAY["vTaskDelayUntil(&wake, 100ms)"]
        DELAY --> READ["bme280_read(I2C3)\n8 byte ham veri"]
        READ --> PARSE["bme280_parse()\n→ temperature, pressure,\n   humidity, altitude"]
        PARSE --> BARO_SNAP["xQueueOverwrite(baro_q)\nbaro_snapshot_t"]

        BARO_SNAP --> REF{ref_set?}
        REF -- Hayır --> SET_REF["alt_ref = altitude\nref_set = true\nlast_tick = now"]
        SET_REF --> LOOP_START

        REF -- Evet --> DT["dt = (now - last_tick) / 1000.0f"]
        DT --> IMU_PEEK["imu_snapshot_peek()\nimu_ptr = &imu_snap\n(NULL ise baro-only)"]
        IMU_PEEK --> AVERT["avert = accel_vertical(imu_ptr)\n= q × body_accel × q⁻¹ − 9.81\n(imu_ptr NULL ise 0.0f)"]
        AVERT --> ALTREL["alt_rel = altitude − alt_ref"]
        ALTREL --> KALMAN["alt_kalman_update(&kf, alt_rel, avert, dt)\n→ filtered altitude"]
        KALMAN --> ALT_SNAP["xQueueOverwrite(alt_q)\nalt_snapshot_t\n(altitude_rel, velocity, accel_vert)"]
        ALT_SNAP --> FSM["flight_sm_update(&alt_snap, imu_ptr)"]
        FSM --> IWDG["iwdg_feed()"]
        IWDG --> LOOP_START
    end

    style C fill:#ffcccc,stroke:#cc0000
    style KALMAN fill:#ddeeff,stroke:#0055cc
    style FSM fill:#fff0cc,stroke:#cc8800
    style IWDG fill:#ffeeee,stroke:#cc4444
```

> **Referans kalibrasyonu:** Boot'taki ilk baro ölçümü `alt_ref` olarak alınır. Sonraki tüm ölçümler bu değere göre göreceli (`alt_rel`) hesaplanır. Bu, basınç değişimlerini irtifa değişimine doğru dönüştürür.
