"""
MainWindow — SKYRTOS Ground Station ana penceresi.
SIT / SUT mod geçişi, seri bağlantı, grafik ve roket yönelimi.
"""
import csv
import os
import time
from collections import deque
from datetime import datetime

import pyqtgraph as pg
import serial.tools.list_ports
from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (
    QMainWindow, QWidget, QHBoxLayout, QVBoxLayout,
    QLabel, QComboBox, QPushButton, QLineEdit,
    QProgressBar, QPlainTextEdit, QFileDialog,
    QGroupBox, QSplitter, QStackedWidget, QGridLayout,
    QFrame, QSizePolicy,
)

from protocol import load_csv, build_windows, status_to_phase
from serial_worker import SerialWorker
from sit_worker import SitWorker
from plot_widget import SutPlotWidget
from rocket_widget import RocketWidget

pg.setConfigOptions(antialias=False, useOpenGL=False)

# ── Renk paleti ──────────────────────────────────────────────────────────────
_BG     = "#1e1e1e"
_PANEL  = "#252525"
_BORDER = "#444444"
_TEXT   = "#e0e0e0"
_DIM    = "#909090"
_GREEN  = "#4caf50"
_RED    = "#ef5350"
_ACCENT = "#4fc3f7"


def _grp_style() -> str:
    return (
        f"QGroupBox{{background:{_PANEL}; border:1px solid {_BORDER}; border-radius:4px; "
        f"margin-top:12px; padding:8px; color:{_TEXT}; font-size:13px; font-weight:bold;}}"
        f"QGroupBox::title{{subcontrol-origin:margin; left:8px; padding:0 4px;}}"
    )


def _btn_style(bg: str = "#2d4a6d", hover: str = "#3a5f8a") -> str:
    return (
        f"QPushButton{{background:{bg}; color:{_TEXT}; border:1px solid {_BORDER}; "
        f"border-radius:3px; padding:6px 12px; font-size:13px;}}"
        f"QPushButton:hover{{background:{hover};}}"
        f"QPushButton:checked{{background:{_ACCENT}; color:#000;}}"
        f"QPushButton:disabled{{background:#2a2a2a; color:#555; border-color:#333;}}"
    )


def _combo_style() -> str:
    return (
        f"QComboBox{{background:#2a2a2a; color:{_TEXT}; border:1px solid {_BORDER}; "
        f"border-radius:3px; padding:4px 8px; font-size:13px;}}"
        f"QComboBox::drop-down{{border:none; width:20px;}}"
        f"QComboBox QAbstractItemView{{background:#2a2a2a; color:{_TEXT}; "
        f"selection-background-color:#3a3a3a;}}"
    )


