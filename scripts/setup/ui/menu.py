#!/usr/bin/env python3
"""
Menu System - GlowLight Setup System
Provides the main menu interface and navigation for all workflows.
"""

import os
import sys
from typing import Optional, Dict, Any, Callable

from core.error_handler import ErrorHandler
from managers.config_manager import ConfigManager
from managers.platformio_manager import PlatformIOManager
from ui.ascii_art_fixed import Colors, ASCIIArt
from workflows.configuration import ConfigurationWorkflow
from workflows.build_flash import BuildFlashWorkflow
from workflows.device_management import DeviceManagementWorkflow
from workflows.serial_monitor import SerialMonitorWorkflow
from managers.platformio_manager import PlatformIOManager
from utils.system_utils import get_system_info


class MenuSystem:
    """Main menu system for the GlowLight setup tool."""
    
    def __init__(self):
        self.error_handler = ErrorHandler()
        self.ascii_art = ASCIIArt()
        
        # Initialize managers
        self.config_manager = ConfigManager()
        self.platformio_manager = PlatformIOManager()
        
        # Initialize workflows
        self.config_workflow = ConfigurationWorkflow(self.config_manager)
        self.build_flash_workflow = BuildFlashWorkflow()
        self.device_workflow = DeviceManagementWorkflow()
        self.monitor_workflow = SerialMonitorWorkflow()
        
        self.project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
    
    def show_main_menu(self) -> bool:
        """
        Display the main menu and handle user selection.
        
        Returns:
            bool: False when user wants to exit, True to continue
        """
        while True:
            try:
                self._display_main_menu()
                choice = self._get_user_choice()
                
                if not self._handle_main_menu_choice(choice):
                    return False  # Exit requested
                    
            except KeyboardInterrupt:
                print(f"\n{Colors.YELLOW}⚠️  Interrupted by user{Colors.RESET}")
                if self._confirm_exit():
                    return False
            except Exception as e:
                self.error_handler.handle_error(e, "main menu")
                input(f"\n{Colors.CYAN}Press Enter to continue...{Colors.RESET}")
                ASCIIArt.clear_screen()
    
    def _display_main_menu(self) -> None:
        """Display the main menu interface."""
        # Clear screen and show header
        os.system('clear' if os.name == 'posix' else 'cls')
        
        # Show ASCII art header
        self.ascii_art.show_header()
        
        # Show system info
        self._show_system_status()
        
        # Show menu options
        print(f"\n{Colors.CYAN}╔{'═'*60}╗{Colors.RESET}")
        print(f"{Colors.CYAN}║{Colors.RESET} {Colors.BOLD}🚀 GLOWLIGHT SETUP MAIN MENU  {Colors.RESET}                             {Colors.CYAN}║{Colors.RESET}")
        print(f"{Colors.CYAN}╠{'═'*60}╣{Colors.RESET}")
        
        menu_options = [
            ("1", "⚙️", "Project Configuration"),
            ("2", "🔨", "Build, Flash & Devices"),
            ("3", "📱", "Device Management"),
            ("4", "📺", "Serial Monitor"),
            ("5", "🔧", "PlatformIO Setup"),
            ("6", "ℹ️", "System Information"),
            ("7", "🛠️", "Troubleshooting"),
            ("8", "❓", "Help & Documentation"),
            ("q", "🚪", "Exit")
        ]
        
        for key, _, title in menu_options:
            print(f"{Colors.CYAN}║{Colors.RESET} {Colors.BLUE}[{key}]{Colors.RESET} {title:<54} {Colors.CYAN}║{Colors.RESET}")
        
        print(f"{Colors.CYAN}╚{'═'*60}╝{Colors.RESET}")
    
    def _show_system_status(self) -> None:
        """Show current system status as a formatted table."""
        # Check all status items
        pio_status = "✅ Installed" if self.platformio_manager.is_platformio_installed() else "❌ Not installed"
        
        config_exists = os.path.exists(os.path.join(self.project_root, "include", "GlowConfig.h"))
        config_status = "✅ Configured" if config_exists else "❌ Not configured"
        
        devices = self.device_workflow.device_manager.scan_for_devices()
        device_status = f"✅ {len(devices)} device(s)" if devices else "❌ No devices"
        
        # Check firmware with correct path
        firmware_exists = os.path.exists(os.path.join(self.project_root, ".pio", "build", "esp32c3", "firmware.bin"))
        firmware_status = "✅ Built" if firmware_exists else "❌ Not built"
        
        # Display status table
        print(f"\n{Colors.CYAN}┌{'─'*60}┐{Colors.RESET}")
        print(f"{Colors.CYAN}│{Colors.RESET} {Colors.BOLD}📊 System Status{Colors.RESET} {' '*41} {Colors.CYAN}│{Colors.RESET}")
        print(f"{Colors.CYAN}├{'─'*60}┤{Colors.RESET}")
        print(f"{Colors.CYAN}│{Colors.RESET} 🔧 PlatformIO      {pio_status:<38} {Colors.CYAN}│{Colors.RESET}")
        print(f"{Colors.CYAN}│{Colors.RESET} ⚙️  Configuration   {config_status:<38} {Colors.CYAN}│{Colors.RESET}")
        print(f"{Colors.CYAN}│{Colors.RESET} 📱 ESP32 Devices   {device_status:<38} {Colors.CYAN}│{Colors.RESET}")
        print(f"{Colors.CYAN}│{Colors.RESET} 📦 Firmware        {firmware_status:<38} {Colors.CYAN}│{Colors.RESET}")
        print(f"{Colors.CYAN}└{'─'*60}┘{Colors.RESET}")
    
    def _get_user_choice(self) -> str:
        """Get user menu choice."""
        return input(f"\n{Colors.CYAN}Select option: {Colors.RESET}").strip().lower()
    
    def _handle_main_menu_choice(self, choice: str) -> bool:
        """
        Handle main menu choice.
        
        Args:
            choice: User's menu choice
            
        Returns:
            bool: False to exit, True to continue
        """
        handlers = {
            '1': self._handle_configuration,
            '2': self._handle_build_flash,
            '3': self._handle_device_management,
            '4': self._handle_serial_monitor,
            '5': self._handle_platformio_setup,
            '6': self._handle_system_info,
            '7': self._handle_troubleshooting,
            '8': self._handle_help,
            'q': self._handle_exit,
            'quit': self._handle_exit,
            'exit': self._handle_exit
        }
        
        handler = handlers.get(choice)
        if handler:
            return handler()
        else:
            print(f"{Colors.RED}❌ Invalid option. Please try again.{Colors.RESET}")
            input(f"{Colors.CYAN}Press Enter to continue...{Colors.RESET}")
            ASCIIArt.clear_screen()
            return True
    
    def _handle_configuration(self) -> bool:
        """Handle configuration workflow."""
        self.config_workflow.run_configuration_workflow()
        self._pause_for_user()
        return True
    
    def _handle_build_flash(self) -> bool:
        """Handle build and flash workflow with integrated device management."""
        while True:
            action = self.build_flash_workflow.show_build_menu()
            if action is None or action == 'back':
                break
            
            if action == 'build':
                self.build_flash_workflow.run_build_workflow()
            elif action == 'flash':
                self.build_flash_workflow.run_flash_workflow()
            elif action == 'build_flash':
                self.build_flash_workflow.run_build_flash_workflow()
            elif action == 'build_flash_monitor':
                self.build_flash_workflow.run_build_flash_monitor_workflow()
            elif action == 'monitor':
                self.build_flash_workflow.start_serial_monitor()
            elif action == 'scan_devices':
                self.build_flash_workflow.scan_and_show_devices()
            elif action == 'reset_device':
                self.build_flash_workflow.reset_selected_device()
            elif action == 'build_info':
                self.build_flash_workflow.show_build_info()
            elif action == 'clean':
                self.build_flash_workflow.clean_build_files()
            
            self._pause_for_user()
        
        return True
    
    def _handle_device_management(self) -> bool:
        """Handle device management workflow."""
        while True:
            action = self.device_workflow.show_device_menu()
            if action is None or action == 'back':
                break
            
            if action == 'scan':
                devices = self.device_workflow.run_device_scan()
            elif action == 'info':
                devices = self.device_workflow.run_device_scan()
                if devices:
                    device = self._select_device(devices)
                    if device:
                        self.device_workflow.show_device_info(device)
            elif action == 'test':
                devices = self.device_workflow.run_device_scan()
                if devices:
                    device = self._select_device(devices)
                    if device:
                        self.device_workflow.test_device_connection(device)
            elif action == 'monitor':
                devices = self.device_workflow.run_device_scan()
                if devices:
                    device = self._select_device(devices)
                    if device:
                        self.device_workflow.monitor_device(device)
            elif action == 'reset':
                devices = self.device_workflow.run_device_scan()
                if devices:
                    device = self._select_device(devices)
                    if device:
                        self.device_workflow.reset_device(device)
            elif action == 'list_ports':
                self.device_workflow.list_all_ports()
            
            self._pause_for_user()
        
        return True
    
    def _handle_serial_monitor(self) -> bool:
        """Handle serial monitor workflow."""
        while True:
            action = self.monitor_workflow.show_monitor_menu()
            if action is None or action == 'back':
                break
            
            if action == 'monitor':
                self.monitor_workflow.run_serial_monitor()
            elif action == 'advanced':
                devices = self.device_workflow.run_device_scan()
                if devices:
                    device = self._select_device(devices)
                    if device:
                        self.monitor_workflow.run_advanced_monitor(device)
            elif action == 'logs':
                self.monitor_workflow.show_logs()
            elif action == 'clear_logs':
                self.monitor_workflow.clear_logs()
            elif action == 'settings':
                self._show_monitor_settings()
            elif action == 'help':
                self.monitor_workflow.show_monitor_help()
            
            self._pause_for_user()
        
        return True
    
    def _handle_platformio_setup(self) -> bool:
        """Handle PlatformIO setup."""
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}🔧 PLATFORMIO SETUP{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        if self.platformio_manager.is_platformio_installed():
            print(f"{Colors.GREEN}✅ PlatformIO is already installed{Colors.RESET}")
            
            # Show PlatformIO info
            self.platformio_manager.show_platformio_info()
            
            # Offer to reinstall or update
            choice = input(f"\n{Colors.CYAN}Reinstall PlatformIO? (y/N): {Colors.RESET}").strip().lower()
            if choice in ['y', 'yes']:
                self.platformio_manager.install_platformio()
        else:
            print(f"{Colors.YELLOW}📦 PlatformIO not found. Installing...{Colors.RESET}")
            self.platformio_manager.install_platformio()
        
        self._pause_for_user()
        return True
    
    def _handle_system_info(self) -> bool:
        """Show detailed system information."""
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}ℹ️  SYSTEM INFORMATION{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        # System info
        system_info = get_system_info()
        print(f"{Colors.BLUE}🖥️  System:{Colors.RESET}")
        print(f"   Platform: {system_info['platform']}")
        print(f"   Architecture: {system_info['architecture']}")
        print(f"   Python: {system_info['python_version']}")
        print()
        
        # Project info
        print(f"{Colors.BLUE}📁 Project:{Colors.RESET}")
        print(f"   Root: {self.project_root}")
        print(f"   Setup Script: {__file__}")
        print()
        
        # Show detailed status
        self.build_flash_workflow.show_build_info()
        
        self._pause_for_user()
        return True
    
    def _handle_troubleshooting(self) -> bool:
        """Show troubleshooting information."""
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}🛠️  TROUBLESHOOTING GUIDE{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        print(f"{Colors.BLUE}🔧 Common Issues:{Colors.RESET}")
        print(f"  1. {Colors.YELLOW}Device not detected:{Colors.RESET}")
        print(f"     • Check USB cable and connection")
        print(f"     • Ensure device drivers are installed")
        print(f"     • Try a different USB port")
        print(f"     • Check device is not in use by other software")
        print()
        
        print(f"  2. {Colors.YELLOW}Build failures:{Colors.RESET}")
        print(f"     • Check PlatformIO is installed correctly")
        print(f"     • Ensure internet connection for library downloads")
        print(f"     • Try cleaning build files and rebuilding")
        print(f"     • Check configuration file syntax")
        print()
        
        print(f"  3. {Colors.YELLOW}Flash failures:{Colors.RESET}")
        print(f"     • Put device in download mode (hold BOOT button)")
        print(f"     • Check device is not running other firmware")
        print(f"     • Try a lower flash speed")
        print(f"     • Ensure sufficient power supply")
        print()
        
        print(f"  4. {Colors.YELLOW}Serial monitor issues:{Colors.RESET}")
        print(f"     • Check baud rate (115200 for GlowLight)")
        print(f"     • Ensure no other programs are using the port")
        print(f"     • Try resetting the device")
        print(f"     • Check cable supports data transfer")
        print()
        
        print(f"{Colors.BLUE}🔗 Getting Help:{Colors.RESET}")
        print(f"   • Check the project README.md")
        print(f"   • Review PlatformIO documentation")
        print(f"   • Search for similar issues online")
        print(f"   • Ask for help on project forums")
        
        self._pause_for_user()
        return True
    
    def _handle_help(self) -> bool:
        """Show help and documentation."""
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}❓ HELP & DOCUMENTATION{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        print(f"{Colors.BLUE}📚 GlowLight Setup Tool:{Colors.RESET}")
        print(f"   This tool helps you configure, build, and flash the GlowLight")
        print(f"   mesh networking firmware for ESP32-C3 devices.")
        print()
        
        print(f"{Colors.BLUE}🚀 Quick Start:{Colors.RESET}")
        print(f"   1. Run 'Project Configuration' to set up your mesh network")
        print(f"   2. Connect your ESP32-C3 device via USB")
        print(f"   3. Use 'Build & Flash' to compile and upload firmware")
        print(f"   4. Monitor device output with 'Serial Monitor'")
        print()
        
        print(f"{Colors.BLUE}⚙️  Configuration:{Colors.RESET}")
        print(f"   • Mesh network settings (SSID, password, channel)")
        print(f"   • GPIO pin assignments for LEDs and controls")
        print(f"   • Device-specific parameters")
        print()
        
        print(f"{Colors.BLUE}🔨 Building:{Colors.RESET}")
        print(f"   • Automatic dependency management")
        print(f"   • ESP32-C3 optimized compilation")
        print(f"   • Error checking and validation")
        print()
        
        print(f"{Colors.BLUE}📱 Device Support:{Colors.RESET}")
        print(f"   • ESP32-C3 (primary target)")
        print(f"   • Automatic device detection")
        print(f"   • USB serial communication")
        print()
        
        print(f"{Colors.BLUE}📁 Project Structure:{Colors.RESET}")
        print(f"   • src/main.cpp - Main firmware code")
        print(f"   • include/GlowConfig.h - Configuration file")
        print(f"   • platformio.ini - Build configuration")
        print(f"   • scripts/setup/ - This setup tool")
        
        self._pause_for_user()
        return True
    
    def _handle_exit(self) -> bool:
        """Handle exit request."""
        if self._confirm_exit():
            print(f"\n{Colors.GREEN}👋 Thank you for using GlowLight Setup Tool!{Colors.RESET}")
            return False
        return True
    
    def _confirm_exit(self) -> bool:
        """Confirm exit with user."""
        try:
            choice = input(f"\n{Colors.CYAN}Are you sure you want to exit? (y/N): {Colors.RESET}").strip().lower()
            return choice in ['y', 'yes']
        except KeyboardInterrupt:
            return True
    
    def _select_device(self, devices) -> Optional[Dict[str, Any]]:
        """Helper to select a device from a list."""
        if len(devices) == 1:
            return devices[0]
        
        print(f"\n{Colors.BLUE}📱 Select device:{Colors.RESET}")
        for i, device in enumerate(devices, 1):
            print(f"  {i}. {device['port']} - {device['description']}")
        
        while True:
            try:
                choice = input(f"\n{Colors.CYAN}Enter device number (1-{len(devices)}): {Colors.RESET}")
                index = int(choice) - 1
                if 0 <= index < len(devices):
                    return devices[index]
                else:
                    print(f"{Colors.RED}❌ Invalid choice{Colors.RESET}")
            except ValueError:
                print(f"{Colors.RED}❌ Invalid input{Colors.RESET}")
            except KeyboardInterrupt:
                return None
    
    def _show_monitor_settings(self) -> None:
        """Show serial monitor settings."""
        print(f"\n{Colors.BLUE}⚙️  Serial Monitor Settings:{Colors.RESET}")
        print(f"   • Default baud rate: 115200")
        print(f"   • Log directory: {self.monitor_workflow.log_directory}")
        print(f"   • Timestamp format: configurable")
        print(f"   • Message filtering: available in advanced mode")
    
    def _pause_for_user(self) -> None:
        """Pause and wait for user input."""
        try:
            input(f"\n{Colors.CYAN}Press Enter to continue...{Colors.RESET}")
            ASCIIArt.clear_screen()
        except KeyboardInterrupt:
            pass
