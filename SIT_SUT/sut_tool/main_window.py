"""
MainWindow — SUT test aracının ana penceresi.

Sol panel (dar):   port, baud, CSV seçimi | bağlan | başlat/durdur | ilerleme | log
Orta panel:        iki altitude grafiği (gönderilen + filtreli)
Sağ panel (kare):  3D roket yönelim görselleştirmesi
"""
import csv
import os
<<<<<<< HEAD
=======
import time
from collections import deque
>>>>>>> 2c7440e (SIT loguna time eklendi)
from datetime import datetime

import serial.tools.list_ports
from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (
    QMainWindow, QWidget, QHBoxLayout, QVBoxLayout,
    QLabel, QComboBox, QPushButton, QLineEdit,
    QProgressBar, QPlainTextEdit, QFileDialog,
    QGroupBox, QSplitter,
)

from protocol import load_csv, build_windows, status_to_phase
from serial_worker import SerialWorker
from plot_widget import SutPlotWidget
from rocket_widget import RocketWidget


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SUT Test Aracı")
        self.resize(1400, 720)
        self.setStyleSheet("background:#252526; color:#d4d4d4;")

        self._worker: SerialWorker | None = None
        self._windows: list[list[dict]] = []
        self._results: list[dict] = []

        self._build_ui()
        self._refresh_ports()

    # ─────────────────────────────────────────────────────────────────────
    # UI inşası
    # ─────────────────────────────────────────────────────────────────────
    def _build_ui(self):
        splitter = QSplitter(Qt.Horizontal)
        splitter.setHandleWidth(4)

        # ── Sol panel: kontroller ─────────────────────────────────────────
        left = QWidget()
        left.setFixedWidth(290)
        left.setStyleSheet("background:#2d2d30;")
        lv = QVBoxLayout(left)
        lv.setContentsMargins(10, 10, 10, 10)
        lv.setSpacing(8)

        # Bağlantı
        grp_conn = QGroupBox("Bağlantı")
        grp_conn.setStyleSheet(self._grp_style())
        gv = QVBoxLayout(grp_conn)

        row_port = QHBoxLayout()
        row_port.addWidget(QLabel("Port:"))
        self.cb_port = QComboBox()
        self.cb_port.setMinimumWidth(90)
        row_port.addWidget(self.cb_port, 1)
        btn_ref = QPushButton("↺")
        btn_ref.setFixedWidth(28)
        btn_ref.clicked.connect(self._refresh_ports)
        row_port.addWidget(btn_ref)
        gv.addLayout(row_port)

        row_baud = QHBoxLayout()
        row_baud.addWidget(QLabel("Baud:"))
        self.cb_baud = QComboBox()
        for b in ("115200", "230400", "460800"):
            self.cb_baud.addItem(b)
        self.cb_baud.setCurrentText("230400")
        row_baud.addWidget(self.cb_baud, 1)
        gv.addLayout(row_baud)

        self.btn_connect = QPushButton("Bağlan")
        self.btn_connect.setCheckable(True)
        self.btn_connect.clicked.connect(self._on_connect_toggle)
        self.btn_connect.setStyleSheet(self._btn_style())
        gv.addWidget(self.btn_connect)

        self.lbl_status = QLabel("● Bağlı değil")
        self.lbl_status.setStyleSheet("color:#ef5350;")
        gv.addWidget(self.lbl_status)
        lv.addWidget(grp_conn)

        # CSV
        grp_csv = QGroupBox("CSV Dosyası")
        grp_csv.setStyleSheet(self._grp_style())
        cv = QVBoxLayout(grp_csv)

        self.le_csv = QLineEdit()
        self.le_csv.setPlaceholderText("Dosya seç...")
        self.le_csv.setReadOnly(True)
        cv.addWidget(self.le_csv)

        btn_browse = QPushButton("Gözat...")
        btn_browse.clicked.connect(self._browse_csv)
        btn_browse.setStyleSheet(self._btn_style())
        cv.addWidget(btn_browse)

        self.lbl_csv_info = QLabel("")
        self.lbl_csv_info.setStyleSheet("color:#9e9e9e; font-size:11px;")
        self.lbl_csv_info.setWordWrap(True)
        cv.addWidget(self.lbl_csv_info)
        lv.addWidget(grp_csv)

        # Kontrol
        grp_ctrl = QGroupBox("Kontrol")
        grp_ctrl.setStyleSheet(self._grp_style())
        ctv = QVBoxLayout(grp_ctrl)

        self.btn_start = QPushButton("▶  Başlat")
        self.btn_start.setEnabled(False)
        self.btn_start.clicked.connect(self._on_start)
        self.btn_start.setStyleSheet(self._btn_style("#2e7d32", "#1b5e20"))
        ctv.addWidget(self.btn_start)

        self.btn_stop = QPushButton("■  Durdur")
        self.btn_stop.setEnabled(False)
        self.btn_stop.clicked.connect(self._on_stop)
        self.btn_stop.setStyleSheet(self._btn_style("#c62828", "#b71c1c"))
        ctv.addWidget(self.btn_stop)

        self.btn_save = QPushButton("💾  Kaydet")
        self.btn_save.setEnabled(False)
        self.btn_save.clicked.connect(self._on_save)
        self.btn_save.setStyleSheet(self._btn_style())
        ctv.addWidget(self.btn_save)
        lv.addWidget(grp_ctrl)

        # İlerleme
        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setValue(0)
        self.progress.setStyleSheet(
            "QProgressBar{background:#1e1e1e; border:1px solid #555; "
            "border-radius:3px; color:#fff; text-align:center;}"
            "QProgressBar::chunk{background:#0288d1;}"
        )
        lv.addWidget(self.progress)

        # Log
        self.log_box = QPlainTextEdit()
        self.log_box.setReadOnly(True)
        self.log_box.setMaximumBlockCount(500)
        self.log_box.setStyleSheet(
            "background:#1e1e1e; color:#b0bec5; "
            "font-family:Consolas,monospace; font-size:11px; "
            "border:1px solid #444;"
        )
        lv.addWidget(self.log_box, 1)

        splitter.addWidget(left)

        # ── Orta panel: grafikler ─────────────────────────────────────────
        self.plot = SutPlotWidget()
        splitter.addWidget(self.plot)

        # ── Sağ panel: 3D roket ───────────────────────────────────────────
        self.rocket = RocketWidget()
        self.rocket.setFixedWidth(280)
        splitter.addWidget(self.rocket)

        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        splitter.setStretchFactor(2, 0)

        self.setCentralWidget(splitter)

    # ─────────────────────────────────────────────────────────────────────
    # Event handlers
    # ─────────────────────────────────────────────────────────────────────
    def _refresh_ports(self):
        current = self.cb_port.currentText()
        self.cb_port.clear()
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.cb_port.addItems(ports)
        if current in ports:
            self.cb_port.setCurrentText(current)

    def _browse_csv(self):
        path, _ = QFileDialog.getOpenFileName(self, "CSV Dosyası Seç", "", "CSV (*.csv)")
        if not path:
            return
        self.le_csv.setText(path)
        try:
            rows = load_csv(path)
            self._windows = build_windows(rows)
            dur = rows[-1]['time'] - rows[0]['time'] if len(rows) > 1 else 0.0
            self.lbl_csv_info.setText(
                f"{len(rows)} satır | {len(self._windows)} pencere | {dur:.1f} s"
            )
            self._log(f"CSV: {os.path.basename(path)}")
            self._update_start_btn()
        except Exception as exc:
            self._log(f"CSV hatası: {exc}")
            self._windows = []
            self.lbl_csv_info.setText("Hata — dosya okunamadı")

    def _on_connect_toggle(self, checked: bool):
        if checked:
            self.btn_connect.setText("Bağlantıyı Kes")
            self.lbl_status.setText("● Bağlandı")
            self.lbl_status.setStyleSheet("color:#66bb6a;")
            self.cb_port.setEnabled(False)
            self.cb_baud.setEnabled(False)
        else:
            self._on_stop()
            self.btn_connect.setText("Bağlan")
            self.lbl_status.setText("● Bağlı değil")
            self.lbl_status.setStyleSheet("color:#ef5350;")
            self.cb_port.setEnabled(True)
            self.cb_baud.setEnabled(True)
        self._update_start_btn()

    def _on_start(self):
        if not self._windows:
            self._log("Önce CSV dosyası seç.")
            return
        self._results.clear()
