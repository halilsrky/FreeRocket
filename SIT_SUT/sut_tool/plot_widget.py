"""
SutPlotWidget — PyQtGraph tabanlı canlı grafik.

Graf 1 (üst):  Gönderilen altitude — CSV'den (ham baro), gri
Graf 2 (alt):  Gelen filtreli altitude — STM32 Kalman çıkışı, yeşil

Altitude grafikleri x-ekseninde senkronize.
Faz geçişlerinde "gelen altitude" grafiğine dikey sarı çizgi + etiket.
"""
import pyqtgraph as pg
from PyQt5.QtCore import Qt
from protocol import status_to_phase

pg.setConfigOptions(antialias=False, useOpenGL=False)


class SutPlotWidget(pg.GraphicsLayoutWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setBackground('#1e1e1e')

        # ── Graf 1: Gönderilen altitude ───────────────────────────────────
        self.p_sent = self.addPlot(row=0, col=0, title="Gönderilen Altitude (CSV)")
        self._style_plot(self.p_sent, left='m', bottom='sim time  s')
        self.sent_curve = self.p_sent.plot(
            pen=pg.mkPen(color='#78909c', width=1),
        )

        # ── Graf 2: Gelen filtreli altitude ───────────────────────────────
        self.p_recv = self.addPlot(row=1, col=0, title="Kalman Filtreli Altitude (STM32)")
        self._style_plot(self.p_recv, left='m', bottom='sim time  s')
        self.recv_curve = self.p_recv.plot(
            pen=pg.mkPen(color='#00e676', width=2),
        )

        self._phase_lines: list[pg.InfiniteLine] = []
        self._last_phase = ""

        # X eksenlerini senkronize et
        self.p_recv.setXLink(self.p_sent)

        self._reset_data()

    # ── Yardımcılar ───────────────────────────────────────────────────────
    @staticmethod
    def _style_plot(p, left: str, bottom: str):
        p.setLabel('left', left)
        p.setLabel('bottom', bottom)
        p.showGrid(x=True, y=True, alpha=0.3)
        p.getAxis('left').setTextPen('w')
        p.getAxis('bottom').setTextPen('w')

    def _reset_data(self):
        self._t_sent:   list[float] = []
        self._alt_sent: list[float] = []
        self._t_recv:   list[float] = []
        self._alt_recv: list[float] = []

    # ── Sıfırla ───────────────────────────────────────────────────────────
    def reset(self):
        for line in self._phase_lines:
            self.p_recv.removeItem(line)
        self._phase_lines.clear()
        self._last_phase = ""
        self._reset_data()
        self.sent_curve.setData([], [])
        self.recv_curve.setData([], [])

    # ── Gönderilen nokta ──────────────────────────────────────────────────
    def append_sent(self, sim_time: float, alt: float):
        self._t_sent.append(sim_time)
        self._alt_sent.append(alt)
        self.sent_curve.setData(self._t_sent, self._alt_sent)

    # ── Gelen (filtreli) nokta ────────────────────────────────────────────
    def append_recv(self, sim_time: float, alt: float, status: int):
        self._t_recv.append(sim_time)
        self._alt_recv.append(alt)
        self.recv_curve.setData(self._t_recv, self._alt_recv)

        # Faz geçişi: dikey çizgi ekle
        phase = status_to_phase(status)
        if phase != self._last_phase:
            line = pg.InfiniteLine(
                pos=sim_time,
                angle=90,
                pen=pg.mkPen(color='#ffd600', width=1, style=Qt.DashLine),
                label=phase,
                labelOpts={
                    'position': 0.92,
                    'color': '#ffd600',
                    'fill': pg.mkBrush('#1e1e1e80'),
                    'movable': False,
                },
            )
            self.p_recv.addItem(line)
            self._phase_lines.append(line)
            self._last_phase = phase
