"""
RocketWidget — QPainter ile perspektif projeksiyonlu 3D roket görselleştirmesi.

Eski sit_screen.py'deki RocketVisualization'dan PyQt5'e port edildi.
30 fps QTimer + smooth lerp animasyonu.
"""
import math
from PyQt5.QtWidgets import QWidget
from PyQt5.QtCore import Qt, QTimer, QPoint
from PyQt5.QtGui import QPainter, QColor, QPen, QBrush, QPolygon, QFont

_N_SEG = 12   # silindir segment sayısı
_SMOOTH = 0.15


class RocketWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(240, 300)

        # Görüntülenen açılar (smooth interpolasyon sonrası)
        self.roll  = 0.0
        self.pitch = 0.0
        self.yaw   = 0.0

        # Hedef açılar (STM32'den gelen)
        self._tgt_roll  = 0.0
        self._tgt_pitch = 0.0
        self._tgt_yaw   = 0.0

        # Roket boyutları
        self._body_len = 130
        self._body_r   = 18
        self._cone_h   = 45
        self._fin_sz   = 32

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        self._timer.start(33)   # ~30 fps

    # ── Public API ─────────────────────────────────────────────────────────
    def set_orientation(self, roll: float, pitch: float, yaw: float):
        self._tgt_roll  = roll
        self._tgt_pitch = pitch
        self._tgt_yaw   = yaw

    def reset(self):
        self.roll = self.pitch = self.yaw = 0.0
        self._tgt_roll = self._tgt_pitch = self._tgt_yaw = 0.0
        self.update()

    # ── Animasyon ──────────────────────────────────────────────────────────
    def _tick(self):
        self.roll  += (self._tgt_roll  - self.roll)  * _SMOOTH
        self.pitch += (self._tgt_pitch - self.pitch) * _SMOOTH
        self.yaw   += (self._tgt_yaw   - self.yaw)   * _SMOOTH
        self.update()

    # ── Matematik ──────────────────────────────────────────────────────────
    def _rotate(self, x, y, z):
        """sit_screen.py'deki applyMatrix dönüşümü — değiştirilmedi."""
        r  = math.radians(self.roll)
        p  = math.radians(self.pitch)
        y_ = math.radians(-self.yaw)
        c2, s2 = math.cos(r),  math.sin(r)
        c1, s1 = math.cos(p),  math.sin(p)
        c3, s3 = math.cos(y_), math.sin(y_)
        xn = x*(c2*c3) + y*(s1*s3 + c1*c3*s2) + z*(c3*s1*s2 - c1*s3)
        yn = x*(-s2)   + y*(c1*c2)             + z*(c2*s1)
        zn = x*(c2*s3) + y*(c1*s2*s3 - c3*s1) + z*(c1*c3 + s1*s2*s3)
        return xn, yn, zn

    def _project(self, x, y, z, cx, cy):
        """Perspektif projeksiyon."""
        fov   = 400
        scale = fov / (fov + z)
        return cx + x * scale, cy - y * scale, z

    # ── Çizim ──────────────────────────────────────────────────────────────
    def paintEvent(self, _event):
        w, h  = self.width(), self.height()
        cx, cy = w / 2, h / 2

        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing, False)

        # Arka plan
        painter.fillRect(0, 0, w, h, QColor('#1e1e1e'))

        # Referans çizgileri
        painter.setPen(QPen(QColor('#333'), 1, Qt.DashLine))
        painter.drawLine(0, int(cy), w, int(cy))
        painter.drawLine(int(cx), 0, int(cx), h)

        # ── Geometri üret ─────────────────────────────────────────────────
        R  = self._body_r
        HL = self._body_len / 2

        # Silindir köşeleri: [alt0, üst0, alt1, üst1, ...]
        cyl = []
        for i in range(_N_SEG):
            a = 2 * math.pi * i / _N_SEG
            cx_, cz_ = R * math.cos(a), R * math.sin(a)
            cyl.append((cx_, -HL, cz_))   # alt
            cyl.append((cx_,  HL, cz_))   # üst

        tip = (0.0, HL + self._cone_h, 0.0)   # koni ucu

        # Kanatlar (4 adet, üçgen)
        fins = []
        fh, fw = self._fin_sz, self._fin_sz * 0.8
        for i in range(4):
            a = math.pi / 4 + i * math.pi / 2
            dx, dz = math.cos(a), math.sin(a)
            fins.append([
                (R * dx,            -HL,           R * dz),
                ((R + fh) * dx,     -HL - fw*0.3,  (R + fh) * dz),
                (R * dx,            -HL + fw,      R * dz),
            ])

        # ── Döndür + Projeksiyon ───────────────────────────────────────────
        def rp(pt):
            return self._project(*self._rotate(*pt), cx, cy)

        rcyl  = [rp(v) for v in cyl]
        rtip  = rp(tip)
        rfins = [[rp(v) for v in fin] for fin in fins]

        # ── Yüz listesi oluştur ────────────────────────────────────────────
        faces = []
        N = _N_SEG

        # Silindir yan yüzleri (dörtgenler)
        for i in range(N):
            ni  = (i + 1) % N
            idx = [i*2, ni*2, ni*2+1, i*2+1]
            avg_z = sum(rcyl[j][2] for j in idx) / 4
            fa    = 2 * math.pi * i / N
            br    = max(0, min(255, int(80 + 60 * math.cos(fa - math.pi/4))))
            color = QColor(br, min(255, br + 30), min(255, br + 80))
            faces.append(('quad', idx, rcyl, avg_z, color))

        # Koni yüzleri (üçgenler, kırmızı/turuncu)
        for i in range(N):
            ni    = (i + 1) % N
            v1, v2 = i*2+1, ni*2+1
            avg_z  = (rcyl[v1][2] + rcyl[v2][2] + rtip[2]) / 3
            fa     = 2 * math.pi * i / N
            rv = max(0, min(255, int(200 + 55 * math.cos(fa - math.pi/4))))
            gv = max(0, min(255, int( 60 + 40 * math.cos(fa - math.pi/4))))
            faces.append(('cone', (v1, v2), rcyl, avg_z, QColor(rv, gv, 30), rtip))

        # Kanat yüzleri (gri)
        for i, rf in enumerate(rfins):
            avg_z = sum(v[2] for v in rf) / 3
            br    = 100 + (i * 20) % 40
            faces.append(('fin', rf, None, avg_z, QColor(br, br, min(255, br + 20))))

        # Ressam algoritması: arkadan öne sırala
        faces.sort(key=lambda f: f[3], reverse=True)

        # ── Çiz ───────────────────────────────────────────────────────────
        for face in faces:
            ft    = face[0]
            color = face[4]
            painter.setBrush(QBrush(color))

            if ft == 'quad':
                _, idx, verts, _, _ = face
                pts = QPolygon([QPoint(int(verts[j][0]), int(verts[j][1]))
                                for j in idx])
                painter.setPen(QPen(QColor('#444'), 1))
                painter.drawPolygon(pts)

            elif ft == 'cone':
                _, (v1, v2), verts, _, _, tip_pt = face
                pts = QPolygon([
                    QPoint(int(verts[v1][0]), int(verts[v1][1])),
                    QPoint(int(verts[v2][0]), int(verts[v2][1])),
                    QPoint(int(tip_pt[0]),    int(tip_pt[1])),
                ])
                painter.setPen(QPen(QColor('#aa3300'), 1))
                painter.drawPolygon(pts)

            elif ft == 'fin':
                _, rf, _, _, _ = face
                pts = QPolygon([QPoint(int(v[0]), int(v[1])) for v in rf])
                painter.setPen(QPen(QColor('#222'), 1))
                painter.drawPolygon(pts)

        # ── Açı etiketleri ─────────────────────────────────────────────────
        painter.setPen(QColor('#b0bec5'))
        painter.setFont(QFont('Consolas', 10))
        painter.drawText(8, h - 50, f"Roll:  {self.roll:+6.1f}°")
        painter.drawText(8, h - 33, f"Pitch: {self.pitch:+6.1f}°")
        painter.drawText(8, h - 16, f"Yaw:   {self.yaw:+6.1f}°")

        painter.end()
