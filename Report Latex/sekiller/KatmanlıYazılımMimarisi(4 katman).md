# Diyagram 2 — Katmanlı Yazılım Mimarisi

Bölüm 3.1 için. Tek yönlü bağımlılık ilkesiyle 4 katmanlı mimari.

```mermaid
block-beta
  columns 1

  block:app_layer["Uygulama Katmanı (Application Layer)"]:1
    columns 4
    imu_task["imu_task\n(osPriorityHigh)"]
    baro_task["baro_task\n(BelowNormal)"]
    gnss_task["gnss_task\n(BelowNormal)"]
    tel_task["telemetry_task\n(BelowNormal)"]
  end

  block:algo_layer["Algoritma / Servis Katmanı"]:1
    columns 4
    mahony["mahony.c\nAHRS Filtresi"]
    kalman["alt_kalman.c\nIrtifa Kalman"]
    fsm["flight_sm.c\n7-Faz FSM"]
    sut["sut_task.c\nPIL Test"]
  end

  block:driver_layer["Sürücü Katmanı (Driver Layer)"]:1
    columns 3
    bmi["bmi088.c\nIMU Driver"]
    bme["bme280.c\nBaro Driver"]
    gnss_drv["gnss_task.c\nNMEA Parser"]
  end

  block:hal_layer["HAL + FreeRTOS Middleware"]:1
    columns 3
    hal["STM32 HAL\n(CubeMX üretilen)"]
    freertos["FreeRTOS\n(Kernel + Tasks)"]
    segger["SEGGER SystemView\n(Post-Mortem)"]
  end

  block:hw_layer["Donanım (Hardware)"]:1
    columns 4
    hw_imu["BMI088\nI2C1 + DRDY"]
    hw_baro["BME280\nI2C3"]
    hw_gnss["L86 GNSS\nUSART6 DMA"]
    hw_tel["USART2 DMA\nTelemetri"]
  end

  app_layer --> algo_layer
  algo_layer --> driver_layer
  driver_layer --> hal_layer
  hal_layer --> hw_layer
```

> **Bağımlılık yönü:** Her katman yalnızca bir alt katmanı çağırır; yukarı doğru doğrudan çağrı yoktur. CubeMX üretilen `HAL` dosyalarına (`main.c`, `gpio.c`, `i2c.c` vb.) uygulama katmanından dokunulmaz.
