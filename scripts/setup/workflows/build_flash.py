#!/usr/bin/env python3
"""
Build and Flash Workflow - GlowLight Setup System
Handles building and flashing firmware with comprehensive error handling and user guidance.
"""

import os
import sys
import time
from typing import Optional, List, Dict, Any

from core.error_handler import ErrorHandler, SetupError, PlatformIOError, DeviceError
from managers.platformio_manager import PlatformIOManager
from managers.device_manager import DeviceManager
from managers.config_manager import ConfigManager
from ui.progress import ProgressBar, TaskProgress
from ui.ascii_art_fixed import Colors, ASCIIArt
from utils.file_utils import check_file_exists


class BuildFlashWorkflow:
    """Manages the build and flash workflow for GlowLight firmware."""
    
    def __init__(self):
        self.error_handler = ErrorHandler()
        self.platformio_manager = PlatformIOManager()
        self.device_manager = DeviceManager()
        self.config_manager = ConfigManager()
        self.project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
    
    def run_build_workflow(self) -> bool:
        """
        Run the complete build workflow.
        
        Returns:
            bool: True if build completed successfully, False otherwise
        """
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}🔨 GLOWLIGHT BUILD WORKFLOW{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        try:
            # Step 1: Validate project structure
            if not self._validate_project_structure():
                return False
            
            # Step 2: Check configuration
            if not self._check_configuration():
                return False
            
            # Step 3: Check PlatformIO installation
            if not self._check_platformio():
                return False
            
            # Step 4: Build firmware
            if not self._build_firmware():
                return False
            
            print(f"\n{Colors.GREEN}✅ Build completed successfully!{Colors.RESET}")
            return True
            
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⚠️  Build cancelled by user{Colors.RESET}")
            return False
        except Exception as e:
            self.error_handler.handle_error(e, "build workflow")
            return False
    
    def run_flash_workflow(self) -> bool:
        """
        Run the complete flash workflow.
        
        Returns:
            bool: True if flash completed successfully, False otherwise
        """
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}⚡ GLOWLIGHT FLASH WORKFLOW{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        try:
            # Step 1: Check if firmware is built
            if not self._check_firmware_exists():
                print(f"{Colors.YELLOW}📦 Firmware not found. Building first...{Colors.RESET}\n")
                if not self.run_build_workflow():
                    return False
            
            # Step 2: Detect devices
            devices = self._detect_devices()
            if not devices:
                return False
            
            # Step 3: Select device
            device = self._select_device(devices)
            if not device:
                return False
            
            # Step 4: Flash firmware
            if not self._flash_firmware(device):
                return False
            
            print(f"\n{Colors.GREEN}✅ Flash completed successfully!{Colors.RESET}")
            return True
            
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⚠️  Flash cancelled by user{Colors.RESET}")
            return False
        except Exception as e:
            self.error_handler.handle_error(e, "flash workflow")
            return False
    
    def run_build_flash_workflow(self) -> bool:
        """
        Run the complete build and flash workflow.
        
        Returns:
            bool: True if both build and flash completed successfully, False otherwise
        """
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}🚀 GLOWLIGHT BUILD & FLASH WORKFLOW{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        try:
            # Step 1: Build firmware
            print(f"{Colors.BLUE}🔨 Building firmware...{Colors.RESET}")
            if not self.run_build_workflow():
                return False
            
            print(f"\n{Colors.BLUE}⚡ Flashing firmware...{Colors.RESET}")
            
            # Step 2: Detect devices
            devices = self._detect_devices()
            if not devices:
                return False
            
            # Step 3: Select device
            device = self._select_device(devices)
            if not device:
                return False
            
            # Step 4: Flash firmware
            if not self._flash_firmware(device):
                return False
            
            print(f"\n{Colors.GREEN}✅ Build and flash completed successfully!{Colors.RESET}")
            return True
            
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⚠️  Build and flash cancelled by user{Colors.RESET}")
            return False
        except Exception as e:
            self.error_handler.handle_error(e, "build and flash workflow")
            return False
    
    def _validate_project_structure(self) -> bool:
        """Validate that all required project files exist."""
        print(f"{Colors.BLUE}📋 Validating project structure...{Colors.RESET}")
        
        required_files = [
            "platformio.ini",
            "src/main.cpp",
            "include/GlowConfig.h"
        ]
        
        with ProgressBar(total=len(required_files)) as progress:
            for i, file_path in enumerate(required_files):
                full_path = os.path.join(self.project_root, file_path)
                if not check_file_exists(full_path):
                    print(f"\n{Colors.RED}❌ Missing required file: {file_path}{Colors.RESET}")
                    return False
                progress.update(i + 1, f"Checking {file_path}")
                time.sleep(0.1)
        
        print(f"\n{Colors.GREEN}✅ Project structure validated{Colors.RESET}")
        return True
    
    def _check_configuration(self) -> bool:
        """Check if configuration is valid."""
        print(f"{Colors.BLUE}⚙️  Checking configuration...{Colors.RESET}")
        
        config_path = os.path.join(self.project_root, "include", "GlowConfig.h")
        if not check_file_exists(config_path):
            print(f"{Colors.RED}❌ Configuration file not found: {config_path}{Colors.RESET}")
            print(f"{Colors.YELLOW}💡 Run configuration workflow first{Colors.RESET}")
            return False
        
        print(f"{Colors.GREEN}✅ Configuration validated{Colors.RESET}")
        return True
    
    def _check_platformio(self) -> bool:
        """Check PlatformIO installation."""
        print(f"{Colors.BLUE}🔧 Checking PlatformIO installation...{Colors.RESET}")
        
        if not self.platformio_manager.is_platformio_installed():
            print(f"{Colors.RED}❌ PlatformIO not installed{Colors.RESET}")
            print(f"{Colors.YELLOW}💡 Run setup workflow first to install PlatformIO{Colors.RESET}")
            return False
        
        print(f"{Colors.GREEN}✅ PlatformIO installation verified{Colors.RESET}")
        return True
    
    def _build_firmware(self) -> bool:
        """Build the firmware."""
        print(f"\n{Colors.BLUE}🔨 Building firmware...{Colors.RESET}")
        
        return self.platformio_manager.build_project(self.project_root)
    
    def _check_firmware_exists(self) -> bool:
        """Check if firmware binary exists."""
        firmware_path = os.path.join(self.project_root, ".pio", "build", "esp32c3", "firmware.bin")
        return check_file_exists(firmware_path)
    
    def _detect_devices(self) -> List[Dict[str, Any]]:
        """Detect available ESP32 devices."""
        print(f"\n{Colors.BLUE}🔍 Scanning for ESP32 devices...{Colors.RESET}")
        
        devices = self.device_manager.scan_for_devices()
        
        if not devices:
            print(f"{Colors.RED}❌ No ESP32 devices found{Colors.RESET}")
            print(f"{Colors.YELLOW}💡 Please connect your ESP32 device and try again{Colors.RESET}")
            print(f"   • Check USB cable connection")
            print(f"   • Ensure device is powered on")
            print(f"   • Try a different USB port")
            return []
        
        print(f"{Colors.GREEN}✅ Found {len(devices)} ESP32 device(s){Colors.RESET}")
        return devices
    
    def _select_device(self, devices: List[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
        """Allow user to select a device."""
        if len(devices) == 1:
            device = devices[0]
            print(f"\n{Colors.GREEN}📱 Using device: {device['port']} ({device['description']}){Colors.RESET}")
            return device
        
        print(f"\n{Colors.BLUE}📱 Multiple devices found. Please select one:{Colors.RESET}")
        for i, device in enumerate(devices, 1):
            print(f"  {i}. {device['port']} - {device['description']}")
        
        while True:
            try:
                choice = input(f"\n{Colors.CYAN}Enter device number (1-{len(devices)}): {Colors.RESET}")
                index = int(choice) - 1
                if 0 <= index < len(devices):
                    return devices[index]
                else:
                    print(f"{Colors.RED}❌ Invalid choice. Please enter a number between 1 and {len(devices)}{Colors.RESET}")
            except ValueError:
                print(f"{Colors.RED}❌ Invalid input. Please enter a number{Colors.RESET}")
            except KeyboardInterrupt:
                print(f"\n{Colors.YELLOW}⚠️  Device selection cancelled{Colors.RESET}")
                return None
    
    def _flash_firmware(self, device: Dict[str, Any]) -> bool:
        """Flash firmware to the selected device."""
        print(f"\n{Colors.BLUE}⚡ Flashing firmware to {device['port']}...{Colors.RESET}")
        
        return self.platformio_manager.flash_project(self.project_root, device['port'])
    
    def show_build_menu(self) -> Optional[str]:
        """
        Show build and flash menu options.
        
        Returns:
            str: Selected menu option or None if cancelled
        """
        print(f"\n{Colors.CYAN}{'='*50}")
        print(f"{Colors.BOLD}🔨 BUILD, FLASH & DEVICE MENU{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*50}{Colors.RESET}\n")
        
        options = [
            ("1", "Build firmware only", "build"),
            ("2", "Flash firmware only", "flash"),
            ("3", "Build and flash firmware", "build_flash"),
            ("4", "Build, flash and monitor", "build_flash_monitor"),
            ("5", "Serial monitor", "monitor"),
            ("6", "Scan devices", "scan_devices"),
            ("7", "Reset device", "reset_device"),
            ("8", "View build information", "build_info"),
            ("9", "Clean build files", "clean"),
            ("b", "Back to main menu", "back")
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
    
    def show_build_info(self) -> None:
        """Show build information and status."""
        print(f"\n{Colors.CYAN}{'='*50}")
        print(f"{Colors.BOLD}📊 BUILD INFORMATION{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*50}{Colors.RESET}\n")
        
        # Check firmware existence
        firmware_exists = self._check_firmware_exists()
        firmware_path = os.path.join(self.project_root, ".pio", "build", "esp32c3", "firmware.bin")
        
        print(f"{Colors.BLUE}📁 Project Path:{Colors.RESET} {self.project_root}")
        print(f"{Colors.BLUE}🔧 Target Platform:{Colors.RESET} ESP32-C3")
        print(f"{Colors.BLUE}📦 Firmware Status:{Colors.RESET} ", end="")
        
        if firmware_exists:
            print(f"{Colors.GREEN}✅ Built{Colors.RESET}")
            # Get file size and modification time
            try:
                stat = os.stat(firmware_path)
                size_kb = stat.st_size / 1024
                mod_time = time.ctime(stat.st_mtime)
                print(f"{Colors.BLUE}📏 Firmware Size:{Colors.RESET} {size_kb:.1f} KB")
                print(f"{Colors.BLUE}🕒 Last Built:{Colors.RESET} {mod_time}")
            except OSError:
                pass
        else:
            print(f"{Colors.RED}❌ Not built{Colors.RESET}")
        
        # Check configuration
        config_path = os.path.join(self.project_root, "include", "GlowConfig.h")
        config_exists = check_file_exists(config_path)
        print(f"{Colors.BLUE}⚙️  Configuration:{Colors.RESET} ", end="")
        if config_exists:
            print(f"{Colors.GREEN}✅ Present{Colors.RESET}")
        else:
            print(f"{Colors.RED}❌ Missing{Colors.RESET}")
        
        # Check PlatformIO
        pio_installed = self.platformio_manager.is_platformio_installed()
        print(f"{Colors.BLUE}🔧 PlatformIO:{Colors.RESET} ", end="")
        if pio_installed:
            print(f"{Colors.GREEN}✅ Installed{Colors.RESET}")
        else:
            print(f"{Colors.RED}❌ Not installed{Colors.RESET}")
        
        print()
    
    def clean_build_files(self) -> bool:
        """Clean build files."""
        print(f"\n{Colors.BLUE}🧹 Cleaning build files...{Colors.RESET}")
        
        return self.platformio_manager.clean("esp32c3")
    
    def run_build_flash_monitor_workflow(self) -> bool:
        """
        Run the complete build, flash and monitor workflow.
        
        Returns:
            bool: True if successful, False otherwise
        """
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}🚀📺 GLOWLIGHT BUILD, FLASH & MONITOR WORKFLOW{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        try:
            # Step 1: Build and flash
            if not self.run_build_flash_workflow():
                return False
            
            # Step 2: Start serial monitor
            print(f"\n{Colors.BLUE}📺 Starting serial monitor...{Colors.RESET}")
            print(f"{Colors.YELLOW}💡 Press Ctrl+C to exit monitor{Colors.RESET}")
            
            # Get device that was just flashed
            devices = self._detect_devices()
            if not devices:
                print(f"{Colors.YELLOW}⚠️  No devices found for monitoring{Colors.RESET}")
                return True
            
            device = self._select_device(devices)
            if device:
                self.start_serial_monitor(device)
            
            return True
            
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⚠️  Workflow cancelled by user{Colors.RESET}")
            return False
        except Exception as e:
            self.error_handler.handle_error(e, "build, flash and monitor workflow")
            return False
    
    def start_serial_monitor(self, device: Dict[str, Any] = None) -> bool:
        """
        Start serial monitor for a device.
        
        Args:
            device: Device info dictionary, if None will scan for devices
            
        Returns:
            bool: True if monitor started successfully
        """
        try:
            if device is None:
                # Scan for devices
                devices = self._detect_devices()
                if not devices:
                    return False
                
                device = self._select_device(devices)
                if not device:
                    return False
            
            print(f"\n{Colors.BLUE}📺 Starting serial monitor for {device['port']}...{Colors.RESET}")
            print(f"{Colors.YELLOW}💡 Press Ctrl+C to exit monitor{Colors.RESET}\n")
            
            # Use PlatformIO manager to start monitor
            return self.platformio_manager.monitor_serial(device['port'])
            
        except Exception as e:
            self.error_handler.handle_error(e, "serial monitor")
            return False
    
    def scan_and_show_devices(self) -> bool:
        """
        Scan for devices and show them in a formatted table.
        
        Returns:
            bool: True if scan completed successfully
        """
        try:
            print(f"\n{Colors.BLUE}🔍 Scanning for ESP32 devices...{Colors.RESET}")
            
            devices = self.device_manager.scan_for_devices()
            
            if not devices:
                print(f"{Colors.RED}❌ No ESP32 devices found{Colors.RESET}")
                print(f"{Colors.YELLOW}💡 Make sure your ESP32 is connected via USB{Colors.RESET}")
                return False
            
            print(f"\n{Colors.GREEN}✅ Found {len(devices)} ESP32 device(s){Colors.RESET}")
            print(f"\n{Colors.CYAN}{'='*70}")
            print(f"{'#':<3} {'Port':<15} {'Description':<35} {'ESP32':<6}")
            print(f"{Colors.CYAN}{'='*70}{Colors.RESET}")
            
            for i, device in enumerate(devices):
                esp32_indicator = f"{Colors.GREEN}✅{Colors.RESET}" if device['is_esp32'] else f"{Colors.RED}❌{Colors.RESET}"
                port_color = Colors.GREEN if device['is_esp32'] else Colors.YELLOW
                print(f"{i+1:<3} {port_color}{device['port']:<15}{Colors.RESET} {device['description']:<35} {esp32_indicator:<6}")
            
            print(f"{Colors.CYAN}{'='*70}{Colors.RESET}")
            return True
            
        except Exception as e:
            self.error_handler.handle_error(e, "device scan")
            return False
    
    def reset_selected_device(self) -> bool:
        """
        Allow user to select and reset a device.
        
        Returns:
            bool: True if reset completed successfully
        """
        try:
            # Scan for devices
            devices = self._detect_devices()
            if not devices:
                return False
            
            # Select device
            device = self._select_device(devices)
            if not device:
                return False
            
            print(f"\n{Colors.BLUE}🔄 Resetting device {device['port']}...{Colors.RESET}")
            
            # Reset device using PlatformIO manager
            success = self.platformio_manager.reset_device(device['port'])
            
            if success:
                print(f"{Colors.GREEN}✅ Device reset successfully{Colors.RESET}")
            else:
                print(f"{Colors.RED}❌ Device reset failed{Colors.RESET}")
            
            return success
            
        except Exception as e:
            self.error_handler.handle_error(e, "device reset")
            return False
