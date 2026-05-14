"""
SIT Screen - System Integration Test
Telemetry reception only - no connection controls
"""
import tkinter as tk
from tkinter import ttk
from typing import Optional, Callable
from collections import deque
import time
import math

try:
    import matplotlib
    matplotlib.use('TkAgg')
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    from matplotlib.figure import Figure
    import numpy as np
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False

from models import TelemetryPacket
from widgets import TelemetryGroupWidget


class RocketVisualization(tk.Canvas):
    """Simple 3D rocket visualization using 2D projection"""
    
    def __init__(self, parent, width=350, height=350, **kwargs):
        super().__init__(parent, width=width, height=height, bg='#e8e8e8', 
                        highlightthickness=1, highlightbackground='#999', **kwargs)
        self.width = width
        self.height = height
        
        # Current angles (in degrees)
        self.roll = 0.0    # X axis - gyro_x
        self.pitch = 0.0   # Y axis - gyro_y  
        self.yaw = 0.0     # Z axis - gyro_z
        
        # Target angles for smooth interpolation
        self.target_roll = 0.0
        self.target_pitch = 0.0
        self.target_yaw = 0.0
        
        # Smoothing factor (0-1, higher = faster)
        self.smoothing = 0.2
        
        # Rocket dimensions (scaled up)
        self.body_length = 140
        self.body_radius = 20
        self.cone_height = 50
        self.fin_size = 35
        self.num_segments = 12  # For cylinder approximation
        
        # Initial draw
        self._draw_rocket()
        self._animate()
    
    def _apply_rotation(self, x, y, z, roll, pitch, yaw):
        """Apply rotation using the same matrix as Processing code"""
        # Convert to radians
        roll_rad = math.radians(roll)
        pitch_rad = math.radians(pitch)
        yaw_rad = math.radians(-yaw)  # Negative yaw like in Processing
        
        c2 = math.cos(roll_rad)
        s2 = math.sin(roll_rad)
        c1 = math.cos(pitch_rad)
        s1 = math.sin(pitch_rad)
        c3 = math.cos(yaw_rad)
        s3 = math.sin(yaw_rad)
        
        # Rotation matrix from Processing applyMatrix
        x_new = x * (c2*c3) + y * (s1*s3 + c1*c3*s2) + z * (c3*s1*s2 - c1*s3)
        y_new = x * (-s2) + y * (c1*c2) + z * (c2*s1)
        z_new = x * (c2*s3) + y * (c1*s2*s3 - c3*s1) + z * (c1*c3 + s1*s2*s3)
        
        return x_new, y_new, z_new
    
    def _project(self, x, y, z):
        """Simple perspective projection"""
        cx, cy = self.width / 2, self.height / 2
        
        # Perspective projection
        fov = 400
        scale = fov / (fov + z)
        screen_x = cx + x * scale
        screen_y = cy - y * scale  # Invert Y for screen coordinates
        
        return screen_x, screen_y, z
    
    def _generate_cylinder_vertices(self):
        """Generate vertices for a cylinder (rocket body)"""
        vertices = []
        r = self.body_radius
        half_len = self.body_length / 2
        
        # Bottom and top circles
        for i in range(self.num_segments):
            angle = 2 * math.pi * i / self.num_segments
            x = r * math.cos(angle)
            z = r * math.sin(angle)
            # Bottom vertex
            vertices.append((x, -half_len, z))
            # Top vertex
            vertices.append((x, half_len, z))
        
        return vertices
    
    def _generate_fin_vertices(self):
        """Generate vertices for 4 fins"""
        fins = []
        r = self.body_radius
        half_len = self.body_length / 2
        fin_h = self.fin_size
        fin_w = self.fin_size * 0.8
        
        # 4 fins at 90 degree intervals
        for i in range(4):
            angle = math.pi / 4 + i * math.pi / 2  # 45, 135, 225, 315 degrees
            dx = math.cos(angle)
            dz = math.sin(angle)
            
            # Fin vertices (triangle)
            base_inner = (r * dx, -half_len, r * dz)
            base_outer = ((r + fin_h) * dx, -half_len - fin_w * 0.3, (r + fin_h) * dz)
            tip = (r * dx, -half_len + fin_w, r * dz)
            
            fins.append([base_inner, base_outer, tip])
        
        return fins
    
    def _draw_rocket(self):
        """Draw the rocket with current orientation"""
        self.delete("all")
        
        # Draw background grid
        self._draw_grid()
        
        # Generate cylinder vertices
        cyl_verts = self._generate_cylinder_vertices()
        
        # Cone tip
        cone_tip = (0, self.body_length / 2 + self.cone_height, 0)
        
        # Fins
        fins = self._generate_fin_vertices()
        
        # Rotate all vertices
        rotated_cyl = []
        for v in cyl_verts:
            rv = self._apply_rotation(v[0], v[1], v[2], self.roll, self.pitch, self.yaw)
            rotated_cyl.append(rv)
        
        rotated_tip = self._apply_rotation(cone_tip[0], cone_tip[1], cone_tip[2], 
                                           self.roll, self.pitch, self.yaw)
        
        rotated_fins = []
        for fin in fins:
            rotated_fin = []
            for v in fin:
                rv = self._apply_rotation(v[0], v[1], v[2], self.roll, self.pitch, self.yaw)
                rotated_fin.append(rv)
            rotated_fins.append(rotated_fin)
        
        # Project to 2D
        projected_cyl = [self._project(*v) for v in rotated_cyl]
        projected_tip = self._project(*rotated_tip)
        projected_fins = [[self._project(*v) for v in fin] for fin in rotated_fins]
        
        # Collect all faces with depth for sorting
        all_faces = []
        
        # Cylinder side faces
        n = self.num_segments
        for i in range(n):
            next_i = (i + 1) % n
            # Each face is a quad: bottom[i], bottom[next_i], top[next_i], top[i]
            indices = [i*2, next_i*2, next_i*2+1, i*2+1]
            avg_z = sum(projected_cyl[idx][2] for idx in indices) / 4
            
            # Color based on face angle for shading
            face_angle = 2 * math.pi * i / n
            brightness = int(80 + 60 * math.cos(face_angle - math.pi/4))
            color = f'#{brightness:02x}{brightness+30:02x}{brightness+80:02x}'
            
            all_faces.append(('quad', indices, projected_cyl, avg_z, color))
        
        # Cone faces
        for i in range(n):
            next_i = (i + 1) % n
            # Triangle from top circle to tip
            v1_idx = i*2 + 1  # top vertex
            v2_idx = next_i*2 + 1  # next top vertex
            
            avg_z = (projected_cyl[v1_idx][2] + projected_cyl[v2_idx][2] + projected_tip[2]) / 3
            
            # Red/orange cone
            face_angle = 2 * math.pi * i / n
            r_val = int(200 + 55 * math.cos(face_angle - math.pi/4))
            g_val = int(60 + 40 * math.cos(face_angle - math.pi/4))
            color = f'#{r_val:02x}{g_val:02x}30'
            
            all_faces.append(('cone', (v1_idx, v2_idx), projected_cyl, avg_z, color, projected_tip))
        
        # Fin faces
        for i, proj_fin in enumerate(projected_fins):
            avg_z = sum(v[2] for v in proj_fin) / 3
            # Gray fins with slight variation
            brightness = 100 + (i * 20) % 40
            color = f'#{brightness:02x}{brightness:02x}{brightness+20:02x}'
            all_faces.append(('fin', proj_fin, None, avg_z, color))
        
        # Sort by depth (painter's algorithm)
        all_faces.sort(key=lambda f: f[3], reverse=True)
        
        # Draw all faces
        for face in all_faces:
            face_type = face[0]
            
            if face_type == 'quad':
                _, indices, verts, _, color = face
                points = []
                for idx in indices:
                    points.extend([verts[idx][0], verts[idx][1]])
                self.create_polygon(points, fill=color, outline='#333', width=1)
            
            elif face_type == 'cone':
                _, (v1_idx, v2_idx), verts, _, color, tip = face
                points = [
                    verts[v1_idx][0], verts[v1_idx][1],
                    verts[v2_idx][0], verts[v2_idx][1],
                    tip[0], tip[1]
                ]
                self.create_polygon(points, fill=color, outline='#aa3300', width=1)
            
            elif face_type == 'fin':
                _, fin_verts, _, _, color = face
                points = []
                for v in fin_verts:
                    points.extend([v[0], v[1]])
                self.create_polygon(points, fill=color, outline='#222', width=1)
        
        # Draw angle labels
        self.create_text(10, self.height - 45, text=f"Roll: {self.roll:.1f}°", 
                        fill='#333', font=("Consolas", 10), anchor=tk.W)
        self.create_text(10, self.height - 30, text=f"Pitch: {self.pitch:.1f}°", 
                        fill='#333', font=("Consolas", 10), anchor=tk.W)
        self.create_text(10, self.height - 15, text=f"Yaw: {self.yaw:.1f}°", 
                        fill='#333', font=("Consolas", 10), anchor=tk.W)
    
    def _draw_grid(self):
        """Draw a simple reference grid"""
        cx, cy = self.width / 2, self.height / 2
        
        # Horizon line
        self.create_line(0, cy, self.width, cy, fill='#ccc', width=1, dash=(4, 4))
        # Vertical line
        self.create_line(cx, 0, cx, self.height, fill='#ccc', width=1, dash=(4, 4))
        
        # Small circle at center
        r = 5
        self.create_oval(cx-r, cy-r, cx+r, cy+r, outline='#aaa', width=1)
    
    def _animate(self):
        """Smooth animation loop"""
        # Interpolate towards target angles
        self.roll += (self.target_roll - self.roll) * self.smoothing
        self.pitch += (self.target_pitch - self.pitch) * self.smoothing
        self.yaw += (self.target_yaw - self.yaw) * self.smoothing
        
        # Redraw
        self._draw_rocket()
        
        # Schedule next frame
        if self.winfo_exists():
            self.after(33, self._animate)  # ~30 FPS
    
    def set_orientation(self, roll: float, pitch: float, yaw: float):
        """Set target orientation (will animate smoothly)"""
        self.target_roll = roll
        self.target_pitch = pitch
        self.target_yaw = yaw


