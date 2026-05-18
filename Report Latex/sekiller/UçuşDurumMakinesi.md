# Diyagram 6 — Uçuş Durum Makinesi (7-Faz FSM)

Bölüm 3.6 için. Her geçişin koşulu ve eşik değerleri ile birlikte tam FSM.

```mermaid
flowchart TD
    START([flight_sm_init]) --> IDLE

    IDLE([IDLE])
    IDLE --> C1["IMU: total_accel > 45 m/s²\nBaro-only: velocity > 15 m/s"]
    C1 --> BOOST

    BOOST([BOOST])
    BOOST --> C2["accel_vert < 0 · 5 ardışık\nVEYA t_boost > 8000 ms"]
    C2 --> COAST

    COAST([COAST])
    COAST --> C3["altitude_rel > 1700 m → armed = true"]
    C3 --> COAST
    COAST --> C4["armed && velocity < 0\n5 ardışık örnek"]
    C4 --> APOGEE
    COAST --> C5["armed && theta > 70°\nAcil — anlık geçiş"]
    C5 --> APOGEE

    APOGEE([APOGEE])
    APOGEE --> C6["Anlık — Drogue GPIO tetiklenir"]
    C6 --> DROGUE

    DROGUE([DROGUE_DESCENT])
    DROGUE --> C7["altitude_rel < 300 m\nAna paraşüt GPIO tetiklenir"]
    C7 --> MAIN

    MAIN([MAIN_DESCENT])
    MAIN --> C8["velocity < 2 m/s · 30 ardışık · 3 s"]
    C8 --> LANDED

    LANDED([LANDED — Terminal\nYalnızca reset ile çıkış])

    style IDLE   fill:#f0f0f0,stroke:#555
    style BOOST  fill:#ffe0b2,stroke:#e65100
    style COAST  fill:#e3f2fd,stroke:#0d47a1
    style APOGEE fill:#fce4ec,stroke:#880e4f
    style DROGUE fill:#e8f5e9,stroke:#1b5e20
    style MAIN   fill:#e8f5e9,stroke:#1b5e20
    style LANDED fill:#eeeeee,stroke:#333,stroke-dasharray:4 4

    style C1 fill:#fffde7,stroke:#f9a825
    style C2 fill:#fffde7,stroke:#f9a825
    style C3 fill:#fffde7,stroke:#f9a825
    style C4 fill:#fffde7,stroke:#f9a825
    style C5 fill:#fffde7,stroke:#f9a825
    style C6 fill:#fffde7,stroke:#f9a825
    style C7 fill:#fffde7,stroke:#f9a825
    style C8 fill:#fffde7,stroke:#f9a825
```

> **Hız ve açı tabanlı apogee bağımsız çalışır:** Her ikisi de `armed == true` olduktan sonra her `flight_sm_update()` çağrısında kontrol edilir; yalnızca `COAST` fazındayken `APOGEE` geçişini tetikler.
