"""
SUT Test Aracı — giriş noktası.

Gereksinimler:
    pip install pyqt5 pyqtgraph pyserial
"""
import sys
from PyQt5.QtWidgets import QApplication
from main_window import MainWindow


def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    win = MainWindow()
    win.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