class SITTelemetryPanel(ttk.Frame):
    """Telemetry panel for SIT mode without status indicators"""
    
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
        self.gyro_group = TelemetryGroupWidget(self, "Orientation (deg)")
        self.gyro_group.grid(row=1, column=1, sticky=tk.NSEW, padx=5, pady=5)
        self.gyro_group.add_field("gyro_x", "Roll (X)", "deg")
        self.gyro_group.add_field("gyro_y", "Pitch (Y)", "deg")
        self.gyro_group.add_field("gyro_z", "Yaw (Z)", "deg")

        # GPS group
        self.gps_group = TelemetryGroupWidget(self, "GPS")
        self.gps_group.grid(row=2, column=0, sticky=tk.NSEW, padx=5, pady=5)
        self.gps_group.add_field("gps_altitude", "Alt", "m")
        self.gps_group.add_field("gps_latitude", "Lat", "deg")
        self.gps_group.add_field("gps_longitude", "Lon", "deg")

        # Velocity group
        self.velocity_group = TelemetryGroupWidget(self, "Velocity")
        self.velocity_group.grid(row=2, column=1, sticky=tk.NSEW, padx=5, pady=5)
        self.velocity_group.add_field("velocity", "Speed", "m/s")
    
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

        # GPS
        self.gps_group.update_from_telemetry(packet.gps.gps_altitude, "gps_altitude")
        self.gps_group.update_from_telemetry(packet.gps.gps_latitude, "gps_latitude")
        self.gps_group.update_from_telemetry(packet.gps.gps_longitude, "gps_longitude")

        # Velocity
        self.velocity_group.update_from_telemetry(packet.velocity.velocity, "velocity")
    
    def clear_all(self):
        """Clear all displays"""
        self.altitude_group.clear_all()
        self.pressure_group.clear_all()
        self.accel_group.clear_all()
        self.gyro_group.clear_all()
        self.gps_group.clear_all()
        self.velocity_group.clear_all()


