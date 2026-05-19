# Diyagram 2 — Katmanlı Yazılım Mimarisi

Bölüm 3.1 için. Tek yönlü bağımlılık ilkesiyle 4 katmanlı mimari.

```mermaid
flowchart TB
  subgraph APP["Uygulama Katmanı (Application Layer)"]
    direction LR
    imu_task["imu_task (osPriorityHigh)"]
    baro_task["baro_task (BelowNormal)"]
    gnss_task["gnss_task (BelowNormal)"]
    tel_task["telemetry_task (BelowNormal)"]
    
    imu_task ~~~ baro_task ~~~ gnss_task ~~~ tel_task
  end

  subgraph ALGO["Algoritma / Servis Katmanı"]
    direction LR
    mahony["mahony.c (AHRS Filtresi)"]
    kalman["alt_kalman.c (İrtifa Kalman)"]
    fsm["flight_sm.c (7-Faz FSM)"]
    sut["sut_task.c (PIL Test)"]
    
    mahony ~~~ kalman ~~~ fsm ~~~ sut
  end

  subgraph DRV["Sürücü Katmanı (Driver Layer)"]
    direction LR
    bmi["bmi088.c (IMU Driver)"]
    bme["bme280.c (Baro Driver)"]
    gnss_drv["gnss_task.c (NMEA Parser)"]
    
    bmi ~~~~~ bme ~~~~~ gnss_drv
  end

  subgraph HAL["HAL + FreeRTOS Middleware"]
    direction LR
    hal["STM32 HAL (CubeMX)"]
    freertos["FreeRTOS (Kernel+Tasks)"]
    segger["SEGGER (Post-Mortem)"]
    
    hal ~~~~~ freertos ~~~~~ segger
  end

  subgraph HW["Donanım (Hardware)"]
    direction LR
    hw_imu["BMI088 (I2C1+DRDY)"]
    hw_baro["BME280 (I2C3)"]
    hw_gnss["L86 GNSS (USART6 DMA)"]
    hw_tel["Telemetri (USART2 DMA)"]
    
    hw_imu ~~~ hw_baro ~~~ hw_gnss ~~~ hw_tel
  end

  APP --> ALGO
  ALGO --> DRV
  DRV --> HAL
  HAL --> HW
```

> **Bağımlılık yönü:** Her katman yalnızca bir alt katmanı çağırır; yukarı doğru doğrudan çağrı yoktur. CubeMX üretilen `HAL` dosyalarına (`main.c`, `gpio.c`, `i2c.c` vb.) uygulama katmanından dokunulmaz.