<<<<<<< HEAD
        self.plot.reset()
=======
        self._sit_start_time: float | None = None
>>>>>>> 2c7440e (SIT loguna time eklendi)
        self.rocket.reset()
        self.progress.setValue(0)
        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(True)
        self.btn_save.setEnabled(False)
        self._log(f"Test başlıyor — {len(self._windows)} pencere")

        self._worker = SerialWorker(
            port     = self.cb_port.currentText(),
            baudrate = int(self.cb_baud.currentText()),
            windows  = self._windows,
        )
        self._worker.sent_point.connect(self._on_sent)
        self._worker.result_ready.connect(self._on_result)
        self._worker.progress.connect(self.progress.setValue)
        self._worker.log.connect(self._log)
        self._worker.finished.connect(self._on_finished)
        self._worker.start()

    def _on_stop(self):
        if self._worker and self._worker.isRunning():
            self._worker.stop()
            self._worker.wait(2000)
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)

<<<<<<< HEAD
    def _on_sent(self, sim_time: float, alt: float):
        self.plot.append_sent(sim_time, alt)
=======
    # ── SIT callbacks ─────────────────────────────────────────────────────────
    def _on_sit_packet(self, alt, pressure, ax, ay, az,
                       pitch, roll, yaw, gps_alt, lat, lon, vel, status):
        now = time.monotonic()
        if self._sit_start_time is None:
            self._sit_start_time = now
        elapsed = round(now - self._sit_start_time, 3)

        self._sit_view.update_packet(alt, pressure, ax, ay, az,
                                     pitch, roll, yaw, gps_alt, lat, lon, vel, status)
        self.rocket.set_orientation(roll, pitch, yaw)
        phase = status_to_phase(status)
        self._results.append(dict(
            time=elapsed,
            alt=alt, pressure=pressure,
            ax=ax, ay=ay, az=az,
            pitch=pitch, roll=roll, yaw=yaw,
            gps_alt=gps_alt, lat=lat, lon=lon,
            vel=vel, status=status, phase=phase,
        ))
        if len(self._results) == 1:
            self.btn_save.setEnabled(True)
