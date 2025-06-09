"""
Device Manager

Handles ESP32 device detection, port management, and serial communication.
"""

import os
import subprocess
import serial.tools.list_ports
from pathlib import Path

from core.error_handler import ErrorHandler, DeviceError
from core.validator import ConfigValidator
from ui.ascii_art_fixed import ASCIIArt


class DeviceManager:
    """Manages ESP32 devices and serial ports."""
    
    # ESP32 vendor IDs and product IDs for detection
    ESP32_VID_PIDS = [
        (0x10C4, 0xEA60),  # Silicon Labs CP210x (common USB-to-serial)
        (0x10C4, 0xEA71),  # Silicon Labs CP210x variant
        (0x1A86, 0x7523),  # QinHeng Electronics CH340
        (0x1A86, 0x55D4),  # QinHeng Electronics CH9102
        (0x0403, 0x6001),  # FTDI FT232RL
        (0x0403, 0x6014),  # FTDI FT232H
        (0x0403, 0x6015),  # FTDI FT X-Series
        (0x239A, 0x8014),  # Adafruit ESP32-S2
        (0x239A, 0x80C2),  # Adafruit ESP32-C3
        (0x303A, 0x1001),  # Espressif ESP32-C3 USB JTAG/serial debug unit
        (0x303A, 0x0002),  # Espressif ESP32-S2 Native USB
        (0x303A, 0x8001),  # Espressif ESP32-S3 USB JTAG/serial debug unit
    ]
    
    def __init__(self):
        """Initialize device manager."""
        self.validator = ConfigValidator()
        
    def scan_devices(self):
        """Scan for connected ESP32 devices.
        
        Returns:
            list: List of device info dictionaries
        """
        devices = []
        
        try:
            # Get all serial ports
            ports = serial.tools.list_ports.comports()
            
            for port in ports:
                # Skip virtual ttyS* ports - these are not real USB devices
                if '/dev/ttyS' in port.device:
                    continue
                    
                device_info = {
                    'port': port.device,
                    'description': port.description,
                    'hwid': port.hwid,
                    'vid': port.vid,
                    'pid': port.pid,
                    'is_esp32': self._is_esp32_device(port)
                }
                
                devices.append(device_info)
                
        except Exception as e:
            ErrorHandler.handle_error(e, "scanning for devices")
            
        return devices
        
    def get_esp32_devices(self):
        """Get only ESP32 devices.
        
        Returns:
            list: List of ESP32 device info dictionaries
        """
        all_devices = self.scan_devices()
        return [device for device in all_devices if device['is_esp32']]
    
    def list_all_ports(self):
        """Get all available serial ports with real hardware (excluding virtual ports).
        
        Returns:
            list: List of real serial port info dictionaries
        """
        ports = []
        
        try:
            # Get all serial ports without filtering
            import serial.tools.list_ports
            all_ports = serial.tools.list_ports.comports()
            
            for port in all_ports:
                # Skip virtual/built-in serial ports
                if '/dev/ttyS' in port.device:
                    continue
                    
                # Skip if no real hardware ID (virtual ports)
                if not port.hwid or port.hwid == 'n/a':
                    continue
                
                port_info = {
                    'port': port.device,
                    'description': port.description or 'Unknown',
                    'hwid': port.hwid or 'Unknown',
                    'vid': port.vid,
                    'pid': port.pid,
                    'manufacturer': getattr(port, 'manufacturer', 'Unknown'),
                    'product': getattr(port, 'product', 'Unknown'),
                    'serial_number': getattr(port, 'serial_number', 'Unknown'),
                    'is_esp32': self._is_esp32_device(port)
                }
                ports.append(port_info)
                
        except Exception as e:
            ErrorHandler.handle_error(e, "listing all serial ports")
            
        return ports
    
    def scan_for_devices(self):
        """Scan for connected ESP32 devices (alias for scan_devices).
        
        Returns:
            list: List of device info dictionaries
        """
        return self.scan_devices()
        
    def show_devices(self, esp32_only=True):
        """Display available devices in a formatted table.
        
        Args:
            esp32_only: If True, show only ESP32 devices
        """
        devices = self.get_esp32_devices() if esp32_only else self.scan_devices()
        
        if not devices:
            ASCIIArt.show_warning("No devices found!")
            if esp32_only:
                ASCIIArt.show_info("Make sure your ESP32 is connected via USB")
            return
            
        ASCIIArt.show_separator("Connected Devices")
        
        print(f"{'#':<3} {'Port':<15} {'Description':<30} {'ESP32':<6}")
        print("-" * 60)
        
        for i, device in enumerate(devices):
            esp32_indicator = "✅" if device['is_esp32'] else "❌"
            print(f"{i:<3} {device['port']:<15} {device['description']:<30} {esp32_indicator:<6}")
            
        print("-" * 60)
        print(f"Found {len(devices)} device(s)")
        
    def select_device(self, devices=None, prompt="Select device"):
        """Interactive device selection.
        
        Args:
            devices: List of devices (None to scan)
            prompt: Selection prompt
            
        Returns:
            dict or None: Selected device info
        """
        if devices is None:
            devices = self.get_esp32_devices()
            
        if not devices:
            ASCIIArt.show_error("No ESP32 devices found!")
            return None
            
        if len(devices) == 1:
            ASCIIArt.show_info(f"Auto-selecting only device: {devices[0]['port']}")
            return devices[0]
            
        self.show_devices(esp32_only=True)
        
        while True:
            try:
                choice = input(f"\n🎯 {prompt} (0-{len(devices)-1}): ").strip()
                
                if not choice.isdigit():
                    ASCIIArt.show_error("Please enter a number!")
                    continue
                    
                index = int(choice)
                
                if 0 <= index < len(devices):
                    selected = devices[index]
                    ASCIIArt.show_success(f"Selected: {selected['port']} - {selected['description']}")
                    return selected
                else:
                    ASCIIArt.show_error(f"Please enter a number between 0 and {len(devices)-1}")
                    
            except KeyboardInterrupt:
                print("\n")
                return None
                
    def select_multiple_devices(self, devices=None):
        """Select multiple devices for batch operations.
        
        Args:
            devices: List of devices (None to scan)
            
        Returns:
            list: List of selected devices
        """
        if devices is None:
            devices = self.get_esp32_devices()
            
        if not devices:
            ASCIIArt.show_error("No ESP32 devices found!")
            return []
            
        self.show_devices(esp32_only=True)
        
        print(f"\n📋 Select devices to flash:")
        print(f"   Enter 'all' for all devices")
        print(f"   Enter device numbers separated by commas (e.g., 0,2,3)")
        print(f"   Enter 'none' to cancel")
        
        while True:
            try:
                choice = input(f"\n🎯 Selection: ").strip().lower()
                
                if choice == 'none':
                    return []
                    
                if choice == 'all':
                    ASCIIArt.show_success(f"Selected all {len(devices)} devices")
                    return devices
                    
                # Parse comma-separated indices
                try:
                    indices = [int(x.strip()) for x in choice.split(',')]
                    selected_devices = []
                    
                    for index in indices:
                        if 0 <= index < len(devices):
                            selected_devices.append(devices[index])
                        else:
                            ASCIIArt.show_error(f"Invalid device index: {index}")
                            break
                    else:
                        # All indices were valid
                        ports = [d['port'] for d in selected_devices]
                        ASCIIArt.show_success(f"Selected devices: {', '.join(ports)}")
                        return selected_devices
                        
                except ValueError:
                    ASCIIArt.show_error("Invalid format! Use numbers separated by commas")
                    
            except KeyboardInterrupt:
                print("\n")
                return []
                
    def test_device_connection(self, port):
        """Test if device is responsive.
        
        Args:
            port: Serial port path
            
        Returns:
            bool: True if device responds
        """
        try:
            import serial
            
            with serial.Serial(port, 115200, timeout=2) as ser:
                # Try to read any existing data
                ser.read_all()
                return True
                
        except Exception:
            return False
            
    def get_device_info(self, port):
        """Get detailed device information.
        
        Args:
            port: Serial port path
            
        Returns:
            dict: Device information
        """
        try:
            ports = serial.tools.list_ports.comports()
            
            for p in ports:
                if p.device == port:
                    return {
                        'port': p.device,
                        'description': p.description,
                        'manufacturer': getattr(p, 'manufacturer', 'Unknown'),
                        'product': getattr(p, 'product', 'Unknown'),
                        'serial_number': getattr(p, 'serial_number', 'Unknown'),
                        'vid': p.vid,
                        'pid': p.pid,
                        'hwid': p.hwid
                    }
                    
        except Exception as e:
            ErrorHandler.handle_error(e, "getting device info")
            
        return None
        
    def _is_esp32_device(self, port):
        """Check if a port belongs to an ESP32 device.
        
        Args:
            port: Serial port object
            
        Returns:
            bool: True if likely ESP32 device
        """
        # Check VID/PID first (most reliable)
        if port.vid and port.pid:
            if (port.vid, port.pid) in self.ESP32_VID_PIDS:
                return True
                
        # Check for common ESP32 descriptions
        if port.description:
            description = port.description.lower()
            esp32_keywords = ['esp32', 'jtag', 'debug unit', 'cp210', 'ch340', 'ftdi']
            if any(keyword in description for keyword in esp32_keywords):
                return True
                
        # Default to True for USB devices (ttyUSB*, ttyACM*) without known VID/PID
        # This covers many ESP32 boards with generic USB-to-serial chips
        if '/dev/ttyUSB' in port.device or '/dev/ttyACM' in port.device:
            return True

        return False


class SerialMonitor:
    """Simple serial monitor implementation."""
    
    def __init__(self, port, baud=115200):
        """Initialize serial monitor.
        
        Args:
            port: Serial port path
            baud: Baud rate
        """
        self.port = port
        self.baud = baud
        
    def start(self):
        """Start monitoring serial output."""
        try:
            import serial
            
            ASCIIArt.show_info(f"Opening serial monitor on {self.port} at {self.baud} baud")
            ASCIIArt.show_info("Press Ctrl+C to exit")
            print("-" * 60)
            
            with serial.Serial(self.port, self.baud, timeout=1) as ser:
                while True:
                    if ser.in_waiting:
                        data = ser.readline()
                        try:
                            print(data.decode('utf-8'), end='')
                        except UnicodeDecodeError:
                            print(data, end='')
                            
        except KeyboardInterrupt:
            print("\n" + "-" * 60)
            ASCIIArt.show_info("Serial monitor closed")
            
        except Exception as e:
            ErrorHandler.handle_error(e, "running serial monitor")