"""
Serial Communication Handler
Handles UART communication with UKB device
"""
import serial
import struct
import threading
import time
from typing import Optional, Callable, List
from dataclasses import dataclass
from queue import Queue

from models import TelemetryPacket, StatusPacket, ConnectionConfig


# Protocol Constants
PACKET_HEADER_TELEMETRY = 0xAB
PACKET_HEADER_STATUS = 0xAA
PACKET_FOOTER = bytes([0x0D, 0x0A])

SIT_FLOAT_COUNT = 12
SIT_FLOAT_COUNT_LEGACY = 8
SIT_PACKET_SIZE = 1 + (SIT_FLOAT_COUNT * 4) + 1 + 2
SIT_PACKET_SIZE_LEGACY = 1 + (SIT_FLOAT_COUNT_LEGACY * 4) + 1 + 2

# Command Packets
SIT_CMD = bytearray([0xAA, 0x20, 0xCA, 0x0D, 0x0A])
SUT_CMD = bytearray([0xAA, 0x22, 0xCC, 0x0D, 0x0A])
STOP_CMD = bytearray([0xAA, 0x24, 0xCE, 0x0D, 0x0A])


class SerialHandler:
    """Thread-safe serial communication handler"""
    
    def __init__(self):
        self._serial: Optional[serial.Serial] = None
        self._config: Optional[ConnectionConfig] = None
        self._connected = False
        self._running = False
        self._mode: Optional[str] = None
        
        # Callbacks
        self._telemetry_callback: Optional[Callable[[TelemetryPacket], None]] = None
        self._status_callback: Optional[Callable[[StatusPacket], None]] = None
        self._error_callback: Optional[Callable[[str], None]] = None
        
        # Threads
        self._receiver_thread: Optional[threading.Thread] = None
        self._sender_thread: Optional[threading.Thread] = None
        
        # SUT mode data
        self._csv_data: Optional[List[List[float]]] = None
        self._csv_index = 0
        
        # Thread lock
        self._lock = threading.Lock()
    
    @property
    def connected(self) -> bool:
        return self._connected
    
    @property
    def mode(self) -> Optional[str]:
        return self._mode
    
    def set_telemetry_callback(self, callback: Callable[[TelemetryPacket], None]):
        self._telemetry_callback = callback
    
    def set_status_callback(self, callback: Callable[[StatusPacket], None]):
        self._status_callback = callback
    
    def set_error_callback(self, callback: Callable[[str], None]):
        self._error_callback = callback
    
    def connect(self, config: ConnectionConfig) -> bool:
        """Establish serial connection"""
        try:
            self._serial = serial.Serial(
                port=config.port,
                baudrate=config.baudrate,
                bytesize=config.databits,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=config.timeout
            )
            self._config = config
            self._connected = True
            return True
        except serial.SerialException as e:
            self._report_error(f"Connection failed: {e}")
            return False
    
    def disconnect(self):
        """Close serial connection"""
        self.stop_mode()
        if self._serial and self._serial.is_open:
            self._serial.close()
        self._connected = False
        self._serial = None
    
    def start_sit_mode(self):
        """Start SIT mode - receive telemetry from device"""
        if not self._connected:
            self._report_error("Not connected")
            return
        
        self._mode = "SIT"
        self._running = True
        self._send_command(SIT_CMD)
        
        self._receiver_thread = threading.Thread(target=self._sit_receiver_loop, daemon=True)
        self._receiver_thread.start()
    
    def start_sut_mode(self, csv_data: Optional[List[List[float]]] = None):
        """Start SUT mode - send telemetry, receive status"""
        if not self._connected:
            self._report_error("Not connected")
            return
        
        self._mode = "SUT"
        self._running = True
        self._csv_data = csv_data
        self._csv_index = 0
        
        # Aggressive reset sequence (like disconnect/reconnect does)
        if self._serial:
            # First send STOP to reset STM32 state
            self._serial.write(STOP_CMD)
            time.sleep(0.1)
            
            # Flush both buffers
            self._serial.reset_input_buffer()
            self._serial.reset_output_buffer()
            time.sleep(0.05)
        
        # Start receiver FIRST (like test.py does)
        self._receiver_thread = threading.Thread(target=self._sut_receiver_loop, daemon=True)
        self._receiver_thread.start()
        
        # Small delay to ensure receiver is ready
        time.sleep(0.1)
        
        # Send SUT command
        self._send_command(SUT_CMD)
        
        # Flush input buffer after command to clear any echo
        if self._serial:
            time.sleep(0.05)
            self._serial.reset_input_buffer()
        
        # Start sender thread
        self._sender_thread = threading.Thread(target=self._sut_sender_loop, daemon=True)
        self._sender_thread.start()
    
    def stop_mode(self):
        """Stop current mode"""
        self._running = False
        if self._connected and self._serial:
            self._send_command(STOP_CMD)
        
        if self._receiver_thread and self._receiver_thread.is_alive():
            self._receiver_thread.join(timeout=1.0)
        if self._sender_thread and self._sender_thread.is_alive():
            self._sender_thread.join(timeout=1.0)
        
        self._mode = None
        self._receiver_thread = None
        self._sender_thread = None
    
    def _send_command(self, cmd: bytearray):
        """Send command to device"""
        if self._serial and self._serial.is_open:
            with self._lock:
                self._serial.write(cmd)
    
    def _report_error(self, message: str):
        """Report error through callback"""
        if self._error_callback:
            self._error_callback(message)
    
    def _sit_receiver_loop(self):
        """Receive loop for SIT mode - telemetry packets"""
        buffer = bytearray()
        
        while self._running and self._mode == "SIT":
            try:
                if self._serial and self._serial.in_waiting > 0:
                    with self._lock:
                        buffer.extend(self._serial.read(self._serial.in_waiting))
                
                while len(buffer) >= SIT_PACKET_SIZE_LEGACY:
                    header_idx = buffer.find(PACKET_HEADER_TELEMETRY)
                    if header_idx == -1:
                        buffer.clear()
                        break
                    
                    if header_idx > 0:
                        buffer = buffer[header_idx:]
                    
                    if len(buffer) < SIT_PACKET_SIZE_LEGACY:
                        break

                    packet = None
                    consumed = 0

                    if len(buffer) >= SIT_PACKET_SIZE:
                        packet = self._parse_telemetry_packet(buffer[:SIT_PACKET_SIZE], SIT_FLOAT_COUNT)
                        if packet:
                            consumed = SIT_PACKET_SIZE
                        else:
                            packet = self._parse_telemetry_packet(buffer[:SIT_PACKET_SIZE_LEGACY], SIT_FLOAT_COUNT_LEGACY)
                            if packet:
                                consumed = SIT_PACKET_SIZE_LEGACY
                    else:
                        if buffer[SIT_PACKET_SIZE_LEGACY - 2:SIT_PACKET_SIZE_LEGACY] != PACKET_FOOTER:
                            break
                        packet = self._parse_telemetry_packet(buffer[:SIT_PACKET_SIZE_LEGACY], SIT_FLOAT_COUNT_LEGACY)
                        if packet:
                            consumed = SIT_PACKET_SIZE_LEGACY

                    if packet:
                        if self._telemetry_callback:
                            self._telemetry_callback(packet)
                        buffer = buffer[consumed:]
                    else:
                        buffer = buffer[1:]
                
                time.sleep(0.01)
            except Exception as e:
                self._report_error(f"SIT receiver error: {e}")
    
    def _sut_sender_loop(self):
        """Sender loop for SUT mode - telemetry packets at 10Hz"""
        while self._running and self._mode == "SUT":
            try:
                packet = self._create_sut_packet()
                if packet is None:
                    # CSV ended or error
                    break
                    
                if self._serial and self._serial.is_open:
                    with self._lock:
                        self._serial.write(packet)
                    
                    # Parse for callback - create telemetry packet for GUI
                    values = self._extract_values_from_packet(packet)
                    if values and self._telemetry_callback:
                        telemetry = TelemetryPacket.from_raw_values(values)
                        self._telemetry_callback(telemetry)
                
                time.sleep(0.1)  # 10Hz
            except Exception as e:
                self._report_error(f"SUT sender error: {e}")
                break
    
    def _sut_receiver_loop(self):
        """Receiver loop for SUT mode - status packets"""
        buffer = bytearray()
        
        while self._running and self._mode == "SUT":
            try:
                # Read available data
                if self._serial and self._serial.is_open:
                    in_waiting = self._serial.in_waiting
                    if in_waiting > 0:
                        with self._lock:
                            new_data = self._serial.read(in_waiting)
                        buffer.extend(new_data)
                
                # Look for status packets ending with 0D 0A
                while len(buffer) >= 6:
                    # Find 0D 0A footer
                    footer_idx = -1
                    for i in range(len(buffer) - 1):
                        if buffer[i] == 0x0D and buffer[i+1] == 0x0A:
                            footer_idx = i
                            break
                    
                    if footer_idx == -1:
                        # No footer found, keep last bytes
                        if len(buffer) > 50:
                            buffer = buffer[-50:]
                        break
                    
                    # Check if this could be a 6-byte status packet
                    if footer_idx >= 4:
                        potential_start = footer_idx - 4
                        potential_packet = buffer[potential_start:footer_idx + 2]
                        
                        if len(potential_packet) == 6:
                            header = potential_packet[0]
                            status_low = potential_packet[1]
                            status_high = potential_packet[2]
                            checksum = potential_packet[3]
                            
                            # Check if it's a valid status packet (0xAA header)
                            if header == 0xAA:
                                calc_checksum = (header + status_low + status_high) % 256
                                if calc_checksum == checksum:
                                    packet = StatusPacket.from_raw(status_low, status_high)
                                    if self._status_callback:
                                        self._status_callback(packet)
                    
                    # Remove processed data
                    buffer = buffer[footer_idx + 2:]
                
                time.sleep(0.01)
            except Exception as e:
                self._report_error(f"SUT receiver error: {e}")
    
    def _parse_telemetry_packet(self, data: bytes, float_count: int) -> Optional[TelemetryPacket]:
        """Parse telemetry packet with float_count values"""
        expected_size = 1 + (float_count * 4) + 1 + 2
        if len(data) != expected_size:
            return None

        header = data[0]
        footer = data[expected_size - 2:expected_size]
        
        if header != PACKET_HEADER_TELEMETRY or footer != PACKET_FOOTER:
            return None
        
        # Verify checksum
        checksum_index = expected_size - 3
        calculated_checksum = sum(data[:checksum_index]) % 256
        received_checksum = data[checksum_index]
        
        if calculated_checksum != received_checksum:
            return None
        
        # Extract float values
        values = []
        for i in range(float_count):
            start = 1 + (i * 4)
            float_bytes = data[start:start + 4]
            value = struct.unpack('>f', float_bytes)[0]
            values.append(value)
        
        return TelemetryPacket.from_raw_values(values, received_checksum)
    
    def _parse_status_packet(self, data: bytes) -> Optional[StatusPacket]:
        """Parse 6-byte status packet"""
        if len(data) != 6:
            return None
        
        header, status_low, status_high, checksum, footer1, footer2 = data
        
        if header != PACKET_HEADER_STATUS:
            return None
        if footer1 != 0x0D or footer2 != 0x0A:
            return None
        
        # Verify checksum
        calculated = (header + status_low + status_high) % 256
        if calculated != checksum:
            return None
        
        return StatusPacket.from_raw(status_low, status_high)
    
    def _create_sut_packet(self) -> Optional[bytes]:
        """Create 36-byte SUT telemetry packet"""
        if self._csv_data:
            if self._csv_index >= len(self._csv_data):
                self._running = False
                return None
            
            values = self._csv_data[self._csv_index]
            self._csv_index += 1
        else:
            # Random test data
            import random
            values = [
                random.uniform(0.0, 1000.0),
                random.uniform(900.0, 1100.0),
                random.uniform(-2.0, 2.0),
                random.uniform(-2.0, 2.0),
                random.uniform(-2.0, 2.0),
                random.uniform(-250.0, 250.0),
                random.uniform(-250.0, 250.0),
                random.uniform(-250.0, 250.0)
            ]
        
        # Ensure 8 values
        while len(values) < 8:
            values.append(0.0)
        
        packet = bytearray(36)
        packet[0] = PACKET_HEADER_TELEMETRY
        
        for i, value in enumerate(values[:8]):
            float_bytes = struct.pack('>f', float(value))
            start = 1 + (i * 4)
            packet[start:start + 4] = float_bytes
        
        packet[34] = 0x0D
        packet[35] = 0x0A
        
        checksum = sum(packet[:33]) % 256
        packet[33] = checksum
        
        return bytes(packet)
    
    def _extract_values_from_packet(self, packet: bytes) -> Optional[List[float]]:
        """Extract float values from packet"""
        if len(packet) != 36:
            return None
        
        values = []
        for i in range(8):
            start = 1 + (i * 4)
            float_bytes = packet[start:start + 4]
            value = struct.unpack('>f', float_bytes)[0]
            values.append(value)
        
        return values
