#!/usr/bin/env python3
"""
GlowLight CLI Main Interface

Main command-line interface for the GlowLight setup system.
Provides an interactive menu for configuration, building, and flashing.
"""

import os
import sys
from pathlib import Path

from ui.menu import MenuSystem
from ui.ascii_art_fixed import ASCIIArt, Colors
from core.error_handler import ErrorHandler


class GlowLightCLI:
    """Main CLI interface for GlowLight setup."""
    
    def __init__(self):
        """Initialize the CLI with the menu system."""
        self.menu_system = MenuSystem()
        self.ascii_art = ASCIIArt()
        self.error_handler = ErrorHandler()
        
    def run(self):
        """Main application loop."""
        try:
            self._show_welcome()
            self._run_main_menu()
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⚠️  Setup interrupted by user{Colors.RESET}")
            print(f"{Colors.GREEN}👋 Thank you for using GlowLight Setup Tool!{Colors.RESET}")
        except Exception as e:
            self.error_handler.handle_error(e, "CLI main loop")
            
    def _show_welcome(self):
        """Display welcome screen and project information."""
        # Clear screen and show header
        os.system('clear' if os.name == 'posix' else 'cls')
        
        # Show ASCII art header
        self.ascii_art.show_header()
        
        print(f"\n{Colors.CYAN}{'='*80}")
        print(f"{Colors.BOLD}🌟 Welcome to the GlowLight Setup System!{Colors.RESET}")
        print(f"{Colors.CYAN}   Complete setup from git clone to flashed firmware")
        print(f"{'='*80}{Colors.RESET}\n")
        
        print(f"{Colors.BLUE}📋 This tool will help you:{Colors.RESET}")
        print(f"   • Configure your mesh network and device settings")
        print(f"   • Install and manage PlatformIO dependencies")
        print(f"   • Build and flash firmware to ESP32-C3 devices")
        print(f"   • Monitor device output and debug issues")
        print(f"   • Manage multiple devices in your mesh network")
        
        input(f"\n{Colors.CYAN}Press Enter to continue...{Colors.RESET}")
        ASCIIArt.clear_screen()
        
    def _run_main_menu(self):
        """Run the main menu system."""
        self.menu_system.show_main_menu()


def main():
    """Entry point for the CLI application."""
    cli = GlowLightCLI()
    cli.run()


if __name__ == "__main__":
    main()
