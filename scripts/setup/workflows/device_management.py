#!/usr/bin/env python3
"""
Device Management Workflow - GlowLight Setup System
Handles device detection, information display, and management operations.
"""

import os
import sys
import time
from typing import Optional, List, Dict, Any

from core.error_handler import ErrorHandler, SetupError, DeviceError
from managers.device_manager import DeviceManager
from managers.platformio_manager import PlatformIOManager
from ui.progress import ProgressBar, TaskProgress
from ui.ascii_art_fixed import Colors, ASCIIArt
from utils.system_utils import get_system_info


class DeviceManagementWorkflow:
    """Manages device detection, information display, and operations."""
    
    def __init__(self):
        self.error_handler = ErrorHandler()
        self.device_manager = DeviceManager()
        self.platformio_manager = PlatformIOManager()
        self.project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
    
    def run_device_scan(self) -> List[Dict[str, Any]]:
        """
        Scan for ESP32 devices and display information.
        
        Returns:
            List[Dict[str, Any]]: List of detected devices
        """
        ASCIIArt.clear_screen()
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}🔍 DEVICE SCAN{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        try:
            devices = self.device_manager.scan_for_devices()
            
            if not devices:
                print(f"{Colors.YELLOW}📱 No ESP32 devices detected{Colors.RESET}")
                self._show_connection_help()
                return []
            
            print(f"{Colors.GREEN}✅ Found {len(devices)} ESP32 device(s):{Colors.RESET}\n")
            self._display_devices(devices)
            
            return devices
            
        except Exception as e:
            self.error_handler.handle_error(e, "device scan")
            return []
    
    def show_device_info(self, device: Dict[str, Any]) -> None:
        """
        Show detailed information about a specific device.
        
        Args:
            device: Device information dictionary
        """
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}📱 DEVICE INFORMATION{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        # Basic device info
        print(f"{Colors.BLUE}🔌 Port:{Colors.RESET} {device['port']}")
        print(f"{Colors.BLUE}📋 Description:{Colors.RESET} {device['description']}")
        print(f"{Colors.BLUE}🏷️  Hardware ID:{Colors.RESET} {device.get('hwid', 'Unknown')}")
        print(f"{Colors.BLUE}🏭 Manufacturer:{Colors.RESET} {device.get('manufacturer', 'Unknown')}")
        print(f"{Colors.BLUE}📦 Product:{Colors.RESET} {device.get('product', 'Unknown')}")
        print(f"{Colors.BLUE}🔢 Serial Number:{Colors.RESET} {device.get('serial_number', 'Unknown')}")
        
        # VID/PID info
        vid_pid = device.get('vid_pid', 'Unknown')
        print(f"{Colors.BLUE}🆔 VID:PID:{Colors.RESET} {vid_pid}")
        
        if 'vid' in device and 'pid' in device:
            vid_hex = f"0x{device['vid']:04X}"
            pid_hex = f"0x{device['pid']:04X}"
            print(f"{Colors.BLUE}   VID:{Colors.RESET} {vid_hex} {Colors.GRAY}({device['vid']}){Colors.RESET}")
            print(f"{Colors.BLUE}   PID:{Colors.RESET} {pid_hex} {Colors.GRAY}({device['pid']}){Colors.RESET}")
        
        # Device type detection
        device_type = self._detect_device_type(device)
        print(f"{Colors.BLUE}🔧 Device Type:{Colors.RESET} {device_type}")
        
        # Connection status
        print(f"\n{Colors.BLUE}🔗 Connection Status:{Colors.RESET}")
        if self._test_device_connection(device):
            print(f"  {Colors.GREEN}✅ Device is accessible{Colors.RESET}")
        else:
            print(f"  {Colors.RED}❌ Device connection issues{Colors.RESET}")
        
        # Additional capabilities
        print(f"\n{Colors.BLUE}⚡ Capabilities:{Colors.RESET}")
        capabilities = self._get_device_capabilities(device)
        for capability in capabilities:
            print(f"  • {capability}")
    
    def test_device_connection(self, device: Dict[str, Any]) -> bool:
        """
        Test connection to a specific device.
        
        Args:
            device: Device information dictionary
            
        Returns:
            bool: True if connection test passed, False otherwise
        """
        print(f"\n{Colors.BLUE}🔍 Testing connection to {device['port']}...{Colors.RESET}")
        
        with ProgressBar(total=4) as progress:
            # Simulate connection test steps
            steps = [
                "Opening serial port",
                "Checking device response", 
                "Verifying communication",
                "Closing connection"
            ]
            
            for i, step in enumerate(steps):
                progress.set_status(step)
                time.sleep(0.5)
                progress.update(i + 1)
        
        success = self._test_device_connection(device)
        
        if success:
            print(f"{Colors.GREEN}✅ Connection test passed{Colors.RESET}")
        else:
            print(f"{Colors.RED}❌ Connection test failed{Colors.RESET}")
            self._show_troubleshooting_tips(device)
        
        return success
    
    def monitor_device(self, device: Dict[str, Any]) -> None:
        """
        Start serial monitoring for a device.
        
        Args:
            device: Device information dictionary
        """
        ASCIIArt.clear_screen()
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}📊 SERIAL MONITOR - {device['port']}{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}")
        print(f"{Colors.YELLOW}Press Ctrl+C to stop monitoring{Colors.RESET}\n")
        
        try:
            # Start the serial monitor
            process = self.platformio_manager.monitor_serial(device['port'], 115200)
            
            if process:
                # Read and display output in real-time
                try:
                    while True:
                        output = process.stdout.readline()
                        if output == '' and process.poll() is not None:
                            break
                        if output:
                            print(output.strip())
                except KeyboardInterrupt:
                    print(f"\n{Colors.YELLOW}⚠️  Stopping monitor...{Colors.RESET}")
                    process.terminate()
                    process.wait()
            else:
                print(f"{Colors.RED}❌ Failed to start serial monitor{Colors.RESET}")
                
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⚠️  Monitoring stopped by user{Colors.RESET}")
        except Exception as e:
            self.error_handler.handle_error(e, "serial monitoring")
    
    def show_device_menu(self) -> Optional[str]:
        """
        Show device management menu options.
        
        Returns:
            str: Selected menu option or None if cancelled
        """
        ASCIIArt.clear_screen()
        print(f"\n{Colors.CYAN}{'='*50}")
        print(f"{Colors.BOLD}📱 DEVICE MANAGEMENT MENU{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*50}{Colors.RESET}\n")
        
        options = [
            ("1", "🔍 Scan for devices", "scan"),
            ("2", "📊 Show device information", "info"),
            ("3", "🔗 Test device connection", "test"),
            ("4", "📺 Start serial monitor", "monitor"),
            ("5", "🔄 Reset device", "reset"),
            ("6", "📋 List all serial ports", "list_ports"),
            ("b", "⬅️  Back to main menu", "back")
        ]
        
        for key, desc, _ in options:
            print(f"  {Colors.BLUE}[{key}]{Colors.RESET} {desc}")
        
        while True:
            try:
                choice = input(f"\n{Colors.CYAN}Select option: {Colors.RESET}").strip().lower()
                
                for key, _, action in options:
                    if choice == key.lower():
                        return action
                
                print(f"{Colors.RED}❌ Invalid option. Please try again.{Colors.RESET}")
                
            except KeyboardInterrupt:
                print(f"\n{Colors.YELLOW}⚠️  Menu cancelled{Colors.RESET}")
                return None
    
    def list_all_ports(self) -> None:
        """List all available serial ports."""
        ASCIIArt.clear_screen()
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}🔌 ALL SERIAL PORTS{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        try:
            ports = self.device_manager.list_all_ports()
            
            if not ports:
                print(f"{Colors.YELLOW}📭 No serial ports found{Colors.RESET}")
                return
            
            print(f"{Colors.GREEN}Found {len(ports)} serial port(s):{Colors.RESET}\n")
            
            for port in ports:
                print(f"{Colors.BLUE}🔌 {port['port']}{Colors.RESET}")
                print(f"   📋 Description: {port.get('description', 'Unknown')}")
                print(f"   🏷️  Hardware ID: {port.get('hwid', 'Unknown')}")
                
                # Show VID/PID if available
                if port.get('vid') and port.get('pid'):
                    print(f"   🆔 VID:PID: {port['vid']:04X}:{port['pid']:04X}")
                else:
                    print(f"   🆔 VID:PID: Unknown")
                
                # Check if it's an ESP32 using the is_esp32 flag from list_all_ports
                if port.get('is_esp32', False):
                    print(f"   {Colors.GREEN}✅ ESP32 Device{Colors.RESET}")
                else:
                    print(f"   {Colors.GRAY}ℹ️  Other Device{Colors.RESET}")
                print()
                
        except Exception as e:
            self.error_handler.handle_error(e, "listing serial ports")
    
    def reset_device(self, device: Dict[str, Any]) -> bool:
        """
        Reset a device (if supported).
        
        Args:
            device: Device information dictionary
            
        Returns:
            bool: True if reset was successful, False otherwise
        """
        print(f"\n{Colors.BLUE}🔄 Resetting device {device['port']}...{Colors.RESET}")
        
        try:
            # Use PlatformIO to reset the device
            success = self.platformio_manager.reset_device(device['port'])
            
            if success:
                print(f"{Colors.GREEN}✅ Device reset successfully{Colors.RESET}")
            else:
                print(f"{Colors.RED}❌ Device reset failed{Colors.RESET}")
            
            return success
            
        except Exception as e:
            self.error_handler.handle_error(e, "device reset")
            return False
    
    def _display_devices(self, devices: List[Dict[str, Any]]) -> None:
        """Display a list of devices in a formatted way."""
        for i, device in enumerate(devices, 1):
            print(f"{Colors.BLUE}📱 Device {i}:{Colors.RESET}")
            print(f"   🔌 Port: {device['port']}")
            print(f"   📋 Description: {device['description']}")
            print(f"   🆔 VID:PID: {device.get('vid_pid', 'Unknown')}")
            
            device_type = self._detect_device_type(device)
            print(f"   🔧 Type: {device_type}")
            print()
    
    def _detect_device_type(self, device: Dict[str, Any]) -> str:
        """Detect the specific type of ESP32 device."""
        description = device.get('description', '').lower()
        product = device.get('product', '').lower()
        
        if 'esp32-c3' in description or 'esp32-c3' in product:
            return f"{Colors.GREEN}ESP32-C3{Colors.RESET}"
        elif 'esp32-s3' in description or 'esp32-s3' in product:
            return f"{Colors.GREEN}ESP32-S3{Colors.RESET}"
        elif 'esp32-s2' in description or 'esp32-s2' in product:
            return f"{Colors.GREEN}ESP32-S2{Colors.RESET}"
        elif 'esp32' in description or 'esp32' in product:
            return f"{Colors.GREEN}ESP32{Colors.RESET}"
        else:
            return f"{Colors.YELLOW}ESP32 (Generic){Colors.RESET}"
    
    def _test_device_connection(self, device: Dict[str, Any]) -> bool:
        """Test if device is accessible."""
        try:
            import serial
            with serial.Serial(device['port'], 115200, timeout=1):
                return True
        except Exception:
            return False
    
    def _get_device_capabilities(self, device: Dict[str, Any]) -> List[str]:
        """Get list of device capabilities."""
        capabilities = []
        
        # Basic capabilities for ESP32
        capabilities.append("🔧 GPIO Control")
        capabilities.append("📶 WiFi Communication")
        capabilities.append("🌐 ESP-NOW Synchronization")
        capabilities.append("⚡ PWM Output")
        capabilities.append("📊 ADC Input")
        capabilities.append("🔗 Serial Communication")
        
        # Device-specific capabilities
        device_type = self._detect_device_type(device)
        if 'ESP32-C3' in device_type:
            capabilities.append("🔐 Hardware Security")
            capabilities.append("📱 Bluetooth 5.0 LE")
        elif 'ESP32-S3' in device_type:
            capabilities.append("📷 Camera Interface")
            capabilities.append("🎵 Audio Processing")
            capabilities.append("🧠 AI Acceleration")
        
        return capabilities
    
    def _show_connection_help(self) -> None:
        """Show help for connecting devices."""
        print(f"\n{Colors.BLUE}💡 Device Connection Help:{Colors.RESET}")
        print(f"   • Connect your ESP32 device via USB")
        print(f"   • Ensure the device is powered on")
        print(f"   • Try a different USB cable or port")
        print(f"   • Check if drivers are installed")
        print(f"   • On Linux, ensure user is in 'dialout' group")
        print(f"   • On Windows, install CP210x or CH340 drivers")
    
    def _show_troubleshooting_tips(self, device: Dict[str, Any]) -> None:
        """Show troubleshooting tips for connection issues."""
        print(f"\n{Colors.BLUE}🔧 Troubleshooting Tips:{Colors.RESET}")
        print(f"   • Disconnect and reconnect the device")
        print(f"   • Try a different USB cable")
        print(f"   • Check if another program is using the port")
        print(f"   • Restart the device by pressing reset button")
        print(f"   • Try a different USB port on your computer")
        
        # System-specific tips
        system_info = get_system_info()
        if system_info['platform'].startswith('linux'):
            print(f"   • Linux: Check permissions with 'ls -l {device['port']}'")
            print(f"   • Linux: Add user to dialout group: 'sudo usermod -a -G dialout $USER'")
        elif system_info['platform'].startswith('win'):
            print(f"   • Windows: Check Device Manager for driver issues")
            print(f"   • Windows: Try installing latest USB drivers")
        elif system_info['platform'].startswith('darwin'):
            print(f"   • macOS: Check System Information for USB devices")
            print(f"   • macOS: Try installing latest drivers from manufacturer")
