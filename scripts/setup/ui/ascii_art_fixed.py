"""
ASCII Art Module

Provides ASCII art, formatting, and visual elements for the CLI interface.
"""

import os
import sys


class Colors:
    """ANSI color codes for terminal output."""
    # Reset
    RESET = '\033[0m'
    
    # Regular colors
    BLACK = '\033[30m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'
    GRAY = '\033[90m'
    
    # Bold colors
    BOLD = '\033[1m'
    BOLD_RED = '\033[1;31m'
    BOLD_GREEN = '\033[1;32m'
    BOLD_YELLOW = '\033[1;33m'
    BOLD_BLUE = '\033[1;34m'
    BOLD_MAGENTA = '\033[1;35m'
    BOLD_CYAN = '\033[1;36m'
    BOLD_WHITE = '\033[1;37m'
    
    # Background colors
    BG_BLACK = '\033[40m'
    BG_RED = '\033[41m'
    BG_GREEN = '\033[42m'
    BG_YELLOW = '\033[43m'
    BG_BLUE = '\033[44m'
    BG_MAGENTA = '\033[45m'
    BG_CYAN = '\033[46m'
    BG_WHITE = '\033[47m'


class ASCIIArt:
    """Handles ASCII art and visual formatting."""
    
    LOGO = """
╔══════════════════════════════════════════════════════════════════════════════╗
║                                                                              ║
║ ░██████╗░██╗░░░░░░█████╗░░██╗░░░░░░░██╗██╗░░░░░██╗░██████╗░██╗░░██╗████████╗ ║
║ ██╔════╝░██║░░░░░██╔══██╗░██║░░██╗░░██║██║░░░░░██║██╔════╝░██║░░██║╚══██╔══╝ ║
║ ██║░░██╗░██║░░░░░██║░░██║░╚██╗████╗██╔╝██║░░░░░██║██║░░██╗░███████║░░░██║░░░ ║
║ ██║░░╚██╗██║░░░░░██║░░██║░░████╔═████║░██║░░░░░██║██║░░╚██╗██╔══██║░░░██║░░░ ║
║ ╚██████╔╝███████╗╚█████╔╝░░╚██╔╝░╚██╔╝░███████╗██║╚██████╔╝██║░░██║░░░██║░░░ ║
║ ░╚═════╝░╚══════╝░╚════╝░░░░╚═╝░░░╚═╝░░╚══════╝╚═╝░╚═════╝░╚═╝░░╚═╝░░░╚═╝░░░ ║
║                                                                              ║
║                        🌟 Bedside Lamp Setup System 🌟                       ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
"""
    
    PIN_DIAGRAM = """
    ┌───────────────────────────────────────────────────────────────────┐
    │                    📟 ESP32-C3 Pin Configuration                  │
    ├───────────────────────────────────────────────────────────────────┤
    │                                                                   │
    │   Component  │  Pin  │  ESP32C3 Pin  │  Description               │
    │   ─────────────────────────────────────────────────────────────   │
    │   Button     │   1   │     GND       │  Ground connection         │
    │              │   2   │    GPIO {button_pin}     │  Button input              │
    │   ─────────────────────────────────────────────────────────────   │
    │   VL53L0X    │  VCC  │     5V        │  Power supply              │
    │   (Distance) │  GND  │     GND       │  Ground connection         │
    │              │  SDA  │    GPIO {sda_pin}     │  I2C Data line             │
    │              │  SCL  │    GPIO {scl_pin}     │  I2C Clock line            │
    │   ─────────────────────────────────────────────────────────────   │
    │   WS2812B    │  VCC  │     5V        │  Power supply              │
    │   (LED)      │  GND  │     GND       │  Ground connection         │
    │              │  DI   │    GPIO {led_pin}     │  Data input                │
    │                                                                   │
    └───────────────────────────────────────────────────────────────────┘
    """
    
    @staticmethod
    def clear_screen():
        """Clear the terminal screen."""
        os.system('clear' if os.name == 'posix' else 'cls')
        
    @staticmethod
    def show_logo():
        """Display the GlowLight logo."""
        print(ASCIIArt.LOGO)
        
    @staticmethod
    def show_pin_diagram(button_pin=4, sda_pin=6, scl_pin=7, led_pin=3):
        """Display pin configuration diagram with current settings."""
        diagram = ASCIIArt.PIN_DIAGRAM.format(
            button_pin=button_pin,
            sda_pin=sda_pin, 
            scl_pin=scl_pin,
            led_pin=led_pin
        )
        print(diagram)
        
    @staticmethod
    def show_separator(title="", char="=", width=70):
        """Show a formatted separator with optional title."""
        if title:
            title_len = len(title) + 4  # Add spaces around title
            side_chars = (width - title_len) // 2
            print(f"{char * side_chars} {title} {char * side_chars}")
        else:
            print(char * width)
            
    @staticmethod
    def show_success(message):
        """Display success message with formatting."""
        print(f"✅ {message}")
        
    @staticmethod  
    def show_error(message):
        """Display error message with formatting."""
        print(f"❌ {message}")
        
    @staticmethod
    def show_warning(message):
        """Display warning message with formatting."""
        print(f"⚠️  {message}")
        
    @staticmethod
    def show_info(message):
        """Display info message with formatting."""
        print(f"ℹ️  {message}")
        
    @staticmethod
    def show_header():
        """Display the main header with logo and title."""
        ASCIIArt.clear_screen()
        ASCIIArt.show_logo()
        
    @staticmethod
    def show_step(step_num, total_steps, message):
        """Display step progress with formatting."""
        print(f"📋 Step {step_num}/{total_steps}: {message}")