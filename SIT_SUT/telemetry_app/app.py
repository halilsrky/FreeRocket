"""
UKB Ground Station - Main Application
Professional telemetry application with modular architecture
"""
import tkinter as tk
from tkinter import ttk, messagebox
import csv
import os
from typing import Optional, List
from datetime import datetime

from models import ConnectionConfig, TelemetryPacket, StatusPacket
from serial_handler import SerialHandler
from main_control import MainControlScreen
from sit_screen import SITScreen
from sut_screen import SUTScreen


class GroundStationApp:
    """Main application controller"""
    
    def __init__(self):
        self.root = tk.Tk()
        self.serial_handler = SerialHandler()
        
        # Screen references
        self.main_screen: Optional[MainControlScreen] = None
        self.sit_screen: Optional[SITScreen] = None
        self.sut_screen: Optional[SUTScreen] = None
        
        # Current test configuration
        self.test_config: dict = {}
        
        self._setup_serial_callbacks()
        self._create_main_screen()
        
        # Window close handler
        self.root.protocol("WM_DELETE_WINDOW", self._on_app_close)
    
    def _setup_serial_callbacks(self):
        """Setup serial handler callbacks"""
        self.serial_handler.set_telemetry_callback(self._on_telemetry_received)
        self.serial_handler.set_status_callback(self._on_status_received)
        self.serial_handler.set_error_callback(self._on_serial_error)
    
    def _create_main_screen(self):
        """Create the main control screen"""
        self.main_screen = MainControlScreen(
            self.root,
            on_sit_start=self._start_sit_mode,
            on_sut_start=self._start_sut_mode
        )
        
        self.main_screen.set_connect_callback(self._connect_serial)
        self.main_screen.set_disconnect_callback(self._disconnect_serial)
    
    def _connect_serial(self, config: ConnectionConfig) -> bool:
        """Handle serial connection"""
        return self.serial_handler.connect(config)
    
    def _disconnect_serial(self):
        """Handle serial disconnection"""
        self.serial_handler.disconnect()
    
    def _start_sit_mode(self, config: dict):
        """Start SIT mode and open SIT screen"""
        self.test_config = config
        
        # Create SIT screen
        self.sit_screen = SITScreen(
            self.root,
            on_stop=self._stop_sit_mode,
            on_close=self._on_sit_close
        )
        
        # Start serial handler in SIT mode
        self.serial_handler.start_sit_mode()
        
        self.sit_screen.add_log("SIT mode started")
    
    def _stop_sit_mode(self):
        """Stop SIT mode"""
        self.serial_handler.stop_mode()
        if self.sit_screen:
            self.sit_screen.add_log("SIT mode stopped")
    
    def _on_sit_close(self):
        """Handle SIT screen close"""
        self.serial_handler.stop_mode()
        self.sit_screen = None
    
    def _start_sut_mode(self, config: dict):
        """Start SUT mode and open SUT screen"""
        self.test_config = config
        
        # Load CSV data if enabled
        csv_data = None
        if config.get('csv_enabled') and config.get('csv_file'):
            csv_data = self._load_csv_data(config['csv_file'])
        
        # Create SUT screen
        self.sut_screen = SUTScreen(
            self.root,
            on_stop=self._stop_sut_mode,
            on_close=self._on_sut_close
        )
        
        # Start serial handler in SUT mode
        self.serial_handler.start_sut_mode(csv_data)
        
        self.sut_screen.add_log("SUT mode started")
        if csv_data:
            self.sut_screen.add_log(f"Loaded {len(csv_data)} rows from CSV")
    
    def _stop_sut_mode(self):
        """Stop SUT mode"""
        self.serial_handler.stop_mode()
        
        if self.sut_screen:
            self.sut_screen.add_log("SUT mode stopped")
            
            # Save flight data if logging enabled
            if self.test_config.get('log_enabled'):
                self._save_sut_data()
    
    def _on_sut_close(self):
        """Handle SUT screen close"""
        self.serial_handler.stop_mode()
        self.sut_screen = None
    
    def _on_telemetry_received(self, packet: TelemetryPacket):
        """Handle received telemetry packet"""
        # Route to appropriate screen - use default argument to capture packet
        if self.sit_screen:
            self.root.after(0, lambda p=packet: self._update_sit_telemetry(p))
        elif self.sut_screen:
            self.root.after(0, lambda p=packet: self._update_sut_telemetry(p))
    
    def _update_sit_telemetry(self, packet: TelemetryPacket):
        """Update SIT screen with telemetry (thread-safe)"""
        try:
            if self.sit_screen and self.sit_screen.winfo_exists():
                self.sit_screen.update_telemetry(packet)
        except tk.TclError:
            pass
    
    def _update_sut_telemetry(self, packet: TelemetryPacket):
        """Update SUT screen with telemetry (thread-safe)"""
        try:
            if self.sut_screen and self.sut_screen.winfo_exists():
                self.sut_screen.update_telemetry(packet)
        except tk.TclError:
            pass
    
    def _on_status_received(self, packet: StatusPacket):
        """Handle received status packet"""
        if self.sut_screen:
            self.root.after(0, lambda p=packet: self._update_sut_status(p))
    
    def _update_sut_status(self, packet: StatusPacket):
        """Update SUT screen with status (thread-safe)"""
        try:
            if self.sut_screen and self.sut_screen.winfo_exists():
                self.sut_screen.update_status(packet)
        except tk.TclError:
            pass
    
    def _on_serial_error(self, message: str):
        """Handle serial errors"""
        self.root.after(0, lambda: messagebox.showerror("Serial Error", message))
    
    def _load_csv_data(self, filepath: str) -> Optional[List[List[float]]]:
        """Load CSV data for SUT mode"""
        try:
            data = []
            with open(filepath, 'r') as f:
                reader = csv.reader(f)
                
                # Check for header
                first_row = next(reader, None)
                if first_row:
                    # Check if first row is numeric
                    try:
                        values = [float(v) for v in first_row[:8]]
                        data.append(values)
                    except ValueError:
                        pass  # Skip header row
                
                # Read remaining rows
                for row in reader:
                    if row:
                        try:
                            values = [float(v) for v in row[:8]]
                            while len(values) < 8:
                                values.append(0.0)
                            data.append(values)
                        except ValueError:
                            continue
            
            return data if data else None
        except Exception as e:
            messagebox.showerror("CSV Error", f"Failed to load CSV: {e}")
            return None
    
    def _save_sut_data(self):
        """Save SUT flight data to log file"""
        if not self.sut_screen:
            return
        
        try:
            log_dir = self.test_config.get('log_dir', 'logs')
            os.makedirs(log_dir, exist_ok=True)
            
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            filename = os.path.join(log_dir, f"SUT_flight_log_{timestamp}.csv")
            
            flight_data = self.sut_screen.get_flight_data()
            
            with open(filename, 'w', newline='') as f:
                writer = csv.writer(f)
                
                # Write header
                writer.writerow(['Time_s', 'Altitude_m'])
                
                # Write altitude data
                for t, alt in zip(flight_data['time_data'], flight_data['altitude_data']):
                    writer.writerow([f"{t:.3f}", f"{alt:.2f}"])
                
                # Write events
                writer.writerow([])
                writer.writerow(['Events'])
                writer.writerow(['Time_s', 'Event', 'Altitude_m'])
                
                for evt_time, bit_idx, evt_alt in flight_data['events']:
                    event_name = SUTScreen.EVENT_LABELS.get(bit_idx, f"Bit_{bit_idx}")
                    writer.writerow([f"{evt_time:.3f}", event_name, f"{evt_alt:.2f}"])
            
            self.sut_screen.add_log(f"Data saved to {filename}")
        except Exception as e:
            self.sut_screen.add_log(f"Error saving data: {e}")
    
    def _on_app_close(self):
        """Handle application close"""
        # Stop any running mode
        self.serial_handler.stop_mode()
        self.serial_handler.disconnect()
        
        # Close screens
        if self.sit_screen:
            self.sit_screen.destroy()
        if self.sut_screen:
            self.sut_screen.destroy()
        
        self.root.destroy()
    
    def run(self):
        """Run the application"""
        self.root.mainloop()


def main():
    """Application entry point"""
    app = GroundStationApp()
    app.run()


if __name__ == "__main__":
    main()
