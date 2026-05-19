# Diyagram 1 — STM32F446 Donanım Blok Diyagramı

Bölüm 2.3 ve 3.2 için. MCU çevresel birimlerinin sensörlerle bağlantı haritası.

```mermaid
flowchart LR

  %% =========================
  %% Harici Donanımlar
  %% =========================
  subgraph EXT["Harici Donanımlar"]
    direction TB
    BMI088["BMI088 IMU<br/>(ACC + GYRO)"]
    BME280["BME280<br/>Barometre"]
    L86["L86 GNSS"]
    STLINK["ST-Link VCP<br/>(PC / GUI)"]
    LORA["E22 LoRa<br/>(Rezerve)"]
    BUZ["Buzzer"]
  end

  %% =========================
  %% MCU
  %% =========================
  subgraph MCU["STM32F446 NUCLEO<br/>(Cortex-M4 @ 180 MHz)"]
    direction TB

    subgraph BUS["Haberleşme Arabirimleri"]
      direction LR
      I2C1["I2C1"]
      I2C3["I2C3"]
      USART6["USART6"]
      USART2["USART2"]
      UART4["UART4"]
    end

    subgraph GPIO["GPIO / Kesme Hatları"]
      direction LR
      EXTI3["EXTI3<br/>(PB3)"]
      EXTI4["EXTI4<br/>(PB4)"]
      PB14["PB14"]
    end

    IWDG["IWDG<br/>(~2 s Timeout)"]
  end

  %% =========================
  %% Bağlantılar
  %% =========================
  BMI088 <--> |"PB8, PB9<br/>(I2C)"| I2C1
  BMI088 --> |"ACC DRDY"| EXTI3
  BMI088 --> |"GYRO DRDY"| EXTI4

  BME280 <--> |"PA8, PC9<br/>(I2C)"| I2C3

  L86 --> |"DMA RX<br/>(115200)"| USART6
  STLINK <--> |"DMA TX/RX"| USART2
  LORA --> |"DMA TX"| UART4

  BUZ <-->|"Sinyal"| PB14
```

> **Not:** BMI088, ACC ve GYRO için ayrı I2C adresi kullanır; her ikisi de I2C1 bus üzerindedir. DRDY sinyalleri EXTI kesmesi üzerinden IMU task'ını uyandırır.
