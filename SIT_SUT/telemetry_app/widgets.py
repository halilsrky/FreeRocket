"""
Telemetry Display Widget
Reusable component for displaying telemetry values
"""
import tkinter as tk
from tkinter import ttk
from typing import Dict, Optional
from models import TelemetryValue, TelemetryPacket, StatusFlags


class TelemetryGroupWidget(ttk.LabelFrame):
    """Widget for displaying a group of telemetry values"""
    
    def __init__(self, parent, title: str, **kwargs):
        super().__init__(parent, text=title, **kwargs)
        self.labels: Dict[str, ttk.Label] = {}
        self.values: Dict[str, ttk.Label] = {}
        self.units: Dict[str, ttk.Label] = {}
        self._row = 0
    
    def add_field(self, key: str, label: str, unit: str = "", initial_value: str = "---"):
        """Add a telemetry field to the group"""
        # Label
        lbl = ttk.Label(self, text=label, font=("Segoe UI", 10))
        lbl.grid(row=self._row, column=0, sticky=tk.W, padx=(10, 5), pady=3)
        self.labels[key] = lbl
        
        # Value
        val = ttk.Label(self, text=initial_value, font=("Consolas", 12, "bold"), width=12, anchor=tk.E)
        val.grid(row=self._row, column=1, sticky=tk.E, padx=5, pady=3)
        self.values[key] = val
        
        # Unit
        unit_lbl = ttk.Label(self, text=unit, font=("Segoe UI", 9), width=8)
        unit_lbl.grid(row=self._row, column=2, sticky=tk.W, padx=(0, 10), pady=3)
        self.units[key] = unit_lbl
        
        self._row += 1
    
    def update_value(self, key: str, value: str):
        """Update a single value"""
        if key in self.values:
            self.values[key].config(text=value)
    
    def update_from_telemetry(self, tv: TelemetryValue, key: str):
        """Update from TelemetryValue object"""
        if key in self.values:
            self.values[key].config(text=tv.formatted())
    
    def clear_all(self):
        """Reset all values to default"""
        for key in self.values:
            self.values[key].config(text="---")


class StatusIndicatorWidget(ttk.LabelFrame):
    """Widget for displaying status flags as indicators"""
    
    def __init__(self, parent, title: str = "Status", **kwargs):
        super().__init__(parent, text=title, **kwargs)
        self.indicators: Dict[str, ttk.Label] = {}
        self._row = 0
        self._col = 0
        self._max_cols = 2
    
    def add_indicator(self, key: str, label: str):
        """Add a status indicator"""
        frame = ttk.Frame(self)
        frame.grid(row=self._row, column=self._col, sticky=tk.W, padx=10, pady=3)
        
        # Indicator light (using text)
        indicator = ttk.Label(frame, text="OFF", font=("Consolas", 9), 
                             foreground="gray", width=4)
        indicator.pack(side=tk.LEFT, padx=(0, 5))
        
        # Label
        ttk.Label(frame, text=label, font=("Segoe UI", 9)).pack(side=tk.LEFT)
        
        self.indicators[key] = indicator
        
        self._col += 1
        if self._col >= self._max_cols:
            self._col = 0
            self._row += 1
    
    def set_indicator(self, key: str, active: bool):
        """Set indicator state"""
        if key in self.indicators:
            if active:
                self.indicators[key].config(text="ON", foreground="green")
            else:
                self.indicators[key].config(text="OFF", foreground="gray")
    
    def update_from_flags(self, flags: StatusFlags):
        """Update all indicators from StatusFlags"""
        flag_map = {
            "launch": flags.launch_detected,
            "burnout": flags.burnout_timeout,
            "min_alt": flags.min_altitude_exceeded,
            "angle": flags.angle_exceeded,
            "descent": flags.descent_detected,
            "drogue": flags.drogue_deployed,
            "main_alt": flags.main_altitude_reached,
            "main": flags.main_deployed
        }
        for key, active in flag_map.items():
            self.set_indicator(key, active)
    
    def clear_all(self):
        """Reset all indicators"""
        for key in self.indicators:
            self.set_indicator(key, False)


