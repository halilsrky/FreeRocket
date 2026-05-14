"""
Protokol sabitleri, paket encode/decode, CSV yükleme ve pencere gruplama.
Qt bağımlılığı yok — test edilebilir saf Python.

SIT_TELEMETRY (STM32 → PC, 54 byte):
  0xAB | alt(4) | press(4) | ax(4) | ay(4) | az(4)
       | pitch(4) | roll(4) | yaw(4)
       | gps_alt(4) | lat(4) | lon(4) | vel(4)
       | status_hi | status_lo | chk | 0x0D 0x0A

SUT_COMBINED (PC → STM32):
  0xAD | count(1) | count×[sim_t(4)+gx(4)+gy(4)+gz(4)]
       | ax(4)+ay(4)+az(4)   ← son örnek ivmesi (flight_sm için)
       | alt(4)+press(4)+baro_t(4) | chk | CR LF

  Boyut: count×16 + 29   (count=20 → 349 byte)

SUT_RESPONSE (STM32 → PC):
  0xAE | sim_time(4) | alt(4) | roll(4) | pitch(4) | yaw(4)
       | status_hi | status_lo | chk | CR LF   (26 byte sabit)
"""
import struct
import csv

# ── Komut paketleri (PC → STM32, 5 byte) ──────────────────────────────────
SIT_CMD  = bytes([0xAA, 0x20, 0xCA, 0x0D, 0x0A])
SUT_CMD  = bytes([0xAA, 0x22, 0xCC, 0x0D, 0x0A])
STOP_CMD = bytes([0xAA, 0x24, 0xCE, 0x0D, 0x0A])

# ── SIT_TELEMETRY ──────────────────────────────────────────────────────────
SIT_HEADER      = 0xAB
SIT_PACKET_SIZE = 54       # sabit
SIT_CHKSUM_SPAN = 51       # [0..50] dahil
SIT_FIELD_COUNT = 12       # float sayısı

# ── SUT_COMBINED ───────────────────────────────────────────────────────────
COMBINED_HEADER  = 0xAD
IMU_SAMPLE_SIZE  = 16      # sim_time(4) + gx(4) + gy(4) + gz(4)
IMU_BATCH_MAX    = 25      # cmd_task / sut_task.h ile eşleşmeli
PKT_FIXED_BYTES  = 29      # header(1)+count(1)+accel(12)+baro(12)+chk(1)+footer(2)

# ── SUT_RESPONSE ───────────────────────────────────────────────────────────
RESPONSE_HEADER = 0xAE
RESPONSE_SIZE   = 26

WINDOW_S = 0.1             # 100 ms simülasyon penceresi


# ── flight.status bitmask → faz adı ───────────────────────────────────────
def status_to_phase(status: int) -> str:
    if status & 0x0100:                  # FSM_BIT_LANDED
        return "LANDED"
    if status & 0x0080:                  # FSM_BIT_MAIN
        return "MAIN DESC"
    if status & 0x0020:                  # FSM_BIT_DROGUE
        return "DROGUE DESC"
    if status & 0x0010:                  # FSM_BIT_APOGEE
        tilt = bool(status & 0x0008)    # FSM_BIT_TILT_EMERG
        vel  = bool(status & 0x0200)    # FSM_BIT_VEL_APOGEE
        if tilt and vel:
            return "APOGEE (Açı+İrt.)"  # her iki tespit de onaylandı
        if tilt:
            return "APOGEE (Açı)"       # sadece açı tetikledi, hız henüz onaylamadı
        return "APOGEE (İrt.)"          # hız/irtifa kaynaklı
    if status & 0x0004:                  # FSM_BIT_ARMED — arming aktif, hâlâ COAST
        return "ARMED"
    if status & 0x0002:                  # FSM_BIT_BURNOUT — henüz arming yok
        return "COAST"
    if status & 0x0001:                  # FSM_BIT_LAUNCHED
        return "BOOST"
    return "IDLE"


