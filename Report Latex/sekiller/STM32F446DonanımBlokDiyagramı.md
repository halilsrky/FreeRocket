# Diyagram 1 — STM32F446 Donanım Blok Diyagramı

Bölüm 2.3 ve 3.2 için. MCU çevresel birimlerinin sensörlerle bağlantı haritası.

```mermaid
block-beta
  columns 5

  block:mcu["STM32F446 NUCLEO\n(Cortex-M4 @ 180 MHz)"]:5
    columns 3

    block:i2c_block["I2C Periferiler"]:1
      I2C1["I2C1\nPB8(SCL) PB9(SDA)"]
      I2C3["I2C3\nPA8(SCL) PC9(SDA)"]
    end

    block:uart_block["USART Periferiler"]:1
      USART2["USART2\nDMA TX/RX\n(Telemetri)"]
      USART6["USART6\nDMA RX\n(GNSS)"]
      UART4["UART4\nDMA TX\n(LoRa)"]
    end

    block:other_block["Diğer"]:1
      EXTI3["EXTI3 — PB3\nACC DRDY"]
      EXTI4["EXTI4 — PB4\nGYRO DRDY"]
      PB14["PB14\nBUZZER"]
      IWDG["IWDG\nPrescaler/256\nTimeout ~2s"]
    end
  end

  BMI088["BMI088\n6-Eksen IMU\n(ACC+GYRO)"]
  BME280["BME280\nBarometrik\nBasınç Sensörü"]
  L86["L86 GNSS\nNMEA 1Hz\n115200 baud"]
  STLINK["ST-Link VCP\n(PC / Python GUI)"]
  LORA["E22 LoRa\n(Rezerve)"]

  I2C1 --> BMI088
  I2C3 --> BME280
  BMI088 --> EXTI3
  BMI088 --> EXTI4
  USART6 --> L86
  USART2 --> STLINK
  UART4 --> LORA
```

> **Not:** BMI088, ACC ve GYRO için ayrı I2C adresi kullanır; her ikisi de I2C1 bus üzerindedir. DRDY sinyalleri EXTI kesmesi üzerinden IMU task'ını uyandırır.
