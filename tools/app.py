import serial
import struct
import tkinter as tk
from tkinter import ttk
from threading import Thread

class IMUViewer:
    def __init__(self, master, port='COM6', baudrate=115200):
        self.master = master
        self.master.title("IMU Veri İzleyici")
        self.master.geometry("400x500")
        
        # UI Bileşenleri Sözlüğü
        self.labels = {}
        self.fields = [
            ('Timestamp', 'ms'), ('Acc X', 'g'), ('Acc Y', 'g'), ('Acc Z', 'g'),
            ('Gyro X', '°/s'), ('Gyro Y', '°/s'), ('Gyro Z', '°/s'),
            ('Roll', '°'), ('Pitch', '°'), ('Yaw', '°')
        ]
        
        self._setup_ui()
        
        # Seri Port Ayarları
        try:
            self.ser = serial.Serial(port, baudrate, timeout=1)
            self.running = True
            self.thread = Thread(target=self.read_serial, daemon=True)
            self.thread.start()
        except Exception as e:
            tk.messagebox.showerror("Bağlantı Hatası", f"{port} açılamadı:\n{e}")

    def _setup_ui(self):
        style = ttk.Style()
        style.configure("TLabel", font=('Segoe UI', 10))
        style.configure("Value.TLabel", font=('Consolas', 12, 'bold'), foreground="blue")

        main_frame = ttk.Frame(self.master, padding="20")
        main_frame.pack(fill=tk.BOTH, expand=True)

        for i, (name, unit) in enumerate(self.fields):
            ttk.Label(main_frame, text=f"{name}:").grid(row=i, column=0, sticky='w', pady=5)
            var = tk.StringVar(value="0.00")
            lbl = ttk.Label(main_frame, textvariable=var, style="Value.TLabel")
            lbl.grid(row=i, column=1, sticky='e', padx=10)
            ttk.Label(main_frame, text=unit).grid(row=i, column=2, sticky='w')
            self.labels[name] = var

    def read_serial(self):
        # Frame Formatı: 2s (Header) + I (u32) + 9f (3x Acc, 3x Gyro, 3x RPY) + H (u16 CRC)
        # Toplam: 2 + 4 + (9 * 4) + 2 = 44 byte
        struct_format = '<2sI9fH'
        frame_size = struct.calcsize(struct_format)

        while self.running:
            try:
                # Header senkronizasyonu (AA 55)
                if self.ser.read(1) == b'\xAA':
                    if self.ser.read(1) == b'\x55':
                        # Geri kalan 42 byte'ı oku
                        payload = self.ser.read(frame_size - 2)
                        if len(payload) == (frame_size - 2):
                            full_data = b'\xAA\x55' + payload
                            decoded = struct.unpack(struct_format, full_data)
                            
                            # Arayüzü güncelle (Ana thread'de)
                            self.master.after(0, self.update_ui, decoded)
            except Exception as e:
                print(f"Okuma hatası: {e}")
                break

    def update_ui(self, data):
        # data[0]: Header, data[1]: Timestamp, data[2-10]: Sensör verileri, data[11]: CRC
        self.labels['Timestamp'].set(data[1])
        keys = ['Acc X', 'Acc Y', 'Acc Z', 'Gyro X', 'Gyro Y', 'Gyro Z', 'Roll', 'Pitch', 'Yaw']
        for i, key in enumerate(keys):
            self.labels[key].set(f"{data[i+2]:.4f}")

    def on_close(self):
        self.running = False
        if hasattr(self, 'ser'):
            self.ser.close()
        self.master.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = IMUViewer(root, port='COM6') # Portu cihazınıza göre değiştirin
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()