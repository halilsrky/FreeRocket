# Diyagram 9 — Yükseklik Kalman Filtresi: Tahmin ve Güncelleme Adımları

Bölüm 3.5.2 için. 3-durumlu Kalman filtresinin (h, v, b) tahmin ve güncelleme aşamaları.

```mermaid
flowchart TD
    IN1["baro_alt\nBME280 → hypsometric dönüşüm"]
    IN2["imu_avert\nBMI088 → az_world − 9.81\n(IMU yoksa: 0.0)"]

    IN1 & IN2 --> INIT

    INIT(["alt_kalman_init()\nx=[h,v,b]=0, P=I×1000"])
    INIT --> GUARD

    GUARD{dt ≤ 0?}
    GUARD -- Evet --> SKIP["İlk örnek atla\nbölme hatası koruması"]

    GUARD -- Hayır --> P1

    subgraph PREDICT["TAHMİN  —  Predict"]
        P1["Q hesapla\n(dt kuvvetleri ile ölçekleme)"]
        P2["x̂ = F · x\nh v b → yeni tahmin"]
        P3["P = F·P·Fᵀ + Q"]
        P1 --> P2 --> P3
    end

    subgraph UPDATE["GÜNCELLEME  —  Update"]
        U1["S = H·P·Hᵀ + R\n(2×2 inovasyon kovaryansı)"]
        U2["K = P·Hᵀ · S⁻¹\n(3×2 Kalman kazancı)"]
        U3["y = z − H·x\n[baro_alt − h,  imu_avert − b]"]
        U4["x = x + K·y"]
        U5["P = (I − K·H)·P"]
        U1 --> U2 --> U3 --> U4 --> U5
    end

    PREDICT --> UPDATE

    UPDATE --> OUT["Çıktı → alt_snapshot\nh: filtrelenmiş irtifa\nv: dikey hız  (FSM apogee)\nb: ivme bias  (FSM burnout)"]
    SKIP  --> OUT

    style PREDICT fill:#e8f4fd,stroke:#0066cc
    style UPDATE  fill:#e8ffe8,stroke:#007700
    style OUT     fill:#fff8e8,stroke:#cc8800
```

> **R matrisi:** `r_alt = 5.0`, `r_acc = 10.0`. SUT modunda `r_acc = 5000` — IMU katkısı pratikte sıfıra indirilir, filtre saf barometrik Kalman gibi davranır.