class SITScreen(tk.Toplevel):
    """SIT Mode Screen - Receives telemetry from device"""
    
    def __init__(self, parent, on_stop: Callable, on_close: Callable):
        super().__init__(parent)
        self.on_stop = on_stop
        self.on_close = on_close
        
        # Data storage for plotting
        self.time_data = deque(maxlen=500)
        self.altitude_data = deque(maxlen=500)
        self.start_time = time.time()
        
        # Statistics
        self.packet_count = 0
        self.max_altitude = 0.0
        
        self._build_ui()
        self._start_plot_update()
        
        # Window close handler
        self.protocol("WM_DELETE_WINDOW", self._handle_close)
    
    def _build_ui(self):
        """Build the SIT screen UI"""
        self.title("SIT Mode - System Integration Test")
        self.state('zoomed')  # Windows maximize
        self.minsize(1000, 600)
        
        # Configure grid
        self.columnconfigure(0, weight=1)
        self.columnconfigure(1, weight=2)
        self.rowconfigure(0, weight=1)
        self.rowconfigure(1, weight=0)
        
        # Left panel - Telemetry display
        left_frame = ttk.Frame(self, padding=10)
        left_frame.grid(row=0, column=0, sticky=tk.NSEW)
        
        # Mode indicator
        mode_frame = ttk.Frame(left_frame)
        mode_frame.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(mode_frame, text="SIT MODE", font=("Segoe UI", 14, "bold"),
                 foreground="blue").pack(side=tk.LEFT)
        
        self.status_label = ttk.Label(mode_frame, text="RUNNING", 
                                      font=("Segoe UI", 12, "bold"), foreground="green")
        self.status_label.pack(side=tk.RIGHT)
        
        # Telemetry panel (without status indicators)
        self.telemetry_panel = SITTelemetryPanel(left_frame)
        self.telemetry_panel.pack(fill=tk.X)
        
        # Rocket visualization
        rocket_frame = ttk.LabelFrame(left_frame, text="Rocket Orientation", padding=5)
        rocket_frame.pack(fill=tk.BOTH, expand=True, pady=(10, 0))
        
        self.rocket_viz = RocketVisualization(rocket_frame, width=580, height=345)
        self.rocket_viz.place(relx=0.5, rely=0.5, anchor=tk.CENTER)
        
        # Statistics frame
        stats_frame = ttk.LabelFrame(left_frame, text="Statistics", padding=10)
        stats_frame.pack(fill=tk.X, pady=(10, 0))
        
        stats_grid = ttk.Frame(stats_frame)
        stats_grid.pack(fill=tk.X)
        
        ttk.Label(stats_grid, text="Packets:").grid(row=0, column=0, sticky=tk.W, padx=5)
        self.packets_label = ttk.Label(stats_grid, text="0", font=("Consolas", 10, "bold"))
        self.packets_label.grid(row=0, column=1, sticky=tk.E, padx=5)
        
        ttk.Label(stats_grid, text="Max Alt:").grid(row=0, column=2, sticky=tk.W, padx=5)
        self.max_alt_label = ttk.Label(stats_grid, text="0.00 m", font=("Consolas", 10, "bold"))
        self.max_alt_label.grid(row=0, column=3, sticky=tk.E, padx=5)
        
        ttk.Label(stats_grid, text="Duration:").grid(row=1, column=0, sticky=tk.W, padx=5)
        self.duration_label = ttk.Label(stats_grid, text="0.0 s", font=("Consolas", 10, "bold"))
        self.duration_label.grid(row=1, column=1, sticky=tk.E, padx=5)
        
        # Right panel - Graph
        right_frame = ttk.Frame(self, padding=10)
        right_frame.grid(row=0, column=1, sticky=tk.NSEW)
        
        if MATPLOTLIB_AVAILABLE:
            self._create_plot(right_frame)
        else:
            ttk.Label(right_frame, text="Matplotlib not available", 
                     font=("Segoe UI", 12)).pack(expand=True)
        
        # Bottom panel - Controls
        bottom_frame = ttk.Frame(self, padding=10)
        bottom_frame.grid(row=1, column=0, columnspan=2, sticky=tk.EW)
        
        self.stop_btn = ttk.Button(bottom_frame, text="STOP", command=self._stop_test, width=20)
        self.stop_btn.pack(side=tk.RIGHT)
        
        # Log area
        log_frame = ttk.LabelFrame(bottom_frame, text="Log", padding=5)
        log_frame.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 20))
        
        self.log_text = tk.Text(log_frame, height=3, font=("Consolas", 9), state=tk.DISABLED)
        self.log_text.pack(fill=tk.X)
    
    def _create_plot(self, parent):
        """Create matplotlib plot"""
        self.fig = Figure(figsize=(8, 5), dpi=100)
        self.ax = self.fig.add_subplot(111)
        self.ax.set_xlabel("Time (s)")
        self.ax.set_ylabel("Altitude (m)")
        self.ax.set_title("Altitude vs Time")
        self.ax.grid(True, alpha=0.3)
        
        self.line, = self.ax.plot([], [], 'b-', linewidth=1.5)
        
        self.canvas = FigureCanvasTkAgg(self.fig, master=parent)
        self.canvas.draw()
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
    
    def _start_plot_update(self):
        """Start periodic plot update"""
        self._update_plot()
    
    def _update_plot(self):
        """Update plot with new data"""
        if MATPLOTLIB_AVAILABLE and hasattr(self, 'line'):
            if self.time_data and self.altitude_data:
                self.line.set_data(list(self.time_data), list(self.altitude_data))
                self.ax.relim()
                self.ax.autoscale_view()
                self.canvas.draw_idle()
        
        # Update duration
        elapsed = time.time() - self.start_time
        self.duration_label.config(text=f"{elapsed:.1f} s")
        
        # Schedule next update
        if self.winfo_exists():
            self.after(100, self._update_plot)
    
    def update_telemetry(self, packet: TelemetryPacket):
        """Update display with new telemetry packet"""
        # Update panel
        self.telemetry_panel.update_telemetry(packet)
        
        # Update rocket orientation from gyroscope data
        # Values are already in degrees
        roll = packet.gyroscope.gyro_x.value   # X-axis → Roll
        pitch = packet.gyroscope.gyro_y.value  # Y-axis → Pitch
        yaw = packet.gyroscope.gyro_z.value    # Z-axis → Yaw
        self.rocket_viz.set_orientation(roll, pitch, yaw)
        
        # Update plot data
        current_time = time.time() - self.start_time
        altitude = packet.altitude.altitude.value
        
        self.time_data.append(current_time)
        self.altitude_data.append(altitude)
        
        # Update statistics
        self.packet_count += 1
        self.packets_label.config(text=str(self.packet_count))
        
        if altitude > self.max_altitude:
            self.max_altitude = altitude
            self.max_alt_label.config(text=f"{self.max_altitude:.2f} m")
    
    def add_log(self, message: str):
        """Add message to log"""
        self.log_text.config(state=tk.NORMAL)
        self.log_text.insert(tk.END, message + "\n")
        self.log_text.see(tk.END)
        self.log_text.config(state=tk.DISABLED)
    
    def _stop_test(self):
        """Stop the test"""
        self.status_label.config(text="STOPPED", foreground="red")
        self.stop_btn.config(state=tk.DISABLED)
        if self.on_stop:
            self.on_stop()
    
    def _handle_close(self):
        """Handle window close"""
        self._stop_test()
        if self.on_close:
            self.on_close()
        self.destroy()
