# Diyagram 6 — Uçuş Durum Makinesi (7-Faz FSM)

Bölüm 3.6 için. Her geçişin koşulu ve eşik değerleri ile birlikte tam FSM. Degraded (baro-only) mod kırmızı okla gösterilmiştir.

```mermaid
stateDiagram-v2
    [*] --> IDLE : flight_sm_init()

    IDLE --> BOOST : [IMU var] total_accel > 45 m/s²\nVEYA [Baro-only] velocity > 15 m/s\n→ FSM_BIT_LAUNCHED set

    BOOST --> COAST : accel_vert < 0.0 (5 örnek ardışık)\nVEYA t_boost > 8000 ms timeout\n→ FSM_BIT_BURNOUT set

    note right of COAST
        Arming koşulu:
        altitude_rel > 1700 m AGL
        → FSM_BIT_ARMED set
        (armed=true ise apogee izleme başlar)
    end note

    COAST --> APOGEE : [Hız tabanlı, armed] velocity < 0.0\n5 ardışık örnek onayı\n→ FSM_BIT_VEL_APOGEE | FSM_BIT_APOGEE set

    COAST --> APOGEE : [Açı tabanlı, armed] theta > 70°\n(Acil durum — anlık geçiş)\n→ FSM_BIT_TILT_EMERG | FSM_BIT_APOGEE set

    APOGEE --> DROGUE_DESCENT : Anlık geçiş\nDrogue GPIO tetiklenir\n→ FSM_BIT_DROGUE set

    DROGUE_DESCENT --> MAIN_DESCENT : altitude_rel < 300 m AGL\n→ FSM_BIT_MAIN_ALT set\nAna paraşüt GPIO tetiklenir\n→ FSM_BIT_MAIN set

    MAIN_DESCENT --> LANDED : |velocity| < 2 m/s\n30 ardışık örnek (3 saniye)\n→ FSM_BIT_LANDED set

    LANDED --> [*] : Terminal durum\n(yalnızca reset ile çıkış)
```

```mermaid
flowchart LR
    subgraph STATUS["FSM_BIT Status Word (16-bit)"]
        direction TB
        B0["Bit 0\nLAUNCHED"]
        B1["Bit 1\nBURNOUT"]
        B2["Bit 2\nARMED"]
        B3["Bit 3\nDROGUE"]
        B4["Bit 4\nMAIN_ALT"]
        B5["Bit 5\nMAIN"]
        B6["Bit 6\nVEL_APOGEE"]
        B7["Bit 7\nTILT_EMERG"]
        B8["Bit 8\nAPOGEE"]
        B9["Bit 9\nLANDED"]
    end

    subgraph DEGRADE["Degraded Mod (IMU Yok)"]
        direction TB
        D1["BOOST tespiti:\nBaro Kalman velocity > 15 m/s"]
        D2["Kalman ivme girişi: 0.0f\n(daha gürültülü filtre)"]
        D3["Tilt bazlı apogee:\nDevre dışı (imu_ptr = NULL)"]
        D4["Hız bazlı apogee:\nAktif"]
    end
```

> **Hız ve açı tabanlı apogee bağımsız çalışır:** Her ikisi de `s_armed == true` olduktan sonra, fazdan bağımsız olarak her `flight_sm_update()` çağrısında kontrol edilir. Yalnızca `FLIGHT_COAST` fazındayken `FLIGHT_APOGEE`'ye geçişi tetikler; diğer fazlarda yalnızca status bit set edilir.
