# Diyagram 9 — Yükseklik Kalman Filtresi: Tahmin ve Güncelleme Adımları

Bölüm 3.5.2 için. 3-durumlu Kalman filtresinin tahmin (predict) ve güncelleme (update) aşamaları.

```mermaid
flowchart TD
    subgraph STATE["Durum Vektörü x = [h, v, b]"]
        X1["h — filtrelenmiş irtifa (m)"]
        X2["v — dikey hız (m/s)"]
        X3["b — ivme bias (m/s²)"]
    end

    subgraph MATRICES["Sabit Matrisler"]
        FM["F (Geçiş Matrisi)\n⎡1  dt  dt²/2⎤\n⎢0   1   dt  ⎥\n⎣0   0    1  ⎦"]
        HM["H (Gözlem Matrisi)\n⎡1  0  0⎤ ← baro irtifası\n⎣0  0  1⎦ ← IMU ivmesi"]
        RM["R (Gözlem Gürültüsü)\n⎡r_alt  0  ⎤   r_alt = 5.0\n⎣0    r_acc⎦   r_acc = 10.0\n(SUT: r_acc = 5000 — baro-only)"]
    end

    INIT(["alt_kalman_init()\nx = [0,0,0], P = I×1000"]) --> GUARD

    GUARD{dt ≤ 0?}
    GUARD -- Evet --> SKIP["İlk örnek atla\n(bölme hatası koruma)"]
    GUARD -- Hayır --> PREDICT

    subgraph PREDICT["TAHMIN AŞAMASI (Predict)"]
        P1["Q hesapla (process noise)\ndt kuvvetlerine bağlı ölçekleme"]
        P2["x̂ = F × x\n(önceki durum → yeni tahmin)"]
        P3["P = F×P×Fᵀ + Q\n(kovaryans tahmini)"]
        P1 --> P2 --> P3
    end

    subgraph UPDATE["GÜNCELLEME AŞAMASI (Update)"]
        U1["S = H×P×Hᵀ + R\n(inovasyon kovaryansı — 2×2)"]
        U2["S_inv = S⁻¹\n(analitik ters: det tabanlı)"]
        U3["K = P×Hᵀ × S_inv\n(Kalman kazancı — 3×2)"]
        U4["y = z − H×x\ny = [baro_alt − h, imu_avert − b]\n(inovasyon vektörü)"]
        U5["x = x + K×y\n(durum güncelleme)"]
        U6["P = (I − K×H)×P\n(kovaryans güncelleme)"]
        U1 --> U2 --> U3 --> U4 --> U5 --> U6
    end

    PREDICT --> UPDATE

    UPDATE --> OUT["Çıktı:\nalt_kalman_update() → h (filtrelenmiş irtifa)\nalt_kalman_velocity() → v\nalt_kalman_accel() → b"]

    SKIP --> OUT

    style PREDICT fill:#e8f4fd,stroke:#0066cc
    style UPDATE fill:#e8ffe8,stroke:#007700
    style OUT fill:#fff8e8,stroke:#cc8800
```

```mermaid
block-beta
  columns 2

  block:inputs["Girdiler"]:1
    baro_in["baro_alt: BME280\nhypsometric dönüşüm\nalt_rel = altitude − alt_ref"]
    imu_in["imu_avert: BMI088\nbody-frame → world-frame\naz_world − 9.81 m/s²\n(IMU yoksa: 0.0f)"]
  end

  block:outputs["Çıktılar → FSM ve Telemetri"]:1
    h_out["h → alt_snapshot.altitude_rel"]
    v_out["v → alt_snapshot.velocity\n(FSM apogee tespiti)"]
    b_out["b → alt_snapshot.accel_vert\n(FSM burnout tespiti)"]
  end

  inputs --> outputs
```

> **SUT modunda ivme kanalı susturulmuştur:** `r_acc = 5000` ile IMU ivmesinin Kalman'a katkısı pratikte sıfıra indirilir; filtre saf barometrik Kalman gibi davranır. Bu, SUT ortamında yalnızca RocketPy baro verisinin doğrulanmasını sağlar.
