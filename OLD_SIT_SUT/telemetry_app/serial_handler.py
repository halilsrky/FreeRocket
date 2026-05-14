"""
Serial Communication Handler
Handles UART communication with UKB device
"""
import serial
import struct
import threading
import time
from typing import Optional, Callable, List, Tuple
from dataclasses import dataclass
from queue import Queue

from models import TelemetryPacket, StatusPacket, ConnectionConfig


# Protocol Constants
PACKET_HEADER_TELEMETRY = 0xAB   # SIT/NORMAL telemetri çıkışı (STM32 → PC)
PACKET_HEADER_STATUS    = 0xAA   # Durum paketi  (STM32 → PC) / CMD  (PC → STM32)
PACKET_FOOTER = bytes([0x0D, 0x0A])

# STM32'den gelen telemetri paketi (54 byte, SIT modu)
SIT_FLOAT_COUNT        = 12
SIT_FLOAT_COUNT_LEGACY = 8
SIT_STATUS_BYTES       = 2
SIT_PACKET_SIZE          = 1 + (SIT_FLOAT_COUNT * 4) + SIT_STATUS_BYTES + 1 + 2
SIT_PACKET_SIZE_NO_STATUS = 1 + (SIT_FLOAT_COUNT * 4) + 1 + 2
SIT_PACKET_SIZE_LEGACY    = 1 + (SIT_FLOAT_COUNT_LEGACY * 4) + 1 + 2

# SUT gönderim paketi (PC → STM32) — combined, her 100 ms simülasyon zamanı
#   SUT_COMBINED: 0xAD | count(1) | count×[sim_t(4)+gx(4)+gy(4)+gz(4)] | alt(4)+press(4)+baro_t(4) | chk | CR LF
SUT_COMBINED_HEADER  = 0xAD
SUT_IMU_SAMPLE_SIZE  = 16   # sim_time(4) + gyro_x(4) + gyro_y(4) + gyro_z(4)
SUT_WINDOW_S         = 0.1  # 100 ms simülasyon penceresi

# Komut paketleri (PC → STM32)
SIT_CMD  = bytearray([0xAA, 0x20, 0xCA, 0x0D, 0x0A])
SUT_CMD  = bytearray([0xAA, 0x22, 0xCC, 0x0D, 0x0A])
STOP_CMD = bytearray([0xAA, 0x24, 0xCE, 0x0D, 0x0A])


