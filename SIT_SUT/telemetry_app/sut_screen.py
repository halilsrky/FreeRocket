"""
SUT Screen - System Under Test
Telemetry transmission and status reception - no connection controls
"""
import tkinter as tk
from tkinter import ttk
from typing import Optional, Callable, List, Tuple
from collections import deque
import time

try:
    import matplotlib
    matplotlib.use('TkAgg')
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    from matplotlib.figure import Figure
    import numpy as np
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False

from models import TelemetryPacket, StatusPacket, StatusFlags
from widgets import TelemetryPanel


class SUTScreen(tk.Toplevel):
    """SUT Mode Screen - Sends telemetry, receives status"""
    
    # Event colors for plot markers
    EVENT_COLORS = {
        0: '#FF4444',  # Launch - Red
        1: '#FF8800',  # Burnout - Orange  
        2: '#FFFF00',  # Min Alt - Yellow
        3: '#FF00FF',  # Angle - Magenta
        4: '#00FF00',  # Descent - Green
        5: '#00FFFF',  # Drogue - Cyan
        6: '#0088FF',  # Main Alt - Blue
        7: '#8800FF'   # Main - Purple
    }
    
    EVENT_LABELS = {
        0: 'Launch',
        1: 'Burnout', 
        2: 'Min Alt',
        3: 'Angle',
        4: 'Descent',
        5: 'Drogue',
        6: 'Main Alt',
        7: 'Main Chute'
    }
    
    def __init__(self, parent, on_stop: Callable, on_close: Callable):
        super().__init__(parent)
        self.on_stop = on_stop
        self.on_close = on_close
        
        # Data storage
        self.time_data = deque(maxlen=1000)
        self.altitude_data = deque(maxlen=1000)
        self.events: List[Tuple[float, int, float]] = []  # (time, bit_index, altitude)
        self.start_time = time.time()
        
        # Status tracking
        self.previous_status = 0
        self.current_status = StatusFlags()
        
        # Statistics
        self.tx_count = 0
        self.rx_count = 0
        self.max_altitude = 0.0
        
        # Test state
        self._is_running = True
        self._final_duration = 0.0
        
        self._build_ui()
        self._start_plot_update()
        
        # Window close handler
        self.protocol("WM_DELETE_WINDOW", self._handle_close)
    
    def _build_ui(self):
        """Build the SUT screen UI"""
        self.title("SUT Mode - System Under Test")
        self.state('zoomed')  # Windows maximize
        self.minsize(1200, 700)
        
        # Configure grid
        self.columnconfigure(0, weight=1)
        self.columnconfigure(1, weight=3)
        self.rowconfigure(0, weight=1)
        self.rowconfigure(1, weight=0)
        
        # Left panel - Telemetry and Status
        left_frame = ttk.Frame(self, padding=10)
        left_frame.grid(row=0, column=0, sticky=tk.NSEW)
        left_frame.columnconfigure(0, weight=1)
        left_frame.rowconfigure(1, weight=1)
        
        # Mode indicator
        mode_frame = ttk.Frame(left_frame)
        mode_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(mode_frame, text="SUT MODE", font=("Segoe UI", 14, "bold"),
                 foreground="orange").pack(side=tk.LEFT)
        
        self.status_label = ttk.Label(mode_frame, text="RUNNING", 
                                      font=("Segoe UI", 12, "bold"), foreground="green")
        self.status_label.pack(side=tk.RIGHT)
        
        # Telemetry panel
        self.telemetry_panel = TelemetryPanel(left_frame)
        self.telemetry_panel.pack(fill=tk.BOTH, expand=True)
        
        # Statistics frame
        stats_frame = ttk.LabelFrame(left_frame, text="Statistics", padding=10)
        stats_frame.pack(fill=tk.X, pady=(10, 0))
        
        stats_grid = ttk.Frame(stats_frame)
        stats_grid.pack(fill=tk.X)
        
        ttk.Label(stats_grid, text="TX:").grid(row=0, column=0, sticky=tk.W, padx=5)
        self.tx_label = ttk.Label(stats_grid, text="0", font=("Consolas", 10, "bold"))
        self.tx_label.grid(row=0, column=1, sticky=tk.E, padx=5)
        
        ttk.Label(stats_grid, text="RX:").grid(row=0, column=2, sticky=tk.W, padx=5)
        self.rx_label = ttk.Label(stats_grid, text="0", font=("Consolas", 10, "bold"))
        self.rx_label.grid(row=0, column=3, sticky=tk.E, padx=5)
        
        ttk.Label(stats_grid, text="Max Alt:").grid(row=1, column=0, sticky=tk.W, padx=5)
        self.max_alt_label = ttk.Label(stats_grid, text="0.00 m", font=("Consolas", 10, "bold"))
        self.max_alt_label.grid(row=1, column=1, sticky=tk.E, padx=5)
        
        ttk.Label(stats_grid, text="Duration:").grid(row=1, column=2, sticky=tk.W, padx=5)
        self.duration_label = ttk.Label(stats_grid, text="0.0 s", font=("Consolas", 10, "bold"))
        self.duration_label.grid(row=1, column=3, sticky=tk.E, padx=5)
        
        # Right panel - Graph
        right_frame = ttk.Frame(self, padding=10)
        right_frame.grid(row=0, column=1, sticky=tk.NSEW)
        
        if MATPLOTLIB_AVAILABLE:
            self._create_plot(right_frame)
        else:
            ttk.Label(right_frame, text="Matplotlib not available", 
                     font=("Segoe UI", 12)).pack(expand=True)
        
        # Legend frame
        legend_frame = ttk.LabelFrame(right_frame, text="Event Legend", padding=5)
        legend_frame.pack(fill=tk.X, pady=(10, 0))
        
        legend_grid = ttk.Frame(legend_frame)
        legend_grid.pack()
        
        for i, (bit_idx, label) in enumerate(self.EVENT_LABELS.items()):
            col = i % 4
            row = i // 4
            color = self.EVENT_COLORS[bit_idx]
            
            frame = ttk.Frame(legend_grid)
            frame.grid(row=row, column=col, padx=10, pady=2)
            
            # Color indicator using a label with background
            canvas = tk.Canvas(frame, width=12, height=12, highlightthickness=0)
            canvas.pack(side=tk.LEFT, padx=(0, 5))
            canvas.create_oval(2, 2, 10, 10, fill=color, outline=color)
            
            ttk.Label(frame, text=label, font=("Segoe UI", 8)).pack(side=tk.LEFT)
        
        # Bottom panel - Controls and Log
        bottom_frame = ttk.Frame(self, padding=10)
        bottom_frame.grid(row=1, column=0, columnspan=2, sticky=tk.EW)
        
        self.stop_btn = ttk.Button(bottom_frame, text="STOP", command=self._stop_test, width=20)
        self.stop_btn.pack(side=tk.RIGHT)
        
        # Log area
        log_frame = ttk.LabelFrame(bottom_frame, text="Event Log", padding=5)
        log_frame.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 20))
        
        self.log_text = tk.Text(log_frame, height=3, font=("Consolas", 9), state=tk.DISABLED)
        self.log_text.pack(fill=tk.X)
    
    def _create_plot(self, parent):
        """Create matplotlib plot with event markers"""
        self.fig = Figure(figsize=(10, 5), dpi=100)
        self.ax = self.fig.add_subplot(111)
        self.ax.set_xlabel("Time (s)")
        self.ax.set_ylabel("Altitude (m)")
        self.ax.set_title("Flight Profile")
        self.ax.grid(True, alpha=0.3)
        
        # Altitude line
        self.line, = self.ax.plot([], [], 'b-', linewidth=1.5, label='Altitude')
        
        # Event scatter plots
        self.event_scatters = {}
        for bit_idx in range(8):
            scatter = self.ax.scatter([], [], c=self.EVENT_COLORS[bit_idx], s=60,
                                      marker='o', label=self.EVENT_LABELS[bit_idx],
                                      zorder=5, edgecolors='black', linewidth=0.5)
            self.event_scatters[bit_idx] = scatter
        
        self.canvas = FigureCanvasTkAgg(self.fig, master=parent)
        self.canvas.draw()
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
        # Fullscreen button frame
        btn_frame = ttk.Frame(parent)
        btn_frame.pack(fill=tk.X, pady=(5, 0))
        
        self.fullscreen_btn = ttk.Button(btn_frame, text="Full Screen Graph", 
                                          command=self._open_fullscreen_graph)
        self.fullscreen_btn.pack(side=tk.RIGHT)
    
    def _start_plot_update(self):
        """Start periodic plot update"""
        self._update_plot()
    
    def _update_plot(self):
        """Update plot with new data"""
        if MATPLOTLIB_AVAILABLE and hasattr(self, 'line'):
            if self.time_data and self.altitude_data:
                self.line.set_data(list(self.time_data), list(self.altitude_data))
                
                # Update event markers
                events_by_bit = {i: {'times': [], 'alts': []} for i in range(8)}
                for evt_time, bit_idx, evt_alt in self.events:
                    events_by_bit[bit_idx]['times'].append(evt_time)
                    events_by_bit[bit_idx]['alts'].append(evt_alt)
                
                for bit_idx, scatter in self.event_scatters.items():
                    if events_by_bit[bit_idx]['times']:
                        offsets = np.column_stack((events_by_bit[bit_idx]['times'],
                                                   events_by_bit[bit_idx]['alts']))
                        scatter.set_offsets(offsets)
                    else:
                        scatter.set_offsets(np.empty((0, 2)))
                
                self.ax.relim()
                self.ax.autoscale_view()
                self.canvas.draw_idle()
        
        # Update duration only if running
        if self._is_running:
            elapsed = time.time() - self.start_time
            self.duration_label.config(text=f"{elapsed:.1f} s")
        
        # Schedule next update
        if self.winfo_exists():
            self.after(100, self._update_plot)
    
    def update_telemetry(self, packet: TelemetryPacket):
        """Update display with transmitted telemetry"""
        try:
            # Update panel
            self.telemetry_panel.update_telemetry(packet)
            
            # Update plot data
            current_time = time.time() - self.start_time
            altitude = packet.altitude.altitude.value
            
            self.time_data.append(current_time)
            self.altitude_data.append(altitude)
            
            # Update statistics
            self.tx_count += 1
            self.tx_label.config(text=str(self.tx_count))
            
            if altitude > self.max_altitude:
                self.max_altitude = altitude
                self.max_alt_label.config(text=f"{self.max_altitude:.2f} m")
        except Exception:
            pass
    
    def update_status(self, packet: StatusPacket):
        """Update display with received status"""
        try:
            # Update status indicators
            self.telemetry_panel.update_status(packet.flags)
            self.current_status = packet.flags
            
            # Detect new events
            new_flags = packet.status_word & ~self.previous_status
            
            if new_flags:
                current_time = time.time() - self.start_time
                current_alt = self.altitude_data[-1] if self.altitude_data else 0.0
                
                for bit_idx in range(8):
                    if new_flags & (1 << bit_idx):
                        self.events.append((current_time, bit_idx, current_alt))
                        self.add_log(f"[{current_time:.1f}s] {self.EVENT_LABELS[bit_idx]} at {current_alt:.1f}m")
            
            self.previous_status = packet.status_word
            
            # Update RX count
            self.rx_count += 1
            self.rx_label.config(text=str(self.rx_count))
        except Exception:
            pass
    
    def add_log(self, message: str):
        """Add message to log"""
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, message + "\n")
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)
    
    def _stop_test(self):
        """Stop the test"""
        self._is_running = False
        self._final_duration = time.time() - self.start_time
        self.status_label.config(text="STOPPED", foreground="red")
        self.stop_btn.config(state=tk.DISABLED)
        if self.on_stop:
            self.on_stop()
    
    def _open_fullscreen_graph(self):
        """Open graph in fullscreen window for detailed inspection"""
        if not MATPLOTLIB_AVAILABLE:
            return
        
        # Create fullscreen window
        fs_window = tk.Toplevel(self)
        fs_window.title("Flight Profile - Full Screen")
        fs_window.state('zoomed')  # Windows maximize
        
        # Create new figure with larger size
        fig = Figure(figsize=(16, 9), dpi=100)
        ax = fig.add_subplot(111)
        ax.set_xlabel("Time (s)", fontsize=12)
        ax.set_ylabel("Altitude (m)", fontsize=12)
        ax.set_title("Flight Profile - Detailed View", fontsize=14)
        ax.grid(True, alpha=0.3)
        
        # Plot altitude line
        if self.time_data and self.altitude_data:
            ax.plot(list(self.time_data), list(self.altitude_data), 'b-', 
                   linewidth=2, label='Altitude')
        
        # Plot events
        for evt_time, bit_idx, evt_alt in self.events:
            color = self.EVENT_COLORS[bit_idx]
            label = self.EVENT_LABELS[bit_idx]
            ax.scatter([evt_time], [evt_alt], c=color, s=100, marker='o',
                      label=label, zorder=5, edgecolors='black', linewidth=1)
            ax.annotate(label, (evt_time, evt_alt), textcoords="offset points",
                       xytext=(5, 5), fontsize=9)
        
        # Add legend (remove duplicates)
        handles, labels = ax.get_legend_handles_labels()
        by_label = dict(zip(labels, handles))
        ax.legend(by_label.values(), by_label.keys(), loc='best')
        
        # Add max altitude annotation
        if self.max_altitude > 0:
            ax.axhline(y=self.max_altitude, color='red', linestyle='--', alpha=0.5)
            ax.text(0.02, self.max_altitude, f'Max: {self.max_altitude:.2f}m', 
                   transform=ax.get_yaxis_transform(), va='bottom', fontsize=10, color='red')
        
        fig.tight_layout()
        
        # Create canvas with navigation toolbar
        from matplotlib.backends.backend_tkagg import NavigationToolbar2Tk
        
        canvas = FigureCanvasTkAgg(fig, master=fs_window)
        canvas.draw()
        
        # Toolbar for zoom/pan
        toolbar_frame = ttk.Frame(fs_window)
        toolbar_frame.pack(side=tk.TOP, fill=tk.X)
        toolbar = NavigationToolbar2Tk(canvas, toolbar_frame)
        toolbar.update()
        
        canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
        # Close button
        close_btn = ttk.Button(fs_window, text="Close", command=fs_window.destroy)
        close_btn.pack(side=tk.BOTTOM, pady=10)
        
        # ESC to close
        fs_window.bind('<Escape>', lambda e: fs_window.destroy())
    
    def _handle_close(self):
        """Handle window close"""
        self._stop_test()
        if self.on_close:
            self.on_close()
        self.destroy()
    
    def get_flight_data(self) -> dict:
        """Get flight data for saving"""
        return {
            'time_data': list(self.time_data),
            'altitude_data': list(self.altitude_data),
            'events': self.events,
            'max_altitude': self.max_altitude,
            'tx_count': self.tx_count,
            'rx_count': self.rx_count,
            'duration': time.time() - self.start_time
        }
