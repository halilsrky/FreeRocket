# Diyagram 7 — SUT (Sentetik Uçuş Testi) Sistem Mimarisi

Bölüm 3.8.2 için. Processor-in-the-Loop test sisteminin uçtan uca veri akışı.

```mermaid
---
config:
  theme: default
  themeVariables:
    fontSize: 18px
  layout: fixed
---
flowchart LR
 subgraph PC["PC / Python "]
    direction TB
        RP["RocketPy"]
        CSV["sim_data.csv"]
        SW["SerialWorker"]
        GUI["PyQt5 GUI"]
        PLT["PyQtGraph Plot"]
  end
 subgraph STM["STM32 / sut_task"]
    direction TB
        RX["UART2 RX"]
        M["Mahony"]
        K["Kalman"]
        F["Flight SM"]
        TXD["UART2 TX"]
  end
    RP --> CSV
    CSV --> SW
    SW --> PLT & GUI
    RX --> M
    M --> K
    K --> F
    F --> TXD
    TXD -- "USB-UART" --> SW
    SW -- "USB-UART" --> RX

     RP:::pc
     CSV:::pc
     SW:::pc
     GUI:::pc
     PLT:::pc
     RX:::stm
     M:::stm
     K:::stm
     F:::stm
     TXD:::stm
    classDef pc fill:#e8f4fd,stroke:#0066cc,stroke-width:1px
    classDef stm fill:#fff8e8,stroke:#cc6600,stroke-width:1px
    classDef sleep fill:#ffeeee,stroke:#cc4444,stroke-width:1px
    classDef pkt fill:#f7f7f7,stroke:#666666,stroke-width:1px
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
```

> **Gerçek zamanlı pacing yoktur:** Python her pencereyi gönderir ve cevap bekler. STM32 gerçek C algoritmalarını çalıştırdığından algoritma doğruluğu garanti altındadır; ancak zamanlama STM32 işlem hızına bağlıdır.