class SerialHandler:
    """Thread-safe serial communication handler"""
    
    def __init__(self):
        self._serial: Optional[serial.Serial] = None
        self._config: Optional[ConnectionConfig] = None
        self._connected = False
        self._running = False
        self._mode: Optional[str] = None
        
        # Callbacks
        self._telemetry_callback: Optional[Callable[[TelemetryPacket], None]] = None
        self._telemetry_rx_callback: Optional[Callable[[TelemetryPacket], None]] = None
        self._status_callback: Optional[Callable[[StatusPacket], None]] = None
        self._error_callback: Optional[Callable[[str], None]] = None
        
        # Threads
        self._receiver_thread: Optional[threading.Thread] = None
        self._sender_thread: Optional[threading.Thread] = None
        
        # SUT mode data
        self._csv_data: Optional[List[List[float]]] = None
        self._csv_index = 0
        
        # Thread lock
        self._lock = threading.Lock()
    
    @property
    def connected(self) -> bool:
        return self._connected
    
    @property
    def mode(self) -> Optional[str]:
        return self._mode
    
    def set_telemetry_callback(self, callback: Callable[[TelemetryPacket], None]):
        self._telemetry_callback = callback

    def set_telemetry_rx_callback(self, callback: Callable[[TelemetryPacket], None]):
        self._telemetry_rx_callback = callback
    
    def set_status_callback(self, callback: Callable[[StatusPacket], None]):
        self._status_callback = callback
    
    def set_error_callback(self, callback: Callable[[str], None]):
        self._error_callback = callback
    
    def connect(self, config: ConnectionConfig) -> bool:
        """Establish serial connection"""
        try:
            self._serial = serial.Serial(
                port=config.port,
                baudrate=config.baudrate,
                bytesize=config.databits,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=config.timeout
            )
            self._config = config
            self._connected = True
            return True
        except serial.SerialException as e:
            self._report_error(f"Connection failed: {e}")
            return False
    
    def disconnect(self):
        """Close serial connection"""
        self.stop_mode()
        if self._serial and self._serial.is_open:
            self._serial.close()
        self._connected = False
        self._serial = None
    
    def start_sit_mode(self):
        """Start SIT mode - receive telemetry from device"""
        if not self._connected:
            self._report_error("Not connected")
            return
        
        self._mode = "SIT"
        self._running = True
        self._send_command(SIT_CMD)
        
        self._receiver_thread = threading.Thread(target=self._sit_receiver_loop, daemon=True)
        self._receiver_thread.start()
    
    def start_sut_mode(self, csv_data: Optional[List[dict]] = None):
        """Start SUT mode - send telemetry, receive status"""
        if not self._connected:
            self._report_error("Not connected")
            return
        
        self._mode = "SUT"
        self._running = True
        self._csv_data = csv_data
        
        # Aggressive reset sequence (like disconnect/reconnect does)
        if self._serial:
            # First send STOP to reset STM32 state
            self._serial.write(STOP_CMD)
            time.sleep(0.1)
            
            # Flush both buffers
            self._serial.reset_input_buffer()
            self._serial.reset_output_buffer()
            time.sleep(0.05)
        
        # Start receiver FIRST (like test.py does)
        self._receiver_thread = threading.Thread(target=self._sut_receiver_loop, daemon=True)
        self._receiver_thread.start()
        
        # Small delay to ensure receiver is ready
        time.sleep(0.1)
        
        # Send SUT command
        self._send_command(SUT_CMD)
        
        # Flush input buffer after command to clear any echo
        if self._serial:
            time.sleep(0.05)
            self._serial.reset_input_buffer()
        
        # Start sender thread
        self._sender_thread = threading.Thread(target=self._sut_sender_loop, daemon=True)
        self._sender_thread.start()
    
    def stop_mode(self):
        """Stop current mode"""
        self._running = False
        if self._connected and self._serial:
            self._send_command(STOP_CMD)
        
        if self._receiver_thread and self._receiver_thread.is_alive():
            self._receiver_thread.join(timeout=1.0)
        if self._sender_thread and self._sender_thread.is_alive():
            self._sender_thread.join(timeout=1.0)
        
        self._mode = None
        self._receiver_thread = None
        self._sender_thread = None
    
    def _send_command(self, cmd: bytearray):
        """Send command to device"""
        if self._serial and self._serial.is_open:
            with self._lock:
                self._serial.write(cmd)
    
    def _report_error(self, message: str):
        """Report error through callback"""
        if self._error_callback:
            self._error_callback(message)
    
    def _sit_receiver_loop(self):
        """Receive loop for SIT mode - telemetry packets"""
        buffer = bytearray()
        
        while self._running and self._mode == "SIT":
            try:
                if self._serial and self._serial.in_waiting > 0:
                    with self._lock:
                        buffer.extend(self._serial.read(self._serial.in_waiting))
                
                while len(buffer) >= SIT_PACKET_SIZE_LEGACY:
                    header_idx = buffer.find(PACKET_HEADER_TELEMETRY)
                    if header_idx == -1:
                        buffer.clear()
                        break
                    
                    if header_idx > 0:
                        buffer = buffer[header_idx:]
                    
                    if len(buffer) < SIT_PACKET_SIZE_LEGACY:
                        break

                    packet = None
                    consumed = 0

                    if len(buffer) >= SIT_PACKET_SIZE:
                        parsed = self._parse_telemetry_packet(
                            buffer[:SIT_PACKET_SIZE],
                            SIT_FLOAT_COUNT,
                            has_status=True,
                            swap_pitch_roll=True
                        )
                        if parsed:
                            packet, _ = parsed
                            consumed = SIT_PACKET_SIZE
                        else:
                            parsed = self._parse_telemetry_packet(
                                buffer[:SIT_PACKET_SIZE_NO_STATUS],
                                SIT_FLOAT_COUNT,
                                has_status=False,
                                swap_pitch_roll=True
                            )
                            if parsed:
                                packet, _ = parsed
                                consumed = SIT_PACKET_SIZE_NO_STATUS
                    else:
                        if len(buffer) >= SIT_PACKET_SIZE_NO_STATUS:
                            if buffer[SIT_PACKET_SIZE_NO_STATUS - 2:SIT_PACKET_SIZE_NO_STATUS] != PACKET_FOOTER:
                                break
                            parsed = self._parse_telemetry_packet(
                                buffer[:SIT_PACKET_SIZE_NO_STATUS],
                                SIT_FLOAT_COUNT,
                                has_status=False,
                                swap_pitch_roll=True
                            )
                            if parsed:
                                packet, _ = parsed
                                consumed = SIT_PACKET_SIZE_NO_STATUS

                    if packet is None and len(buffer) >= SIT_PACKET_SIZE_LEGACY:
                        if buffer[SIT_PACKET_SIZE_LEGACY - 2:SIT_PACKET_SIZE_LEGACY] == PACKET_FOOTER:
                            parsed = self._parse_telemetry_packet(
                                buffer[:SIT_PACKET_SIZE_LEGACY],
                                SIT_FLOAT_COUNT_LEGACY,
                                has_status=False,
                                swap_pitch_roll=False
                            )
                            if parsed:
                                packet, _ = parsed
                                consumed = SIT_PACKET_SIZE_LEGACY

                    if packet:
                        if self._telemetry_callback:
                            self._telemetry_callback(packet)
                        buffer = buffer[consumed:]
                    else:
                        buffer = buffer[1:]
                
                time.sleep(0.01)
            except Exception as e:
                self._report_error(f"SIT receiver error: {e}")
    
    def _sut_sender_loop(self):
        """
        100 ms'lik simülasyon pencerelerinde combined paket gönderir.
          • Bir pencere: N adet IMU örneği (sim_time + gyro) + son baro verisi
          • Paket tipi: SUT_COMBINED (0xAD), değişken uzunluk
          • Zamanlama: sleep(98 ms) + 2 ms busy-wait → düşük CPU, ~1 ms hassasiyet
          • Her 100 ms'de 1 write() çağrısı — eski 5 ms'de 1 yerine 20× daha az syscall
        """
        if not self._csv_data:
            self._report_error("SUT sender: CSV verisi yok")
            return

        try:
            import ctypes
            ctypes.windll.winmm.timeBeginPeriod(1)
        except Exception:
            pass

        start_real  = time.perf_counter()
        next_window = SUT_WINDOW_S
        imu_buffer: list = []

        for row in self._csv_data:
            if not self._running or self._mode != "SUT":
                break

            sim_time = row.get('time', 0.0)
            imu_buffer.append(row)

            if sim_time >= next_window - 1e-6:
                # Pencere doldu — hedef gerçek zamana kadar bekle
                target    = start_real + next_window
                remaining = target - time.perf_counter()
                if remaining > 0.002:
                    time.sleep(remaining - 0.001)  # uyku: gereksiz CPU kullanımı önle
                while time.perf_counter() < target:  # son 1-2 ms busy-wait
                    if not self._running:
                        break

                if not self._running or self._mode != "SUT":
                    break

                pkt = self._create_combined_packet(imu_buffer, imu_buffer[-1])
                if self._serial and self._serial.is_open:
                    self._serial.write(pkt)  # tek write() çağrısı — lock gereksiz (sole writer)

                # GUI: sadece pencere başına bir güncelleme
                if self._telemetry_callback:
                    baro_row = imu_buffer[-1]
                    try:
                        telemetry = TelemetryPacket.from_raw_values([
                            baro_row.get('altitude', 0.0), 0.0,
                            baro_row.get('accel_x', 0.0),
                            baro_row.get('accel_y', 0.0),
                            baro_row.get('accel_z', 0.0),
                            0.0, 0.0, 0.0])
                        self._telemetry_callback(telemetry)
                    except Exception:
                        pass

                imu_buffer  = []
                next_window += SUT_WINDOW_S

        self._running = False
    
    def _sut_receiver_loop(self):
        """Receiver loop for SUT mode - status packets"""
        buffer = bytearray()
        
        while self._running and self._mode == "SUT":
            try:
                # Read available data
                if self._serial and self._serial.is_open:
                    in_waiting = self._serial.in_waiting
                    if in_waiting > 0:
                        with self._lock:
                            new_data = self._serial.read(in_waiting)
                        buffer.extend(new_data)
                
                while len(buffer) >= 6:
                    header = buffer[0]

                    if header == PACKET_HEADER_TELEMETRY:
                        if len(buffer) < SIT_PACKET_SIZE:
                            if len(buffer) < SIT_PACKET_SIZE_NO_STATUS:
                                break
                            if buffer[SIT_PACKET_SIZE_NO_STATUS - 2:SIT_PACKET_SIZE_NO_STATUS] != PACKET_FOOTER:
                                break

                            parsed = self._parse_telemetry_packet(
                                buffer[:SIT_PACKET_SIZE_NO_STATUS],
                                SIT_FLOAT_COUNT,
                                has_status=False,
                                swap_pitch_roll=True
                            )
                            if parsed:
                                packet, _ = parsed
                                if self._telemetry_rx_callback:
                                    self._telemetry_rx_callback(packet)
                                buffer = buffer[SIT_PACKET_SIZE_NO_STATUS:]
                                continue
                            buffer = buffer[1:]
                            continue

                        parsed = self._parse_telemetry_packet(
                            buffer[:SIT_PACKET_SIZE],
                            SIT_FLOAT_COUNT,
                            has_status=True,
                            swap_pitch_roll=True
                        )
                        if parsed:
                            packet, status_word = parsed
                            if self._telemetry_rx_callback:
                                self._telemetry_rx_callback(packet)
                            if status_word is not None and self._status_callback:
                                status_low = status_word & 0xFF
                                status_high = (status_word >> 8) & 0xFF
                                self._status_callback(StatusPacket.from_raw(status_low, status_high))
                            buffer = buffer[SIT_PACKET_SIZE:]
                        else:
                            if len(buffer) >= SIT_PACKET_SIZE_NO_STATUS:
                                parsed = self._parse_telemetry_packet(
                                    buffer[:SIT_PACKET_SIZE_NO_STATUS],
                                    SIT_FLOAT_COUNT,
                                    has_status=False,
                                    swap_pitch_roll=True
                                )
                                if parsed:
                                    packet, _ = parsed
                                    if self._telemetry_rx_callback:
                                        self._telemetry_rx_callback(packet)
                                    buffer = buffer[SIT_PACKET_SIZE_NO_STATUS:]
                                else:
                                    buffer = buffer[1:]
                            else:
                                buffer = buffer[1:]
                    elif header == PACKET_HEADER_STATUS:
                        if len(buffer) < 6:
                            break

                        packet = self._parse_status_packet(buffer[:6])
                        if packet and self._status_callback:
                            self._status_callback(packet)
                            buffer = buffer[6:]
                        else:
                            buffer = buffer[1:]
                    else:
                        buffer = buffer[1:]
                
                time.sleep(0.01)
            except Exception as e:
                self._report_error(f"SUT receiver error: {e}")
    
    def _parse_telemetry_packet(
        self,
        data: bytes,
        float_count: int,
        has_status: bool,
        swap_pitch_roll: bool
    ) -> Optional[Tuple[TelemetryPacket, Optional[int]]]:
        """Parse telemetry packet and optional status bits"""
        status_bytes = 2 if has_status else 0
        expected_size = 1 + (float_count * 4) + status_bytes + 1 + 2
        if len(data) != expected_size:
            return None

        header = data[0]
        footer = data[expected_size - 2:expected_size]

        if header != PACKET_HEADER_TELEMETRY or footer != PACKET_FOOTER:
            return None

        status_word = None
        if has_status:
            status_high = data[1 + (float_count * 4)]
            status_low = data[1 + (float_count * 4) + 1]
            status_word = (status_high << 8) | status_low
            checksum_index = 1 + (float_count * 4) + 2
        else:
            checksum_index = 1 + (float_count * 4)

        calculated_checksum = sum(data[:checksum_index]) % 256
        received_checksum = data[checksum_index]

        if calculated_checksum != received_checksum:
            return None

        values = []
        for i in range(float_count):
            start = 1 + (i * 4)
            float_bytes = data[start:start + 4]
            value = struct.unpack('>f', float_bytes)[0]
            values.append(value)

        if swap_pitch_roll and len(values) >= 7:
            values[5], values[6] = values[6], values[5]

        return TelemetryPacket.from_raw_values(values, received_checksum), status_word
    
    def _parse_status_packet(self, data: bytes) -> Optional[StatusPacket]:
        """Parse 6-byte status packet"""
        if len(data) != 6:
            return None
        
        header, status_low, status_high, checksum, footer1, footer2 = data
        
        if header != PACKET_HEADER_STATUS:
            return None
        if footer1 != 0x0D or footer2 != 0x0A:
            return None
        
        # Verify checksum
        calculated = (header + status_low + status_high) % 256
        if calculated != checksum:
            return None
        
        return StatusPacket.from_raw(status_low, status_high)
    
    def _create_combined_packet(self, imu_rows: list, baro_row: dict) -> bytes:
        """
        SUT_COMBINED paketi (değişken uzunluk):
          0xAD | count(1) | count×[sim_time(4)+gx(4)+gy(4)+gz(4)] | alt(4)+press(4)+baro_t(4) | chk | 0x0D 0x0A
        chk = sum(byte[0..size-4]) % 256
        N ≤ 25  →  max ~417 byte
        """
        count    = len(imu_rows)
        pkt_size = 2 + count * SUT_IMU_SAMPLE_SIZE + 12 + 1 + 2
        packet   = bytearray(pkt_size)
        packet[0] = SUT_COMBINED_HEADER
        packet[1] = count & 0xFF
        for i, row in enumerate(imu_rows):
            off = 2 + i * SUT_IMU_SAMPLE_SIZE
            struct.pack_into('>f', packet, off,      float(row.get('time',   0.0)))
            struct.pack_into('>f', packet, off + 4,  float(row.get('gyro_x', 0.0)))
            struct.pack_into('>f', packet, off + 8,  float(row.get('gyro_y', 0.0)))
            struct.pack_into('>f', packet, off + 12, float(row.get('gyro_z', 0.0)))
        baro_off = 2 + count * SUT_IMU_SAMPLE_SIZE
        struct.pack_into('>f', packet, baro_off,     float(baro_row.get('altitude', 0.0)))
        struct.pack_into('>f', packet, baro_off + 4, float(baro_row.get('pressure', 0.0)))
        struct.pack_into('>f', packet, baro_off + 8, float(baro_row.get('time',     0.0)))
        chk_idx       = baro_off + 12
        packet[chk_idx]     = sum(packet[:chk_idx]) % 256
        packet[chk_idx + 1] = 0x0D
        packet[chk_idx + 2] = 0x0A
        return bytes(packet)
