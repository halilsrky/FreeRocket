"""
Telemetry Data Models
Professional ground station telemetry data structures
"""
from dataclasses import dataclass, field
from typing import Optional, Dict, Any, List
from datetime import datetime


@dataclass
class TelemetryValue:
    """Single telemetry value with metadata"""
    label: str
    value: float
    unit: str
    precision: int = 2
    
    def formatted(self) -> str:
        """Return formatted string value"""
        return f"{self.value:.{self.precision}f}"
    
    def display(self) -> str:
        """Return value with unit for display"""
        return f"{self.formatted()} {self.unit}"


@dataclass
class AltitudeData:
    """Altitude telemetry group"""
    altitude: TelemetryValue = field(default_factory=lambda: TelemetryValue("Altitude", 0.0, "m", 2))
    
    @classmethod
    def from_raw(cls, altitude: float) -> "AltitudeData":
        return cls(
            altitude=TelemetryValue("Altitude", altitude, "m", 2)
        )
    
    def to_dict(self) -> Dict[str, TelemetryValue]:
        return {"altitude": self.altitude}


@dataclass
class PressureData:
    """Pressure telemetry group"""
    pressure: TelemetryValue = field(default_factory=lambda: TelemetryValue("Pressure", 0.0, "hPa", 2))
    
    @classmethod
    def from_raw(cls, pressure: float) -> "PressureData":
        return cls(
            pressure=TelemetryValue("Pressure", pressure, "hPa", 2)
        )
    
    def to_dict(self) -> Dict[str, TelemetryValue]:
        return {"pressure": self.pressure}


@dataclass
class AccelerationData:
    """Acceleration telemetry group (3-axis)"""
    acc_x: TelemetryValue = field(default_factory=lambda: TelemetryValue("Acc X", 0.0, "g", 3))
    acc_y: TelemetryValue = field(default_factory=lambda: TelemetryValue("Acc Y", 0.0, "g", 3))
    acc_z: TelemetryValue = field(default_factory=lambda: TelemetryValue("Acc Z", 0.0, "g", 3))
    
    @classmethod
    def from_raw(cls, x: float, y: float, z: float) -> "AccelerationData":
        return cls(
            acc_x=TelemetryValue("Acc X", x, "g", 3),
            acc_y=TelemetryValue("Acc Y", y, "g", 3),
            acc_z=TelemetryValue("Acc Z", z, "g", 3)
        )
    
    def to_dict(self) -> Dict[str, TelemetryValue]:
        return {
            "acc_x": self.acc_x,
            "acc_y": self.acc_y,
            "acc_z": self.acc_z
        }


@dataclass
class GyroscopeData:
    """Gyroscope telemetry group (3-axis)"""
    gyro_x: TelemetryValue = field(default_factory=lambda: TelemetryValue("Gyro X", 0.0, "deg/s", 2))
    gyro_y: TelemetryValue = field(default_factory=lambda: TelemetryValue("Gyro Y", 0.0, "deg/s", 2))
    gyro_z: TelemetryValue = field(default_factory=lambda: TelemetryValue("Gyro Z", 0.0, "deg/s", 2))
    
    @classmethod
    def from_raw(cls, x: float, y: float, z: float) -> "GyroscopeData":
        return cls(
            gyro_x=TelemetryValue("Gyro X", x, "deg/s", 2),
            gyro_y=TelemetryValue("Gyro Y", y, "deg/s", 2),
            gyro_z=TelemetryValue("Gyro Z", z, "deg/s", 2)
        )
    
    def to_dict(self) -> Dict[str, TelemetryValue]:
        return {
            "gyro_x": self.gyro_x,
            "gyro_y": self.gyro_y,
            "gyro_z": self.gyro_z
        }


@dataclass
class GPSData:
    """GPS telemetry group"""
    gps_altitude: TelemetryValue = field(default_factory=lambda: TelemetryValue("GPS Altitude", 0.0, "m", 2))
    gps_latitude: TelemetryValue = field(default_factory=lambda: TelemetryValue("Latitude", 0.0, "deg", 6))
    gps_longitude: TelemetryValue = field(default_factory=lambda: TelemetryValue("Longitude", 0.0, "deg", 6))

    @classmethod
    def from_raw(cls, altitude: float, latitude: float, longitude: float) -> "GPSData":
        return cls(
            gps_altitude=TelemetryValue("GPS Altitude", altitude, "m", 2),
            gps_latitude=TelemetryValue("Latitude", latitude, "deg", 6),
            gps_longitude=TelemetryValue("Longitude", longitude, "deg", 6)
        )

    def to_dict(self) -> Dict[str, TelemetryValue]:
        return {
            "gps_altitude": self.gps_altitude,
            "gps_latitude": self.gps_latitude,
            "gps_longitude": self.gps_longitude
        }


@dataclass
class VelocityData:
    """Velocity telemetry group"""
    velocity: TelemetryValue = field(default_factory=lambda: TelemetryValue("Velocity", 0.0, "m/s", 2))

    @classmethod
    def from_raw(cls, velocity: float) -> "VelocityData":
        return cls(
            velocity=TelemetryValue("Velocity", velocity, "m/s", 2)
        )

    def to_dict(self) -> Dict[str, TelemetryValue]:
        return {"velocity": self.velocity}