# ── CSV yükleme ────────────────────────────────────────────────────────────
def load_csv(filepath: str) -> list[dict]:
    """
    CSV'yi dict listesi olarak döndürür.
    Zorunlu: time, altitude, accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z
    İsteğe bağlı: pressure (yoksa 0.0)
    """
    required = ('time', 'altitude', 'accel_x', 'accel_y', 'accel_z',
                'gyro_x', 'gyro_y', 'gyro_z')
    rows = []
    with open(filepath, newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for raw in reader:
            try:
                entry = {k: float(raw.get(k) or 0.0) for k in required}
                entry['pressure'] = float(raw.get('pressure') or 0.0)
                rows.append(entry)
            except (ValueError, TypeError):
                continue
    return rows


# ── Pencere gruplama ───────────────────────────────────────────────────────
def build_windows(rows: list[dict], window_s: float = WINDOW_S) -> list[list[dict]]:
    """
    Satırları 100 ms'lik simülasyon zamanı pencerelerine böler.
    Pencere 1: [t0, t0+0.1), Pencere 2: [t0+0.1, t0+0.2), ...
    """
    if not rows:
        return []
    windows: list[list[dict]] = []
    current: list[dict] = []
    boundary = rows[0]['time'] + window_s

    for row in rows:
        if row['time'] >= boundary - 1e-9 and current:
            windows.append(current)
            current = [row]
            boundary += window_s
        else:
            current.append(row)

    if current:
        windows.append(current)
    return windows


# ── Paket oluşturma ────────────────────────────────────────────────────────
def build_combined_packet(window: list[dict]) -> bytes:
    """
    SUT_COMBINED paketi döndürür.

    IMU batch: her satırdaki gyro verisi.
    Accel: window[-1] (son örnek) → flight_sm launch/tilt tespiti için.
    Baro:  window[-1] altitude/pressure/time.
    """
    if len(window) > IMU_BATCH_MAX:
        window = window[-IMU_BATCH_MAX:]

    count    = len(window)
    pkt_size = 2 + count * IMU_SAMPLE_SIZE + 12 + 12 + 1 + 2  # accel(12) eklendi
    packet   = bytearray(pkt_size)

    packet[0] = COMBINED_HEADER
    packet[1] = count & 0xFF

    # IMU batch
    for i, row in enumerate(window):
        off = 2 + i * IMU_SAMPLE_SIZE
        struct.pack_into('>f', packet, off,      float(row['time']))
        struct.pack_into('>f', packet, off + 4,  float(row['gyro_x']))
        struct.pack_into('>f', packet, off + 8,  float(row['gyro_y']))
        struct.pack_into('>f', packet, off + 12, float(row['gyro_z']))

    # Son örnek ivmesi
    last     = window[-1]
    acc_off  = 2 + count * IMU_SAMPLE_SIZE
    struct.pack_into('>f', packet, acc_off,     float(last['accel_x']))
    struct.pack_into('>f', packet, acc_off + 4, float(last['accel_y']))
    struct.pack_into('>f', packet, acc_off + 8, float(last['accel_z']))

    # Baro
    baro_off = acc_off + 12
    struct.pack_into('>f', packet, baro_off,     float(last['altitude']))
    struct.pack_into('>f', packet, baro_off + 4, float(last.get('pressure', 0.0)))
    struct.pack_into('>f', packet, baro_off + 8, float(last['time']))

    chk_idx         = baro_off + 12
    packet[chk_idx]     = sum(packet[:chk_idx]) % 256
    packet[chk_idx + 1] = 0x0D
    packet[chk_idx + 2] = 0x0A

    return bytes(packet)


# ── Response çözme ─────────────────────────────────────────────────────────
def parse_response(data: bytes) -> dict | None:
    """26-byte SUT_RESPONSE paketini çözer. Geçersizse None döner."""
    if len(data) < RESPONSE_SIZE:
        return None
    if data[0] != RESPONSE_HEADER:
        return None
    if data[RESPONSE_SIZE - 2] != 0x0D or data[RESPONSE_SIZE - 1] != 0x0A:
        return None
    chk = sum(data[:RESPONSE_SIZE - 3]) % 256
    if chk != data[RESPONSE_SIZE - 3]:
        return None

    return dict(
        sim_time = struct.unpack_from('>f', data,  1)[0],
        alt      = struct.unpack_from('>f', data,  5)[0],
        roll     = struct.unpack_from('>f', data,  9)[0],
        pitch    = struct.unpack_from('>f', data, 13)[0],
        yaw      = struct.unpack_from('>f', data, 17)[0],
        status   = (data[21] << 8) | data[22],
    )


# ── SIT parse ─────────────────────────────────────────────────────────────
def parse_sit_packet(data: bytes) -> dict | None:
    """54-byte SIT_TELEMETRY paketini çözer. Geçersizse None döner."""
    if len(data) < SIT_PACKET_SIZE:
        return None
    if data[0] != SIT_HEADER:
        return None
    if data[52] != 0x0D or data[53] != 0x0A:
        return None
    chk = sum(data[:SIT_CHKSUM_SPAN]) % 256
    if chk != data[51]:
        return None
    fields = struct.unpack_from('>12f', data, 1)
    return dict(
        alt      = fields[0],
        pressure = fields[1],
        ax       = fields[2],
        ay       = fields[3],
        az       = fields[4],
        pitch    = fields[5],
        roll     = fields[6],
        yaw      = fields[7],
        gps_alt  = fields[8],
        lat      = fields[9],
        lon      = fields[10],
        vel      = fields[11],
        status   = (data[49] << 8) | data[50],
    )


def extract_sit_packet(buf: bytearray) -> dict | None:
    """Buffer içinden ilk geçerli SIT paketini çıkarır; bulunan paket silinir."""
    while len(buf) >= SIT_PACKET_SIZE:
        idx = buf.find(SIT_HEADER)
        if idx == -1:
            buf.clear()
            return None
        if idx > 0:
            del buf[:idx]
        if len(buf) < SIT_PACKET_SIZE:
            return None
        pkt = parse_sit_packet(bytes(buf[:SIT_PACKET_SIZE]))
        if pkt is not None:
            del buf[:SIT_PACKET_SIZE]
            return pkt
        del buf[:1]
    return None


def extract_response(buf: bytearray) -> dict | None:
    """
    Buffer içinden ilk geçerli response'ı çıkarır, bulunan paket silinir.
    Header eşleşmesi yoksa buffer temizlenir.
    """
    while len(buf) >= RESPONSE_SIZE:
        idx = buf.find(RESPONSE_HEADER)
        if idx == -1:
            buf.clear()
            return None
        if idx > 0:
            del buf[:idx]
        if len(buf) < RESPONSE_SIZE:
            return None
        resp = parse_response(bytes(buf[:RESPONSE_SIZE]))
        if resp is not None:
            del buf[:RESPONSE_SIZE]
            return resp
        del buf[:1]
    return None
