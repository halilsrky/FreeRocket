"""
Main Control Screen
Connection settings and mode selection
"""
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import serial.tools.list_ports
from typing import Optional, Callable

try:
    import customtkinter as ctk
    CTK_AVAILABLE = True
except ImportError:
    CTK_AVAILABLE = False

from models import ConnectionConfig


class MainControlScreen:
    """Main control panel with connection settings"""
    
    def __init__(self, root, on_sit_start: Callable, on_sut_start: Callable):
        self.root = root
        self.on_sit_start = on_sit_start
        self.on_sut_start = on_sut_start
        
        # Variables
        self.port_var = tk.StringVar(value="COM20")
        self.baud_var = tk.StringVar(value="115200")
        self.log_enabled_var = tk.BooleanVar(value=False)
        self.log_dir_var = tk.StringVar(value="logs")
        self.csv_enabled_var = tk.BooleanVar(value=True)
        self.csv_file_var = tk.StringVar(value="C:/Users/Halil/Desktop/IREC2026/SIT_SUT/Datas/veriler_yeni.csv")
        
        # State
        self.connected = False
        self.serial_handler = None
        
        # Callbacks
        self.on_connect: Optional[Callable[[ConnectionConfig], bool]] = None
        self.on_disconnect: Optional[Callable[[], None]] = None
        
        self._build_ui()
    
    def _build_ui(self):
        """Build the main control UI"""
        self.root.title("UKB Test Tool - Control Panel")
        self.root.geometry("600x500")
        self.root.resizable(False, False)
        
        # Main container
        main_frame = ttk.Frame(self.root, padding=20)
        main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Title
        title_label = ttk.Label(main_frame, text="UKB Ground Station", 
                               font=("Segoe UI", 16, "bold"))
        title_label.pack(pady=(0, 20))
        
        # Connection Frame
        conn_frame = ttk.LabelFrame(main_frame, text="Connection Settings", padding=15)
        conn_frame.pack(fill=tk.X, pady=(0, 15))
        
        # Port row
        port_row = ttk.Frame(conn_frame)
        port_row.pack(fill=tk.X, pady=5)
        
        ttk.Label(port_row, text="Port:", width=12).pack(side=tk.LEFT)
        self.port_combo = ttk.Combobox(port_row, textvariable=self.port_var, width=15)
        self.port_combo.pack(side=tk.LEFT, padx=(0, 10))
        
        ttk.Button(port_row, text="Refresh", command=self._refresh_ports, width=10).pack(side=tk.LEFT)
        
        # Baud row
        baud_row = ttk.Frame(conn_frame)
        baud_row.pack(fill=tk.X, pady=5)
        
        ttk.Label(baud_row, text="Baud Rate:", width=12).pack(side=tk.LEFT)
        baud_combo = ttk.Combobox(baud_row, textvariable=self.baud_var, width=15,
                                  values=["9600", "19200", "38400", "57600", "115200"],
                                  state="readonly")
        baud_combo.pack(side=tk.LEFT)
        
        # Connect button
        self.connect_btn = ttk.Button(conn_frame, text="Connect", 
                                      command=self._toggle_connection, width=20)
        self.connect_btn.pack(pady=(15, 0))
        
        # Status
        self.status_label = ttk.Label(conn_frame, text="Disconnected", foreground="red")
        self.status_label.pack(pady=(5, 0))
        
        # Options Frame
        opts_frame = ttk.LabelFrame(main_frame, text="Options", padding=15)
        opts_frame.pack(fill=tk.X, pady=(0, 15))
        
        # Log directory
        log_row = ttk.Frame(opts_frame)
        log_row.pack(fill=tk.X, pady=5)
        
        ttk.Checkbutton(log_row, text="Enable Logging", 
                       variable=self.log_enabled_var).pack(side=tk.LEFT)
        ttk.Entry(log_row, textvariable=self.log_dir_var, width=25).pack(side=tk.LEFT, padx=10)
        ttk.Button(log_row, text="Browse", command=self._browse_log_dir, width=8).pack(side=tk.LEFT)
        
        # CSV file
        csv_row = ttk.Frame(opts_frame)
        csv_row.pack(fill=tk.X, pady=5)
        
        ttk.Checkbutton(csv_row, text="Use CSV Data", 
                       variable=self.csv_enabled_var).pack(side=tk.LEFT)
        ttk.Entry(csv_row, textvariable=self.csv_file_var, width=25).pack(side=tk.LEFT, padx=10)
        ttk.Button(csv_row, text="Browse", command=self._browse_csv_file, width=8).pack(side=tk.LEFT)
        
        # Mode Selection Frame
        mode_frame = ttk.LabelFrame(main_frame, text="Test Mode", padding=15)
        mode_frame.pack(fill=tk.X)
        
        btn_frame = ttk.Frame(mode_frame)
        btn_frame.pack()
        
        self.sit_btn = ttk.Button(btn_frame, text="SIT Mode", 
                                  command=self._start_sit, width=15, state=tk.DISABLED)
        self.sit_btn.pack(side=tk.LEFT, padx=10)
        
        self.sut_btn = ttk.Button(btn_frame, text="SUT Mode", 
                                  command=self._start_sut, width=15, state=tk.DISABLED)
        self.sut_btn.pack(side=tk.LEFT, padx=10)
        
        # Initial port refresh
        self._refresh_ports()
    
    def _refresh_ports(self):
        """Refresh available COM ports"""
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])
    
    def _browse_log_dir(self):
        """Browse for log directory"""
        path = filedialog.askdirectory()
        if path:
            self.log_dir_var.set(path)
    
    def _browse_csv_file(self):
        """Browse for CSV file"""
        path = filedialog.askopenfilename(filetypes=[("CSV Files", "*.csv")])
        if path:
            self.csv_file_var.set(path)
    
    def _toggle_connection(self):
        """Toggle serial connection"""
        if self.connected:
            if self.on_disconnect:
                self.on_disconnect()
            self.connected = False
            self._update_connection_ui(False)
        else:
            config = ConnectionConfig(
                port=self.port_var.get(),
                baudrate=int(self.baud_var.get())
            )
            if self.on_connect and self.on_connect(config):
                self.connected = True
                self._update_connection_ui(True)
            else:
                messagebox.showerror("Connection Error", 
                                    f"Failed to connect to {config.port}")
    
    def _update_connection_ui(self, connected: bool):
        """Update UI based on connection state"""
        if connected:
            self.connect_btn.config(text="Disconnect")
            self.status_label.config(text="Connected", foreground="green")
            self.sit_btn.config(state=tk.NORMAL)
            self.sut_btn.config(state=tk.NORMAL)
            self.port_combo.config(state=tk.DISABLED)
        else:
            self.connect_btn.config(text="Connect")
            self.status_label.config(text="Disconnected", foreground="red")
            self.sit_btn.config(state=tk.DISABLED)
            self.sut_btn.config(state=tk.DISABLED)
            self.port_combo.config(state="readonly")
    
    def _start_sit(self):
        """Start SIT mode"""
        if self.on_sit_start:
            config = self._get_test_config()
            self.on_sit_start(config)
    
    def _start_sut(self):
        """Start SUT mode"""
        if self.on_sut_start:
            config = self._get_test_config()
            self.on_sut_start(config)
    
    def _get_test_config(self) -> dict:
        """Get current test configuration"""
        return {
            "log_enabled": self.log_enabled_var.get(),
            "log_dir": self.log_dir_var.get(),
            "csv_enabled": self.csv_enabled_var.get(),
            "csv_file": self.csv_file_var.get()
        }
    
    def set_connect_callback(self, callback: Callable[[ConnectionConfig], bool]):
        """Set connection callback"""
        self.on_connect = callback
    
    def set_disconnect_callback(self, callback: Callable[[], None]):
        """Set disconnect callback"""
        self.on_disconnect = callback