@dataclass
class StatusFlags:
    """System status flags"""
    launch_detected: bool = False
    burnout_timeout: bool = False
    min_altitude_exceeded: bool = False
    angle_exceeded: bool = False
    descent_detected: bool = False
    drogue_deployed: bool = False
    main_altitude_reached: bool = False
    main_deployed: bool = False
    
    @classmethod
    def from_16bit(cls, status_word: int) -> "StatusFlags":
        return cls(
            launch_detected=bool(status_word & 0x0001),
            burnout_timeout=bool(status_word & 0x0002),
            min_altitude_exceeded=bool(status_word & 0x0004),
            angle_exceeded=bool(status_word & 0x0008),
            descent_detected=bool(status_word & 0x0010),
            drogue_deployed=bool(status_word & 0x0020),
            main_altitude_reached=bool(status_word & 0x0040),
            main_deployed=bool(status_word & 0x0080)
        )
    
    def to_dict(self) -> Dict[str, bool]:
        return {
            "launch_detected": self.launch_detected,
            "burnout_timeout": self.burnout_timeout,
            "min_altitude_exceeded": self.min_altitude_exceeded,
            "angle_exceeded": self.angle_exceeded,
            "descent_detected": self.descent_detected,
            "drogue_deployed": self.drogue_deployed,
            "main_altitude_reached": self.main_altitude_reached,
            "main_deployed": self.main_deployed
        }
    
    def active_flags(self) -> List[str]:
        """Return list of active flag names"""
        flags = []
        if self.launch_detected:
            flags.append("Launch Detected")
        if self.burnout_timeout:
            flags.append("Burnout Timeout")
        if self.min_altitude_exceeded:
            flags.append("Min Altitude Exceeded")
        if self.angle_exceeded:
            flags.append("Angle Exceeded")
        if self.descent_detected:
            flags.append("Descent Detected")
        if self.drogue_deployed:
            flags.append("Drogue Deployed")
        if self.main_altitude_reached:
            flags.append("Main Altitude Reached")
        if self.main_deployed:
            flags.append("Main Deployed")
        return flags


@dataclass
class TelemetryPacket:
    """Complete telemetry packet with all sensor groups"""
    timestamp: datetime
    altitude: AltitudeData
    pressure: PressureData
    acceleration: AccelerationData
    gyroscope: GyroscopeData
    gps: GPSData
    velocity: VelocityData
    packet_valid: bool = True
    checksum: int = 0
    
    @classmethod
    def from_raw_values(cls, values: List[float], checksum: int = 0) -> "TelemetryPacket":
        """Create packet from raw values [alt, press, ax, ay, az, gx, gy, gz, gps_alt, lat, lon, vel]"""
        if len(values) < 12:
            values = values + [0.0] * (12 - len(values))
        
        return cls(
            timestamp=datetime.now(),
            altitude=AltitudeData.from_raw(values[0]),
            pressure=PressureData.from_raw(values[1]),
            acceleration=AccelerationData.from_raw(values[2], values[3], values[4]),
            gyroscope=GyroscopeData.from_raw(values[5], values[6], values[7]),
            gps=GPSData.from_raw(values[8], values[9], values[10]),
            velocity=VelocityData.from_raw(values[11]),
            packet_valid=True,
            checksum=checksum
        )
    
    def to_flat_dict(self) -> Dict[str, TelemetryValue]:
        """Return flat dictionary of all telemetry values"""
        result = {}
        result.update(self.altitude.to_dict())
        result.update(self.pressure.to_dict())
        result.update(self.acceleration.to_dict())
        result.update(self.gyroscope.to_dict())
        result.update(self.gps.to_dict())
        result.update(self.velocity.to_dict())
        return result
    
    def to_grouped_dict(self) -> Dict[str, Dict[str, TelemetryValue]]:
        """Return grouped dictionary for GUI sections"""
        return {
            "altitude": self.altitude.to_dict(),
            "pressure": self.pressure.to_dict(),
            "acceleration": self.acceleration.to_dict(),
            "gyroscope": self.gyroscope.to_dict(),
            "gps": self.gps.to_dict(),
            "velocity": self.velocity.to_dict()
        }


@dataclass
class StatusPacket:
    """Status packet from device"""
    timestamp: datetime
    status_word: int
    flags: StatusFlags
    packet_valid: bool = True
    
    @classmethod
    def from_raw(cls, status_low: int, status_high: int) -> "StatusPacket":
        status_word = status_low | (status_high << 8)
        return cls(
            timestamp=datetime.now(),
            status_word=status_word,
            flags=StatusFlags.from_16bit(status_word),
            packet_valid=True
        )


@dataclass
class ConnectionConfig:
    """Serial connection configuration"""
    port: str = "COM1"
    baudrate: int = 115200
    databits: int = 8
    parity: str = "N"
    stopbits: float = 1.0
    timeout: float = 1.0


@dataclass
class TestSession:
    """Test session metadata"""
    session_id: str = ""
    mode: str = ""  # "SIT" or "SUT"
    start_time: Optional[datetime] = None
    end_time: Optional[datetime] = None
    packet_count: int = 0
    error_count: int = 0
    csv_file: Optional[str] = None
    log_file: Optional[str] = None
    
    def duration_seconds(self) -> float:
        if self.start_time is None:
            return 0.0
        end = self.end_time or datetime.now()
        return (end - self.start_time).total_seconds()