>>>>>>> 2c7440e (SIT loguna time eklendi)

    def _on_result(self, sim_time: float, alt: float,
                   roll: float, pitch: float, yaw: float,
                   status: int):
        self._results.append(dict(
            sim_time=sim_time, alt=alt,
            roll=roll, pitch=pitch, yaw=yaw,
            status=status, phase=status_to_phase(status),
        ))
        self.plot.append_recv(sim_time, alt, status)
        self.rocket.set_orientation(roll, pitch, yaw)

    def _on_finished(self, sent: int, received: int):
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)
        if self._results:
            self.btn_save.setEnabled(True)

    def _on_save(self):
        if not self._results:
            return
        ts   = datetime.now().strftime("%Y%m%d_%H%M%S")
        path = os.path.join("logs", f"SUT_result_{ts}.csv")
        os.makedirs("logs", exist_ok=True)
<<<<<<< HEAD
=======

        if self._mode == self._MODE_SIT:
            path = os.path.join("logs", f"SIT_log_{ts}.csv")
            fields = ["time", "alt", "pressure", "ax", "ay", "az",
                      "pitch", "roll", "yaw", "gps_alt", "lat", "lon",
                      "vel", "status", "phase"]
        else:
            path = os.path.join("logs", f"SUT_result_{ts}.csv")
            fields = ["sim_time", "alt", "roll", "pitch", "yaw", "status", "phase"]

>>>>>>> 2c7440e (SIT loguna time eklendi)
        with open(path, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(
                f, fieldnames=["sim_time", "alt", "roll", "pitch", "yaw",
                               "status", "phase"]
            )
            writer.writeheader()
            writer.writerows(self._results)
        self._log(f"Kaydedildi: {path}")

    # ─────────────────────────────────────────────────────────────────────
    # Yardımcılar
    # ─────────────────────────────────────────────────────────────────────
    def _update_start_btn(self):
        ok = (self.btn_connect.isChecked() and bool(self._windows)
              and not (self._worker and self._worker.isRunning()))
        self.btn_start.setEnabled(ok)

    def _log(self, msg: str):
<<<<<<< HEAD
        self.log_box.appendPlainText(msg)

    @staticmethod
    def _grp_style() -> str:
        return (
            "QGroupBox{border:1px solid #444; border-radius:4px; "
            "margin-top:6px; padding:6px; color:#b0bec5; font-weight:bold;}"
            "QGroupBox::title{subcontrol-origin:margin; left:8px;}"
        )

    @staticmethod
    def _btn_style(bg="#37474f", hover="#455a64") -> str:
        return (
            f"QPushButton{{background:{bg}; color:#fff; border:none; "
            f"border-radius:3px; padding:6px;}}"
            f"QPushButton:hover{{background:{hover};}}"
            f"QPushButton:disabled{{background:#333; color:#666;}}"
        )
=======
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.log_box.appendPlainText(f"[{ts}] {msg}")
>>>>>>> 2c7440e (SIT loguna time eklendi)