# ── SIT sensör görünümü ──────────────────────────────────────────────────────
class _SitView(QWidget):
    """SIT modu merkez paneli: sensör tablosu + altitude grafiği."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setStyleSheet(f"background:{_BG};")
        vl = QVBoxLayout(self)
        vl.setContentsMargins(8, 8, 8, 8)
        vl.setSpacing(8)

        grp = QGroupBox("Sensör Verileri")
        grp.setStyleSheet(_grp_style())
        grid = QGridLayout(grp)
        grid.setHorizontalSpacing(12)
        grid.setVerticalSpacing(8)
        grid.setColumnMinimumWidth(2, 16)   # iki sütun arası boşluk

        self._vals: dict[str, QLabel] = {}

        def _row(key: str, label: str, r: int, col: int):
            lbl = QLabel(label)
            lbl.setStyleSheet(f"color:{_DIM}; font-size:15px;")
            val = QLabel("—")
            val.setStyleSheet(
                f"color:{_TEXT}; font-size:18px; "
                f"font-family:Consolas,monospace; font-weight:bold;"
            )
            val.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
            grid.addWidget(lbl, r, col)
            grid.addWidget(val, r, col + 1)
            self._vals[key] = val

        # Sol sütun
        _row("alt",      "Altitude AGL (m)",  0, 0)
        _row("vel",      "Hız (m/s)",          1, 0)
        _row("pressure", "Basınç (hPa)",       2, 0)
        _row("gps_alt",  "GPS Alt (m)",        3, 0)
        _row("lat",      "Enlem (°)",          4, 0)
        _row("lon",      "Boylam (°)",         5, 0)

        # Sağ sütun
        _row("ax",     "İvme X (m/s²)",   0, 3)
        _row("ay",     "İvme Y (m/s²)",   1, 3)
        _row("az",     "İvme Z (m/s²)",   2, 3)
        _row("gyro_x", "Gyro X (rad/s)",  3, 3)
        _row("gyro_y", "Gyro Y (rad/s)",  4, 3)
        _row("gyro_z", "Gyro Z (rad/s)",  5, 3)

        vl.addWidget(grp)

        # Altitude grafiği
        pw = pg.GraphicsLayoutWidget()
        pw.setBackground(_BG)
        pw.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        p = pw.addPlot(title="Altitude AGL — Canlı")
        p.setLabel('left', 'm')
        p.setLabel('bottom', 'Zaman (s)')
        p.showGrid(x=True, y=True, alpha=0.3)
        p.getAxis('left').setTextPen(_TEXT)
        p.getAxis('bottom').setTextPen(_TEXT)
        self._curve = p.plot(pen=pg.mkPen(color=_ACCENT, width=2))
        vl.addWidget(pw, 1)

        self._t_buf:   deque[float] = deque(maxlen=600)
        self._alt_buf: deque[float] = deque(maxlen=600)

    def update_packet(self, alt, pressure, ax, ay, az,
                      gps_alt, lat, lon, vel,
                      gyro_x, gyro_y, gyro_z, _status, elapsed: float):
        self._vals["alt"].setText(f"{alt:.2f}")
        self._vals["vel"].setText(f"{vel:.2f}")
        self._vals["pressure"].setText(f"{pressure:.2f}")
        self._vals["gps_alt"].setText(f"{gps_alt:.2f}")
        self._vals["lat"].setText(f"{lat:.6f}")
        self._vals["lon"].setText(f"{lon:.6f}")
        self._vals["ax"].setText(f"{ax:.3f}")
        self._vals["ay"].setText(f"{ay:.3f}")
        self._vals["az"].setText(f"{az:.3f}")
        self._vals["gyro_x"].setText(f"{gyro_x:.4f}")
        self._vals["gyro_y"].setText(f"{gyro_y:.4f}")
        self._vals["gyro_z"].setText(f"{gyro_z:.4f}")

        self._t_buf.append(elapsed)
        self._alt_buf.append(alt)
        self._curve.setData(list(self._t_buf), list(self._alt_buf))

    def reset(self):
        self._t_buf.clear()
        self._alt_buf.clear()
        self._curve.setData([], [])
        for v in self._vals.values():
            v.setText("—")


# ── Ana pencere ───────────────────────────────────────────────────────────────
class MainWindow(QMainWindow):
    _MODE_SIT = 0
    _MODE_SUT = 1

    def __init__(self):
        super().__init__()
        self.setWindowTitle("SKYRTOS Ground Station")
        self.resize(1400, 780)
        self.setStyleSheet(f"background:{_BG}; color:{_TEXT};")

        self._mode    = self._MODE_SIT
        self._worker: SerialWorker | SitWorker | None = None
        self._windows: list[list[dict]] = []
        self._results: list[dict] = []

        self._build_ui()
        self._refresh_ports()
        self._apply_mode(self._MODE_SIT)

    # ── UI inşası ─────────────────────────────────────────────────────────────
    def _build_ui(self):
        root = QWidget()
        root_vl = QVBoxLayout(root)
        root_vl.setContentsMargins(0, 0, 0, 0)
        root_vl.setSpacing(0)

        # Başlık bandı
        header = QFrame()
        header.setFixedHeight(44)
        header.setStyleSheet(
            f"QFrame{{background:{_PANEL}; border-bottom:1px solid {_BORDER};}}"
        )
        hl = QHBoxLayout(header)
        hl.setContentsMargins(16, 0, 16, 0)
        title = QLabel("SKYRTOS  Ground Station")
        title.setStyleSheet(
            f"color:{_ACCENT}; font-size:15px; font-weight:bold;"
        )
        hl.addWidget(title)
        hl.addStretch(1)
        sub = QLabel("STM32F446 · FreeRTOS · BMI088 + BME280 + L86")
        sub.setStyleSheet(f"color:{_DIM}; font-size:12px;")
        hl.addWidget(sub)
        root_vl.addWidget(header)

        splitter = QSplitter(Qt.Horizontal)
        splitter.setHandleWidth(2)
        splitter.setStyleSheet(f"QSplitter::handle{{background:{_BORDER};}}")
        splitter.addWidget(self._build_left())
        splitter.addWidget(self._build_center())
        splitter.addWidget(self._build_right())
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)
        splitter.setStretchFactor(2, 0)
        root_vl.addWidget(splitter, 1)
        self.setCentralWidget(root)

    # ── Sol panel ─────────────────────────────────────────────────────────────
    def _build_left(self) -> QWidget:
        w = QWidget()
        w.setFixedWidth(270)
        w.setStyleSheet(f"background:{_PANEL};")
        vl = QVBoxLayout(w)
        vl.setContentsMargins(10, 10, 10, 10)
        vl.setSpacing(8)

        # Bağlantı
        grp_conn = QGroupBox("Bağlantı")
        grp_conn.setStyleSheet(_grp_style())
        gv = QVBoxLayout(grp_conn)
        gv.setSpacing(6)

        row_port = QHBoxLayout()
        lbl_p = QLabel("Port")
        lbl_p.setStyleSheet(f"color:{_DIM}; font-size:13px;")
        lbl_p.setFixedWidth(36)
        row_port.addWidget(lbl_p)
        self.cb_port = QComboBox()
        self.cb_port.setStyleSheet(_combo_style())
        row_port.addWidget(self.cb_port, 1)
        btn_ref = QPushButton("↺")
        btn_ref.setFixedWidth(30)
        btn_ref.setStyleSheet(_btn_style())
        btn_ref.clicked.connect(self._refresh_ports)
        row_port.addWidget(btn_ref)
        gv.addLayout(row_port)

        row_baud = QHBoxLayout()
        lbl_b = QLabel("Baud")
        lbl_b.setStyleSheet(f"color:{_DIM}; font-size:13px;")
        lbl_b.setFixedWidth(36)
        row_baud.addWidget(lbl_b)
        self.cb_baud = QComboBox()
        self.cb_baud.setStyleSheet(_combo_style())
        for b in ("115200", "230400", "460800"):
            self.cb_baud.addItem(b)
        self.cb_baud.setCurrentText("230400")
        row_baud.addWidget(self.cb_baud, 1)
        gv.addLayout(row_baud)

        self.btn_connect = QPushButton("Bağlan")
        self.btn_connect.setCheckable(True)
        self.btn_connect.setStyleSheet(_btn_style())
        self.btn_connect.clicked.connect(self._on_connect_toggle)
        gv.addWidget(self.btn_connect)

        self.lbl_status = QLabel("● Bağlı değil")
        self.lbl_status.setStyleSheet(f"color:{_RED}; font-size:13px;")
        gv.addWidget(self.lbl_status)
        vl.addWidget(grp_conn)

        # Mod seçimi
        grp_mode = QGroupBox("Test Modu")
        grp_mode.setStyleSheet(_grp_style())
        mv = QHBoxLayout(grp_mode)
        mv.setSpacing(6)
        self.btn_sit = QPushButton("SIT")
        self.btn_sit.setCheckable(True)
        self.btn_sit.setStyleSheet(_btn_style())
        self.btn_sit.clicked.connect(lambda: self._apply_mode(self._MODE_SIT))
        self.btn_sut = QPushButton("SUT")
        self.btn_sut.setCheckable(True)
        self.btn_sut.setStyleSheet(_btn_style())
        self.btn_sut.clicked.connect(lambda: self._apply_mode(self._MODE_SUT))
        mv.addWidget(self.btn_sit)
        mv.addWidget(self.btn_sut)
        vl.addWidget(grp_mode)

        # CSV (yalnızca SUT)
        self.grp_csv = QGroupBox("CSV Senaryosu")
        self.grp_csv.setStyleSheet(_grp_style())
        cv = QVBoxLayout(self.grp_csv)
        cv.setSpacing(6)
        self.le_csv = QLineEdit()
        self.le_csv.setPlaceholderText("Dosya seç...")
        self.le_csv.setReadOnly(True)
        self.le_csv.setStyleSheet(
            f"background:#2a2a2a; color:{_TEXT}; border:1px solid {_BORDER}; "
            f"border-radius:3px; padding:4px; font-size:12px;"
        )
        cv.addWidget(self.le_csv)
        btn_browse = QPushButton("Gözat...")
        btn_browse.setStyleSheet(_btn_style())
        btn_browse.clicked.connect(self._browse_csv)
        cv.addWidget(btn_browse)
        self.lbl_csv_info = QLabel("")
        self.lbl_csv_info.setStyleSheet(f"color:{_DIM}; font-size:12px;")
        self.lbl_csv_info.setWordWrap(True)
        cv.addWidget(self.lbl_csv_info)
        vl.addWidget(self.grp_csv)

        # Kontrol
        grp_ctrl = QGroupBox("Kontrol")
        grp_ctrl.setStyleSheet(_grp_style())
        ctv = QVBoxLayout(grp_ctrl)
        ctv.setSpacing(6)
        self.btn_start = QPushButton("Başlat")
        self.btn_start.setEnabled(False)
        self.btn_start.setStyleSheet(_btn_style("#1b4d1f", "#2e7d32"))
        self.btn_start.clicked.connect(self._on_start)
        ctv.addWidget(self.btn_start)
        self.btn_stop = QPushButton("Durdur")
        self.btn_stop.setEnabled(False)
        self.btn_stop.setStyleSheet(_btn_style("#5a1111", "#c62828"))
        self.btn_stop.clicked.connect(self._on_stop)
        ctv.addWidget(self.btn_stop)
        self.btn_save = QPushButton("Kaydet")
        self.btn_save.setEnabled(False)
        self.btn_save.setStyleSheet(_btn_style())
        self.btn_save.clicked.connect(self._on_save)
        ctv.addWidget(self.btn_save)
        vl.addWidget(grp_ctrl)

        # İlerleme (SUT)
        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setValue(0)
        self.progress.setStyleSheet(
            f"QProgressBar{{background:#2a2a2a; border:1px solid {_BORDER}; "
            f"border-radius:3px; color:{_TEXT}; text-align:center; font-size:12px;}}"
            f"QProgressBar::chunk{{background:{_ACCENT}; border-radius:2px;}}"
        )
        self.progress.setFixedHeight(20)
        vl.addWidget(self.progress)

        # Log
        self.log_box = QPlainTextEdit()
        self.log_box.setReadOnly(True)
        self.log_box.setMaximumBlockCount(600)
        self.log_box.setStyleSheet(
            f"background:#1a1a1a; color:#aaaaaa; "
            f"font-family:Consolas,monospace; font-size:12px; "
            f"border:1px solid {_BORDER}; border-radius:3px;"
        )
        vl.addWidget(self.log_box, 1)

        return w

    # ── Merkez panel ──────────────────────────────────────────────────────────
    def _build_center(self) -> QWidget:
        self._stack = QStackedWidget()
        self._stack.setStyleSheet(f"background:{_BG};")
        self._sit_view = _SitView()
        self._stack.addWidget(self._sit_view)   # index 0
        self._sut_plot = SutPlotWidget()
        self._stack.addWidget(self._sut_plot)   # index 1
        return self._stack

    # ── Sağ panel ─────────────────────────────────────────────────────────────
    def _build_right(self) -> QWidget:
        w = QWidget()
        w.setFixedWidth(270)
        w.setStyleSheet(f"background:{_PANEL};")
        vl = QVBoxLayout(w)
        vl.setContentsMargins(10, 10, 10, 10)
        vl.setSpacing(8)

        hdr = QLabel("Roket Yönelimi")
        hdr.setStyleSheet(
            f"color:{_TEXT}; font-size:13px; font-weight:bold;"
        )
        vl.addWidget(hdr)

        self.rocket = RocketWidget()
        vl.addWidget(self.rocket, 1)

        return w

    # ── Mod geçişi ────────────────────────────────────────────────────────────
    def _apply_mode(self, mode: int):
        self._mode = mode
        is_sit = (mode == self._MODE_SIT)
        self.btn_sit.setChecked(is_sit)
        self.btn_sut.setChecked(not is_sit)
        self.grp_csv.setVisible(not is_sit)
        self.progress.setVisible(not is_sit)
        self._stack.setCurrentIndex(mode)
        self._update_start_btn()

    # ── Event handlers ────────────────────────────────────────────────────────
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
                f"{len(rows)} satır · {len(self._windows)} pencere · {dur:.1f} s"
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
            self.lbl_status.setStyleSheet(f"color:{_GREEN}; font-size:13px;")
            self.cb_port.setEnabled(False)
            self.cb_baud.setEnabled(False)
        else:
            self._on_stop()
            self.btn_connect.setText("Bağlan")
            self.lbl_status.setText("● Bağlı değil")
            self.lbl_status.setStyleSheet(f"color:{_RED}; font-size:13px;")
            self.cb_port.setEnabled(True)
            self.cb_baud.setEnabled(True)
        self._update_start_btn()

    def _on_start(self):
        self._results.clear()
        self._sit_start_time: float | None = None
        self.rocket.reset()
        self.btn_start.setEnabled(False)
        self.btn_stop.setEnabled(True)
        self.btn_save.setEnabled(False)

        port = self.cb_port.currentText()
        baud = int(self.cb_baud.currentText())

        if self._mode == self._MODE_SIT:
            self._sit_view.reset()
            self._log("SIT testi başlatılıyor...")
            self._worker = SitWorker(port=port, baudrate=baud)
            self._worker.packet_ready.connect(self._on_sit_packet)
            self._worker.log.connect(self._log)
            self._worker.finished.connect(self._on_sit_finished)
            self._worker.start()
        else:
            if not self._windows:
                self._log("Önce CSV dosyası seç.")
                self.btn_start.setEnabled(True)
                self.btn_stop.setEnabled(False)
                return
            self._sut_plot.reset()
            self.progress.setValue(0)
            self._log(f"SUT testi başlatılıyor — {len(self._windows)} pencere")
            self._worker = SerialWorker(port=port, baudrate=baud, windows=self._windows)
            self._worker.sent_point.connect(self._on_sut_sent)
            self._worker.result_ready.connect(self._on_sut_result)
            self._worker.progress.connect(self.progress.setValue)
            self._worker.log.connect(self._log)
            self._worker.finished.connect(self._on_sut_finished)
            self._worker.start()

    def _on_stop(self):
        if self._worker and self._worker.isRunning():
            self._worker.stop()
            self._worker.wait(2000)
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)

    # ── SIT callbacks ─────────────────────────────────────────────────────────
    def _on_sit_packet(self, alt, pressure, ax, ay, az,
                       pitch, roll, yaw, gps_alt, lat, lon, vel, status,
                       gyro_x, gyro_y, gyro_z):
        now = time.monotonic()
        if self._sit_start_time is None:
            self._sit_start_time = now
        elapsed = round(now - self._sit_start_time, 3)

        self._sit_view.update_packet(alt, pressure, ax, ay, az,
                                     gps_alt, lat, lon, vel,
                                     gyro_x, gyro_y, gyro_z, status, elapsed)
        self.rocket.set_orientation(roll, pitch, yaw)
        phase = status_to_phase(status)
        self._results.append(dict(
            time=elapsed,
            alt=alt, pressure=pressure,
            ax=ax, ay=ay, az=az,
            gyro_x=gyro_x, gyro_y=gyro_y, gyro_z=gyro_z,
            gps_alt=gps_alt, lat=lat, lon=lon,
            vel=vel, status=status, phase=phase,
        ))
        if len(self._results) == 1:
            self.btn_save.setEnabled(True)

    def _on_sit_finished(self, count: int):
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)

    # ── SUT callbacks ─────────────────────────────────────────────────────────
    def _on_sut_sent(self, sim_time: float, alt: float):
        self._sut_plot.append_sent(sim_time, alt)

    def _on_sut_result(self, sim_time: float, alt: float,
                       roll: float, pitch: float, yaw: float, status: int):
        self._results.append(dict(
            sim_time=sim_time, alt=alt,
            roll=roll, pitch=pitch, yaw=yaw,
            status=status, phase=status_to_phase(status),
        ))
        self._sut_plot.append_recv(sim_time, alt, status)
        self.rocket.set_orientation(roll, pitch, yaw)

    def _on_sut_finished(self, sent: int, received: int):
        self.btn_start.setEnabled(True)
        self.btn_stop.setEnabled(False)
        if self._results:
            self.btn_save.setEnabled(True)

    # ── Kaydet ────────────────────────────────────────────────────────────────
    def _on_save(self):
        if not self._results:
            return
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        os.makedirs("logs", exist_ok=True)

        if self._mode == self._MODE_SIT:
            path = os.path.join("logs", f"SIT_log_{ts}.csv")
            fields = ["time", "alt", "pressure", "ax", "ay", "az",
                      "gyro_x", "gyro_y", "gyro_z", "gps_alt", "lat", "lon",
                      "vel", "status", "phase"]
        else:
            path = os.path.join("logs", f"SUT_result_{ts}.csv")
            fields = ["sim_time", "alt", "roll", "pitch", "yaw", "status", "phase"]

        with open(path, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields, extrasaction='ignore')
            writer.writeheader()
            writer.writerows(self._results)
        self._log(f"Kaydedildi: {path}")

    # ── Yardımcılar ───────────────────────────────────────────────────────────
    def _update_start_btn(self):
        connected = self.btn_connect.isChecked()
        running   = bool(self._worker and self._worker.isRunning())
        if self._mode == self._MODE_SIT:
            ok = connected and not running
        else:
            ok = connected and bool(self._windows) and not running
        self.btn_start.setEnabled(ok)

    def _log(self, msg: str):
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.log_box.appendPlainText(f"[{ts}] {msg}")
