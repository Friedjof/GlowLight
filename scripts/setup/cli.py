"""
GlowLight CLI Interface

Main command-line interface for the GlowLight setup system.
Provides an interactive menu for configuration, building, and flashing.
"""

import os
import sys
from pathlib import Path

from ui.menu import MenuSystem
from ui.ascii_art import ASCIIArt
from ui.progress import ProgressBar
from managers.config_manager import ConfigManager
from managers.platformio_manager import PlatformIOManager
from managers.device_manager import DeviceManager
from managers.git_manager import GitManager
from core.error_handler import ErrorHandler


class GlowLightCLI:
    """Main CLI interface for GlowLight setup."""
    
    def __init__(self):
        """Initialize the CLI with all necessary managers."""
        self.config_manager = ConfigManager()
        self.platformio_manager = PlatformIOManager()
        self.device_manager = DeviceManager()
        self.git_manager = GitManager()
        self.menu = MenuSystem()
        
    def run(self):
        """Main application loop."""
        try:
            self._show_welcome()
            self._initial_setup()
            self._main_menu_loop()
        except Exception as e:
            ErrorHandler.handle_critical_error(e)
            
    def _show_welcome(self):
        """Display welcome screen and project information."""
        ASCIIArt.clear_screen()
        ASCIIArt.show_logo()
        print("\n" + "="*70)
        print("🌟 Welcome to the GlowLight Setup System!")
        print("   Complete setup from git clone to flashed firmware")
        print("="*70)
        
    def _initial_setup(self):
        """Perform initial setup checks and installations."""
        print("\n🔍 Performing initial system checks...")
        
        # Check and setup git ignore
        if self.git_manager.needs_gitignore_update():
            print("📝 Updating .gitignore for PlatformIO...")
            self.git_manager.update_gitignore()
            
        # Check PlatformIO installation
        if not self.platformio_manager.is_installed():
            print("📦 PlatformIO not found. Installing...")
            success = self.platformio_manager.install()
            if not success:
                raise Exception("Failed to install PlatformIO")
            print("✅ PlatformIO installed successfully!")
        else:
            print("✅ PlatformIO found!")
            
    def _main_menu_loop(self):
        """Main interactive menu loop."""
        while True:
            try:
                choice = self._show_main_menu()
                
                if choice == '1':
                    self._handle_configuration()
                elif choice == '2':
                    self._handle_build_flash()
                elif choice == '3':
                    self._handle_device_management()
                elif choice == '4':
                    self._handle_serial_monitor()
                elif choice == '5':
                    break
                else:
                    print("❌ Invalid choice. Please try again.")
                    
            except KeyboardInterrupt:
                print("\n\n🚪 Returning to main menu...")
                continue
            except Exception as e:
                ErrorHandler.handle_error(e)
                input("\nPress Enter to continue...")
                ASCIIArt.clear_screen()
                
    def _show_main_menu(self):
        """Display main menu and get user choice."""
        ASCIIArt.clear_screen()
        ASCIIArt.show_logo()
        
        print("\n" + "="*50)
        print("🏠 MAIN MENU")
        print("="*50)
        print("[1]  Configuration Management")
        print("[2]  Build & Flash Firmware") 
        print("[3]  Device Management")
        print("[4]  Serial Monitor")
        print("[5]  Exit")
        print("="*50)
        
        return input("\n🎯 Select an option (1-5): ").strip()
        
    def _handle_configuration(self):
        """Handle configuration menu."""
        from workflows.configuration import ConfigurationWorkflow
        workflow = ConfigurationWorkflow(self.config_manager)
        workflow.run()
        
    def _handle_build_flash(self):
        """Handle build and flash workflow."""
        from workflows.build_flash import BuildFlashWorkflow
        workflow = BuildFlashWorkflow(
            self.platformio_manager, 
            self.device_manager,
            self.config_manager
        )
        workflow.run()
        
    def _handle_device_management(self):
        """Handle device management."""
        from workflows.device_management import DeviceManagementWorkflow  
        workflow = DeviceManagementWorkflow(self.device_manager)
        workflow.run()
        
    def _handle_serial_monitor(self):
        """Handle serial monitor."""
        from workflows.serial_monitor import SerialMonitorWorkflow
        workflow = SerialMonitorWorkflow(
            self.device_manager,
            self.platformio_manager
        )
        workflow.run()