class TelemetryPanel(ttk.Frame):
    """Complete telemetry display panel"""
    
    def __init__(self, parent, **kwargs):
        super().__init__(parent, **kwargs)
        self._build_ui()
    
    def _build_ui(self):
        """Build the telemetry panel UI"""
        # Configure grid
        self.columnconfigure(0, weight=1)
        self.columnconfigure(1, weight=1)
        
        # Altitude group
        self.altitude_group = TelemetryGroupWidget(self, "Altitude")
        self.altitude_group.grid(row=0, column=0, sticky=tk.NSEW, padx=5, pady=5)
        self.altitude_group.add_field("altitude", "Altitude", "m")
        
        # Pressure group
        self.pressure_group = TelemetryGroupWidget(self, "Pressure")
        self.pressure_group.grid(row=0, column=1, sticky=tk.NSEW, padx=5, pady=5)
        self.pressure_group.add_field("pressure", "Pressure", "hPa")
        
        # Acceleration group
        self.accel_group = TelemetryGroupWidget(self, "Acceleration")
        self.accel_group.grid(row=1, column=0, sticky=tk.NSEW, padx=5, pady=5)
        self.accel_group.add_field("acc_x", "X-Axis", "g")
        self.accel_group.add_field("acc_y", "Y-Axis", "g")
        self.accel_group.add_field("acc_z", "Z-Axis", "g")
        
        # Gyroscope group
        self.gyro_group = TelemetryGroupWidget(self, "Gyroscope")
        self.gyro_group.grid(row=1, column=1, sticky=tk.NSEW, padx=5, pady=5)
        self.gyro_group.add_field("gyro_x", "X-Axis", "deg/s")
        self.gyro_group.add_field("gyro_y", "Y-Axis", "deg/s")
        self.gyro_group.add_field("gyro_z", "Z-Axis", "deg/s")
        
        # Status indicators
        self.status_group = StatusIndicatorWidget(self, "Flight Status")
        self.status_group.grid(row=2, column=0, columnspan=2, sticky=tk.NSEW, padx=5, pady=5)
        self.status_group.add_indicator("launch", "Launch")
        self.status_group.add_indicator("burnout", "Burnout")
        self.status_group.add_indicator("min_alt", "Min Alt")
        self.status_group.add_indicator("angle", "Angle")
        self.status_group.add_indicator("descent", "Descent")
        self.status_group.add_indicator("drogue", "Drogue")
        self.status_group.add_indicator("main_alt", "Main Alt")
        self.status_group.add_indicator("main", "Main Chute")
    
    def update_telemetry(self, packet: TelemetryPacket):
        """Update display from telemetry packet"""
        # Altitude
        self.altitude_group.update_from_telemetry(packet.altitude.altitude, "altitude")
        
        # Pressure
        self.pressure_group.update_from_telemetry(packet.pressure.pressure, "pressure")
        
        # Acceleration
        self.accel_group.update_from_telemetry(packet.acceleration.acc_x, "acc_x")
        self.accel_group.update_from_telemetry(packet.acceleration.acc_y, "acc_y")
        self.accel_group.update_from_telemetry(packet.acceleration.acc_z, "acc_z")
        
        # Gyroscope
        self.gyro_group.update_from_telemetry(packet.gyroscope.gyro_x, "gyro_x")
        self.gyro_group.update_from_telemetry(packet.gyroscope.gyro_y, "gyro_y")
        self.gyro_group.update_from_telemetry(packet.gyroscope.gyro_z, "gyro_z")
    
    def update_status(self, flags: StatusFlags):
        """Update status indicators"""
        self.status_group.update_from_flags(flags)
    
    def clear_all(self):
        """Clear all displays"""
        self.altitude_group.clear_all()
        self.pressure_group.clear_all()
        self.accel_group.clear_all()
        self.gyro_group.clear_all()
        self.status_group.clear_all()
