#!/usr/bin/env python3
"""
Serial Monitor Workflow - GlowLight Setup System
Handles serial monitoring, logging, and real-time device communication.
"""

import os
import sys
import time
import threading
from datetime import datetime
from typing import Optional, List, Dict, Any, Callable

from core.error_handler import ErrorHandler, SetupError, DeviceError
from managers.device_manager import DeviceManager
from managers.platformio_manager import PlatformIOManager
from ui.progress import ProgressBar
from ui.ascii_art_fixed import Colors, ASCIIArt
from utils.file_utils import ensure_directory_exists


class SerialMonitorWorkflow:
    """Manages serial monitoring and logging functionality."""
    
    def __init__(self):
        self.error_handler = ErrorHandler()
        self.device_manager = DeviceManager()
        self.platformio_manager = PlatformIOManager()
        self.project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))
        self.log_directory = os.path.join(self.project_root, "logs")
        self.is_monitoring = False
        self.monitor_thread = None
    
    def run_serial_monitor(self, device: Optional[Dict[str, Any]] = None) -> bool:
        """
        Start serial monitoring for a device.
        
        Args:
            device: Specific device to monitor, or None to select from available devices
            
        Returns:
            bool: True if monitoring started successfully, False otherwise
        """
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}📺 SERIAL MONITOR{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        try:
            # Select device if not provided
            if device is None:
                device = self._select_device_for_monitoring()
                if device is None:
                    return False
            
            # Show monitor options
            options = self._show_monitor_options()
            if options is None:
                return False
            
            # Start monitoring
            return self._start_monitoring(device, options)
            
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⚠️  Serial monitor cancelled{Colors.RESET}")
            return False
        except Exception as e:
            self.error_handler.handle_error(e, "serial monitor")
            return False
    
    def run_advanced_monitor(self, device: Dict[str, Any]) -> bool:
        """
        Run advanced serial monitor with filtering and logging.
        
        Args:
            device: Device to monitor
            
        Returns:
            bool: True if monitoring completed successfully, False otherwise
        """
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}📊 ADVANCED SERIAL MONITOR{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        try:
            # Configure advanced options
            config = self._configure_advanced_monitoring()
            if config is None:
                return False
            
            # Start advanced monitoring
            return self._start_advanced_monitoring(device, config)
            
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⚠️  Advanced monitoring cancelled{Colors.RESET}")
            return False
        except Exception as e:
            self.error_handler.handle_error(e, "advanced serial monitor")
            return False
    
    def show_logs(self) -> None:
        """Display available log files and allow viewing."""
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}📋 SERIAL MONITOR LOGS{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        try:
            log_files = self._get_log_files()
            
            if not log_files:
                print(f"{Colors.YELLOW}📭 No log files found{Colors.RESET}")
                print(f"{Colors.BLUE}💡 Logs will be created when monitoring with logging enabled{Colors.RESET}")
                return
            
            print(f"{Colors.GREEN}Found {len(log_files)} log file(s):{Colors.RESET}\n")
            
            for i, log_file in enumerate(log_files, 1):
                file_path = os.path.join(self.log_directory, log_file)
                file_size = os.path.getsize(file_path)
                mod_time = datetime.fromtimestamp(os.path.getmtime(file_path))
                
                print(f"{Colors.BLUE}{i}. {log_file}{Colors.RESET}")
                print(f"   📏 Size: {file_size:,} bytes")
                print(f"   🕒 Modified: {mod_time.strftime('%Y-%m-%d %H:%M:%S')}")
                print()
            
            # Allow user to view a log file
            self._view_log_file(log_files)
            
        except Exception as e:
            self.error_handler.handle_error(e, "viewing logs")
    
    def show_monitor_menu(self) -> Optional[str]:
        """
        Show serial monitor menu options.
        
        Returns:
            str: Selected menu option or None if cancelled
        """
        print(f"\n{Colors.CYAN}{'='*50}")
        print(f"{Colors.BOLD}📺 SERIAL MONITOR MENU{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*50}{Colors.RESET}\n")
        
        options = [
            ("1", "📺 Start basic monitor", "monitor"),
            ("2", "📊 Start advanced monitor", "advanced"),
            ("3", "📋 View log files", "logs"),
            ("4", "🧹 Clear log files", "clear_logs"),
            ("5", "⚙️  Monitor settings", "settings"),
            ("6", "ℹ️  Monitor help", "help"),
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
    
    def clear_logs(self) -> bool:
        """Clear all log files."""
        print(f"\n{Colors.BLUE}🧹 Clearing log files...{Colors.RESET}")
        
        try:
            log_files = self._get_log_files()
            
            if not log_files:
                print(f"{Colors.YELLOW}📭 No log files to clear{Colors.RESET}")
                return True
            
            # Confirm deletion
            print(f"{Colors.YELLOW}⚠️  This will delete {len(log_files)} log file(s){Colors.RESET}")
            confirm = input(f"{Colors.CYAN}Continue? (y/N): {Colors.RESET}").strip().lower()
            
            if confirm not in ['y', 'yes']:
                print(f"{Colors.YELLOW}🚫 Operation cancelled{Colors.RESET}")
                return False
            
            # Delete files
            deleted_count = 0
            for log_file in log_files:
                file_path = os.path.join(self.log_directory, log_file)
                try:
                    os.remove(file_path)
                    deleted_count += 1
                except OSError as e:
                    print(f"{Colors.RED}❌ Failed to delete {log_file}: {e}{Colors.RESET}")
            
            print(f"{Colors.GREEN}✅ Deleted {deleted_count} log file(s){Colors.RESET}")
            return True
            
        except Exception as e:
            self.error_handler.handle_error(e, "clearing logs")
            return False
    
    def show_monitor_help(self) -> None:
        """Show help information for serial monitoring."""
        print(f"\n{Colors.CYAN}{'='*60}")
        print(f"{Colors.BOLD}ℹ️  SERIAL MONITOR HELP{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
        
        print(f"{Colors.BLUE}📺 Basic Monitor:{Colors.RESET}")
        print(f"   • Simple real-time serial output display")
        print(f"   • Press Ctrl+C to stop monitoring")
        print(f"   • Baud rate: 115200 (default for GlowLight)")
        print()
        
        print(f"{Colors.BLUE}📊 Advanced Monitor:{Colors.RESET}")
        print(f"   • Includes logging to file")
        print(f"   • Message filtering and highlighting")
        print(f"   • Timestamps for each message")
        print(f"   • Configurable baud rate")
        print()
        
        print(f"{Colors.BLUE}🔧 Monitor Controls:{Colors.RESET}")
        print(f"   • Ctrl+C: Stop monitoring")
        print(f"   • Ctrl+Z: Pause/resume (Linux/macOS)")
        print(f"   • Any key: Resume if paused")
        print()
        
        print(f"{Colors.BLUE}📋 Log Files:{Colors.RESET}")
        print(f"   • Stored in: {self.log_directory}")
        print(f"   • Format: glowlight_YYYYMMDD_HHMMSS.log")
        print(f"   • Include timestamps and device info")
        print()
        
        print(f"{Colors.BLUE}🐛 Debugging Tips:{Colors.RESET}")
        print(f"   • Check that device is not in use by other programs")
        print(f"   • Ensure correct baud rate (115200 for GlowLight)")
        print(f"   • Try resetting the device if no output")
        print(f"   • USB cables can affect serial communication")
    
    def _select_device_for_monitoring(self) -> Optional[Dict[str, Any]]:
        """Select a device for monitoring."""
        devices = self.device_manager.scan_for_devices()
        
        if not devices:
            print(f"{Colors.RED}❌ No ESP32 devices found{Colors.RESET}")
            print(f"{Colors.YELLOW}💡 Please connect your device and try again{Colors.RESET}")
            return None
        
        if len(devices) == 1:
            device = devices[0]
            print(f"{Colors.GREEN}📱 Using device: {device['port']} ({device['description']}){Colors.RESET}")
            return device
        
        print(f"{Colors.BLUE}📱 Multiple devices found. Select one for monitoring:{Colors.RESET}")
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
    
    def _show_monitor_options(self) -> Optional[Dict[str, Any]]:
        """Show monitoring options and get user preferences."""
        print(f"\n{Colors.BLUE}⚙️  Monitor Options:{Colors.RESET}")
        
        options = {}
        
        # Baud rate
        print(f"\n{Colors.BLUE}📡 Baud Rate:{Colors.RESET}")
        print(f"  1. 115200 (default)")
        print(f"  2. 9600")
        print(f"  3. 57600")
        print(f"  4. Custom")
        
        while True:
            try:
                choice = input(f"{Colors.CYAN}Select baud rate (1): {Colors.RESET}").strip()
                if not choice:
                    options['baud_rate'] = 115200
                    break
                elif choice == '1':
                    options['baud_rate'] = 115200
                    break
                elif choice == '2':
                    options['baud_rate'] = 9600
                    break
                elif choice == '3':
                    options['baud_rate'] = 57600
                    break
                elif choice == '4':
                    custom_baud = input(f"{Colors.CYAN}Enter custom baud rate: {Colors.RESET}")
                    options['baud_rate'] = int(custom_baud)
                    break
                else:
                    print(f"{Colors.RED}❌ Invalid choice{Colors.RESET}")
            except ValueError:
                print(f"{Colors.RED}❌ Invalid baud rate{Colors.RESET}")
            except KeyboardInterrupt:
                return None
        
        # Logging
        log_choice = input(f"{Colors.CYAN}Enable logging? (y/N): {Colors.RESET}").strip().lower()
        options['enable_logging'] = log_choice in ['y', 'yes']
        
        if options['enable_logging']:
            ensure_directory_exists(self.log_directory)
        
        return options
    
    def _start_monitoring(self, device: Dict[str, Any], options: Dict[str, Any]) -> bool:
        """Start basic serial monitoring."""
        print(f"\n{Colors.GREEN}📺 Starting monitor for {device['port']} at {options['baud_rate']} baud{Colors.RESET}")
        print(f"{Colors.YELLOW}Press Ctrl+C to stop monitoring{Colors.RESET}\n")
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}")
        
        try:
            if options['enable_logging']:
                log_file = self._create_log_file(device)
                print(f"{Colors.BLUE}📝 Logging to: {log_file}{Colors.RESET}")
            
            # Use PlatformIO monitor
            return self.platformio_manager.monitor_serial(
                device['port'], 
                options['baud_rate']
            )
            
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⚠️  Monitoring stopped{Colors.RESET}")
            return True
        except Exception as e:
            self.error_handler.handle_error(e, "serial monitoring")
            return False
    
    def _configure_advanced_monitoring(self) -> Optional[Dict[str, Any]]:
        """Configure advanced monitoring options."""
        print(f"\n{Colors.BLUE}⚙️  Advanced Monitor Configuration:{Colors.RESET}")
        
        config = {}
        
        # Basic options first
        options = self._show_monitor_options()
        if options is None:
            return None
        config.update(options)
        
        # Advanced options
        config['enable_logging'] = True  # Always enable for advanced mode
        
        # Timestamp format
        print(f"\n{Colors.BLUE}🕒 Timestamp Format:{Colors.RESET}")
        print(f"  1. [HH:MM:SS] (short)")
        print(f"  2. [YYYY-MM-DD HH:MM:SS] (full)")
        print(f"  3. [Relative] (seconds since start)")
        
        ts_choice = input(f"{Colors.CYAN}Select format (1): {Colors.RESET}").strip()
        if ts_choice == '2':
            config['timestamp_format'] = 'full'
        elif ts_choice == '3':
            config['timestamp_format'] = 'relative'
        else:
            config['timestamp_format'] = 'short'
        
        # Filtering
        filter_choice = input(f"{Colors.CYAN}Enable message filtering? (y/N): {Colors.RESET}").strip().lower()
        config['enable_filtering'] = filter_choice in ['y', 'yes']
        
        if config['enable_filtering']:
            config['filters'] = self._configure_filters()
        
        return config
    
    def _configure_filters(self) -> List[str]:
        """Configure message filters."""
        print(f"\n{Colors.BLUE}🔍 Message Filters:{Colors.RESET}")
        print(f"Enter keywords to filter (one per line, empty line to finish):")
        
        filters = []
        while True:
            try:
                filter_term = input(f"{Colors.CYAN}Filter: {Colors.RESET}").strip()
                if not filter_term:
                    break
                filters.append(filter_term)
            except KeyboardInterrupt:
                break
        
        return filters
    
    def _start_advanced_monitoring(self, device: Dict[str, Any], config: Dict[str, Any]) -> bool:
        """Start advanced serial monitoring with filtering and logging."""
        print(f"\n{Colors.GREEN}📊 Starting advanced monitor for {device['port']}{Colors.RESET}")
        print(f"{Colors.YELLOW}Press Ctrl+C to stop monitoring{Colors.RESET}\n")
        
        log_file = self._create_log_file(device)
        print(f"{Colors.BLUE}📝 Logging to: {log_file}{Colors.RESET}")
        
        if config.get('enable_filtering'):
            print(f"{Colors.BLUE}🔍 Filters: {', '.join(config['filters'])}{Colors.RESET}")
        
        print(f"{Colors.CYAN}{'='*60}{Colors.RESET}")
        
        # For advanced monitoring, we would implement custom serial handling
        # For now, fall back to basic monitoring
        return self._start_monitoring(device, config)
    
    def _create_log_file(self, device: Dict[str, Any]) -> str:
        """Create a new log file for the monitoring session."""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        port_name = device['port'].replace('/', '_').replace('\\', '_').replace(':', '_')
        log_filename = f"glowlight_{port_name}_{timestamp}.log"
        log_path = os.path.join(self.log_directory, log_filename)
        
        # Create log file with header
        with open(log_path, 'w') as f:
            f.write(f"GlowLight Serial Monitor Log\n")
            f.write(f"Device: {device['port']} ({device['description']})\n")
            f.write(f"Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"{'='*60}\n\n")
        
        return log_path
    
    def _get_log_files(self) -> List[str]:
        """Get list of available log files."""
        if not os.path.exists(self.log_directory):
            return []
        
        log_files = []
        for filename in os.listdir(self.log_directory):
            if filename.startswith('glowlight_') and filename.endswith('.log'):
                log_files.append(filename)
        
        # Sort by modification time (newest first)
        log_files.sort(key=lambda f: os.path.getmtime(os.path.join(self.log_directory, f)), reverse=True)
        return log_files
    
    def _view_log_file(self, log_files: List[str]) -> None:
        """Allow user to view a log file."""
        while True:
            try:
                choice = input(f"\n{Colors.CYAN}Enter log number to view (or Enter to skip): {Colors.RESET}").strip()
                
                if not choice:
                    break
                
                index = int(choice) - 1
                if 0 <= index < len(log_files):
                    log_path = os.path.join(self.log_directory, log_files[index])
                    self._display_log_file(log_path)
                    break
                else:
                    print(f"{Colors.RED}❌ Invalid log number{Colors.RESET}")
                    
            except ValueError:
                print(f"{Colors.RED}❌ Invalid input{Colors.RESET}")
            except KeyboardInterrupt:
                break
    
    def _display_log_file(self, log_path: str) -> None:
        """Display contents of a log file."""
        try:
            print(f"\n{Colors.CYAN}{'='*60}")
            print(f"{Colors.BOLD}📋 LOG FILE: {os.path.basename(log_path)}{Colors.RESET}")
            print(f"{Colors.CYAN}{'='*60}{Colors.RESET}\n")
            
            with open(log_path, 'r') as f:
                content = f.read()
                print(content)
                
        except Exception as e:
            print(f"{Colors.RED}❌ Error reading log file: {e}{Colors.RESET}")
