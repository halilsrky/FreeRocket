"""
SKYRTOS Ground Station — giriş noktası.

Gereksinimler:
    pip install pyqt5 pyqtgraph pyserial
"""
import sys
from PyQt5.QtWidgets import QApplication
from PyQt5.QtGui import QFont
from main_window import MainWindow


def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    app.setFont(QFont("Segoe UI", 10))
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
