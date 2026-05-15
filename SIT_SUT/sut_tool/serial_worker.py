"""
SerialWorker — QThread içinde SUT send/receive döngüsü.

Gönderme ve alma aynı thread'de sıralı: kilit yok, race yok.
Her pencere için:
  1. Gönder → sent_point sinyali (sim_time + sent_alt)
  2. Response bekle → result_ready sinyali (sim_time + filtered_alt + euler + status)
  3. 100 ms pencere süresinin kalanını bekle (gerçek zamanlı pacing)
"""
import time
import serial
from PyQt5.QtCore import QThread, pyqtSignal

from protocol import (
    SUT_CMD, STOP_CMD,
    build_combined_packet,
    extract_response,
    RESPONSE_HEADER,
)


class SerialWorker(QThread):
    # Gönderilen veri: sim_time, altitude (CSV'den)
    sent_point   = pyqtSignal(float, float)

    # STM32'den gelen: sim_time, alt, roll, pitch, yaw, status
    result_ready = pyqtSignal(float, float, float, float, float, int)

    progress = pyqtSignal(int)    # 0–100
    log      = pyqtSignal(str)
    finished = pyqtSignal(int, int)  # sent, received

    _STARTUP_DELAY_S = 3.0
    _RESP_TIMEOUT_S  = 0.5
    _READ_POLL_S     = 0.001
    _WINDOW_PERIOD_S = 0.100   # gerçek zamanlı pacing: 100 ms/pencere

    def __init__(self, port: str, baudrate: int,
                 windows: list[list[dict]], parent=None):
        super().__init__(parent)
        self._port     = port
        self._baudrate = baudrate
        self._windows  = windows
        self._running  = False

    def stop(self):
        self._running = False

    def run(self):
        self._running = True
        sent = received = 0
        total = len(self._windows)

        try:
            ser = serial.Serial(self._port, self._baudrate, timeout=self._RESP_TIMEOUT_S)
        except serial.SerialException as exc:
            self.log.emit(f"Port açılamadı: {exc}")
            self.finished.emit(0, 0)
            return

        try:
            ser.write(STOP_CMD)
            time.sleep(0.1)
            ser.reset_input_buffer()
            ser.reset_output_buffer()

            ser.write(SUT_CMD)
            self.log.emit("SUT komutu gönderildi, STM32 bekleniyor...")
            time.sleep(self._STARTUP_DELAY_S)
            ser.reset_input_buffer()
            self.log.emit(f"Başlandı — {total} pencere gönderilecek")

            rx_buf = bytearray()

            for i, window in enumerate(self._windows):
                if not self._running:
                    break

                t_window_start = time.monotonic()

                last_row = window[-1]
                sent_alt = last_row['altitude']
                sim_time = last_row['time']

                pkt = build_combined_packet(window)
                ser.write(pkt)
                sent += 1

                # Gönderilen noktayı hemen bildir
                self.sent_point.emit(sim_time, sent_alt)

                # Response bekle
                deadline = time.monotonic() + self._RESP_TIMEOUT_S
                resp = None
                while time.monotonic() < deadline and self._running:
                    n = ser.in_waiting
                    if n:
                        rx_buf.extend(ser.read(n))
                    resp = extract_response(rx_buf)
                    if resp is not None:
                        break
                    time.sleep(self._READ_POLL_S)

                if resp:
                    received += 1
                    self.result_ready.emit(
                        resp['sim_time'], resp['alt'],
                        resp['roll'], resp['pitch'], resp['yaw'],
                        resp['status'],
                    )
                else:
                    self.log.emit(
                        f"  Pencere {i+1}/{total} (t={sim_time:.3f} s): timeout"
                    )

                if (i + 1) % 10 == 0 or (i + 1) == total:
                    self.progress.emit(int((i + 1) * 100 / total))

                # Gerçek zamanlı pacing: 100 ms pencere süresinin kalanını bekle
                remaining = self._WINDOW_PERIOD_S - (time.monotonic() - t_window_start)
                if remaining > 0.001:
                    time.sleep(remaining)

        except Exception as exc:
            self.log.emit(f"Hata: {exc}")

        finally:
            try:
                ser.write(STOP_CMD)
            except Exception:
                pass
            ser.close()

        self.log.emit(
            f"Tamamlandı — gönderilen: {sent}, alınan: {received}, "
            f"kayıp: {sent - received}"
        )
        self.finished.emit(sent, received)
