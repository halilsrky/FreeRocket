"""
SitWorker — QThread içinde SIT receive döngüsü.

SIT_CMD gönderir, 0xAB telemetri paketlerini sürekli okur.
Her geçerli pakette packet_ready sinyali emit edilir.
"""
import time
import serial
from PyQt5.QtCore import QThread, pyqtSignal

from protocol import SIT_CMD, STOP_CMD, extract_sit_packet


class SitWorker(QThread):
    # alt, pressure, ax, ay, az, pitch, roll, yaw, gps_alt, lat, lon, vel, status, gyro_x, gyro_y, gyro_z
    packet_ready = pyqtSignal(float, float, float, float, float,
                               float, float, float, float, float,
                               float, float, int,
                               float, float, float)

    log      = pyqtSignal(str)
    finished = pyqtSignal(int)   # toplam paket sayısı

    _READ_POLL_S = 0.001

    def __init__(self, port: str, baudrate: int, parent=None):
        super().__init__(parent)
        self._port     = port
        self._baudrate = baudrate
        self._running  = False

    def stop(self):
        self._running = False

    def run(self):
        self._running = True
        count = 0

        try:
            ser = serial.Serial(self._port, self._baudrate, timeout=0.1)
        except serial.SerialException as exc:
            self.log.emit(f"Port açılamadı: {exc}")
            self.finished.emit(0)
            return

        try:
            ser.write(STOP_CMD)
            time.sleep(0.05)
            ser.reset_input_buffer()
            ser.write(SIT_CMD)
            self.log.emit("SIT komutu gönderildi — telemetri bekleniyor...")

            rx_buf = bytearray()
            while self._running:
                n = ser.in_waiting
                if n:
                    rx_buf.extend(ser.read(n))

                pkt = extract_sit_packet(rx_buf)
                if pkt is not None:
                    count += 1
                    self.packet_ready.emit(
                        pkt['alt'],      pkt['pressure'],
                        pkt['ax'],       pkt['ay'],      pkt['az'],
                        pkt['pitch'],    pkt['roll'],    pkt['yaw'],
                        pkt['gps_alt'],  pkt['lat'],     pkt['lon'],
                        pkt['vel'],      pkt['status'],
                        pkt['gyro_x'],   pkt['gyro_y'],  pkt['gyro_z'],
                    )
                else:
                    time.sleep(self._READ_POLL_S)

        except Exception as exc:
            self.log.emit(f"Hata: {exc}")

        finally:
            try:
                ser.write(STOP_CMD)
            except Exception:
                pass
            ser.close()

        self.log.emit(f"SIT tamamlandı — {count} paket alındı")
        self.finished.emit(count)
