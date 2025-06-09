"""
Configuration Manager

Handles GlowConfig.h creation, modification, and template management.
"""

import os
import shutil
from pathlib import Path
from datetime import datetime

from core.error_handler import ErrorHandler, ConfigurationError
from core.validator import ConfigValidator
from ui.ascii_art_fixed import ASCIIArt


class ConfigManager:
    """Manages GlowLight configuration files."""
    
    CONFIG_FILE = "include/GlowConfig.h"
    TEMPLATE_FILE = "include/GlowConfig.h-template"
    BACKUP_DIR = "include/backups"
    
    # Default pin configurations
    DEFAULT_PINS = {
        'LED_DATA_PIN': 3,
        'BUTTON_PIN': 4,
        'DISTANCE_SENSOR_SDA': 6,
        'DISTANCE_SENSOR_SCL': 7
    }
    
    def __init__(self):
        """Initialize configuration manager."""
        self.config_path = Path(self.CONFIG_FILE)
        self.template_path = Path(self.TEMPLATE_FILE)
        self.backup_dir = Path(self.BACKUP_DIR)
        self.validator = ConfigValidator()
        
        # Ensure backup directory exists
        self.backup_dir.mkdir(exist_ok=True)
        
    def config_exists(self):
        """Check if configuration file exists."""
        return self.config_path.exists()
        
    def template_exists(self):
        """Check if template file exists."""
        return self.template_path.exists()
        
    def create_from_template(self, overwrite=False):
        """Create config from template.
        
        Args:
            overwrite: Whether to overwrite existing config
            
        Returns:
            bool: Success status
        """
        try:
            if self.config_exists() and not overwrite:
                ASCIIArt.show_warning("Configuration file already exists!")
                response = input("🤔 Do you want to overwrite it? (y/N): ").strip().lower()
                if response != 'y':
                    ASCIIArt.show_info("Configuration creation cancelled.")
                    return False
                    
                # Create backup before overwriting
                self._create_backup()
                
            if not self.template_exists():
                raise ConfigurationError("Template file not found!")
                
            # Copy template to config
            shutil.copy2(self.template_path, self.config_path)
            ASCIIArt.show_success("Configuration file created from template!")
            return True
            
        except Exception as e:
            ErrorHandler.handle_error(e, "creating configuration from template")
            return False
            
    def configure_mesh(self):
        """Configure mesh network settings."""
        ASCIIArt.show_separator("Mesh Network Configuration")
        
        print("\n🌐 The GlowLight supports mesh networking between multiple lamps.")
        print("   This allows lamps to synchronize modes and effects.")
        
        # Ask if mesh should be enabled
        enable_mesh = self._ask_yes_no(
            "\n🔗 Do you want to enable mesh networking?", 
            default=False
        )
        
        mesh_config = {'MESH_ON': enable_mesh}
        
        if enable_mesh:
            print("\n📡 Configuring mesh network parameters...")
            
            # Get mesh prefix (SSID)
            default_prefix = "GlowMesh"
            mesh_prefix = input(f"🏷️  Mesh network name (SSID) [{default_prefix}]: ").strip()
            if not mesh_prefix:
                mesh_prefix = default_prefix
                
            # Get mesh password
            default_password = "GlowMesh"
            mesh_password = input(f"🔐 Mesh network password [{default_password}]: ").strip()
            if not mesh_password:
                mesh_password = default_password
                
            mesh_config.update({
                'MESH_PREFIX': f'"{mesh_prefix}"',
                'MESH_PASSWORD': f'"{mesh_password}"'
            })
            
            ASCIIArt.show_success(f"Mesh configured: {mesh_prefix}")
        else:
            ASCIIArt.show_info("Mesh networking disabled")
            
        return mesh_config
        
    def configure_pins(self):
        """Configure GPIO pin assignments."""
        ASCIIArt.show_separator("GPIO Pin Configuration")
        
        print("\n📟 Configure GPIO pins for your hardware components.")
        print("   Default values are shown in brackets - press Enter to use them.")
        
        # Show current pin diagram
        current_pins = self._get_current_pins()
        ASCIIArt.show_pin_diagram(**current_pins)
        
        new_pins = {}
        
        # Configure LED pin
        led_pin = self._ask_pin(
            "💡 LED Data Pin", 
            self.DEFAULT_PINS['LED_DATA_PIN'],
            "WS2812B LED strip data input"
        )
        new_pins['LED_DATA_PIN'] = led_pin
        
        # Configure button pin  
        button_pin = self._ask_pin(
            "🔘 Button Pin",
            self.DEFAULT_PINS['BUTTON_PIN'], 
            "Push button input (with internal pull-up)"
        )
        new_pins['BUTTON_PIN'] = button_pin
        
        # Configure I2C pins for distance sensor
        sda_pin = self._ask_pin(
            "📏 Distance Sensor SDA Pin",
            self.DEFAULT_PINS['DISTANCE_SENSOR_SDA'],
            "I2C data line for VL53L0X sensor"
        )
        new_pins['DISTANCE_SENSOR_SDA'] = sda_pin
        
        scl_pin = self._ask_pin(
            "📏 Distance Sensor SCL Pin", 
            self.DEFAULT_PINS['DISTANCE_SENSOR_SCL'],
            "I2C clock line for VL53L0X sensor"
        )
        new_pins['DISTANCE_SENSOR_SCL'] = scl_pin
        
        # Validate pin configuration
        if not self.validator.validate_pins(new_pins):
            ASCIIArt.show_error("Pin validation failed!")
            return None
            
        # Show final configuration
        print("\n📋 Final Pin Configuration:")
        ASCIIArt.show_pin_diagram(
            button_pin=button_pin,
            sda_pin=sda_pin,
            scl_pin=scl_pin, 
            led_pin=led_pin
        )
        
        confirm = self._ask_yes_no("\n✅ Accept this pin configuration?", default=True)
        if not confirm:
            ASCIIArt.show_info("Pin configuration cancelled")
            return None
            
        return new_pins
        
    def apply_configuration(self, mesh_config, pin_config):
        """Apply configuration changes to the config file.
        
        Args:
            mesh_config: Mesh network configuration dict
            pin_config: Pin configuration dict
            
        Returns:
            bool: Success status
        """
        try:
            if not self.config_exists():
                ASCIIArt.show_error("Configuration file does not exist!")
                return False
                
            # Read current config
            with open(self.config_path, 'r') as f:
                content = f.read()
                
            # Apply mesh configuration
            for key, value in mesh_config.items():
                content = self._update_define(content, key, value)
                
            # Apply pin configuration  
            for key, value in pin_config.items():
                content = self._update_define(content, key, value)
                
            # Write updated config
            with open(self.config_path, 'w') as f:
                f.write(content)
                
            ASCIIArt.show_success("Configuration applied successfully!")
            return True
            
        except Exception as e:
            ErrorHandler.handle_error(e, "applying configuration")
            return False
            
    def _create_backup(self):
        """Create backup of current configuration."""
        if not self.config_exists():
            return
            
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_name = f"GlowConfig_backup_{timestamp}.h"
        backup_path = self.backup_dir / backup_name
        
        shutil.copy2(self.config_path, backup_path)
        ASCIIArt.show_info(f"Backup created: {backup_path}")
        
    def _get_current_pins(self):
        """Get current pin configuration from config file."""
        pins = self.DEFAULT_PINS.copy()
        
        if self.config_exists():
            try:
                with open(self.config_path, 'r') as f:
                    content = f.read()
                    
                for key in pins.keys():
                    value = self._extract_define_value(content, key)
                    if value is not None:
                        pins[key] = value
                        
            except Exception:
                pass  # Use defaults if reading fails
                
        return {
            'button_pin': pins['BUTTON_PIN'],
            'sda_pin': pins['DISTANCE_SENSOR_SDA'], 
            'scl_pin': pins['DISTANCE_SENSOR_SCL'],
            'led_pin': pins['LED_DATA_PIN']
        }
        
    def _ask_pin(self, prompt, default, description):
        """Ask user for pin number with validation.
        
        Args:
            prompt: Question prompt
            default: Default value
            description: Pin description
            
        Returns:
            int: Selected pin number
        """
        while True:
            print(f"\n{prompt} [{default}]")
            print(f"   └─ {description}")
            
            response = input("   Pin number: ").strip()
            
            if not response:
                return default
                
            try:
                pin = int(response)
                if self.validator.validate_pin_number(pin):
                    return pin
                else:
                    ASCIIArt.show_error("Invalid pin number! ESP32-C3 supports GPIO 0-21")
            except ValueError:
                ASCIIArt.show_error("Please enter a valid number!")
                
    def _ask_yes_no(self, prompt, default=True):
        """Ask yes/no question.
        
        Args:
            prompt: Question prompt
            default: Default answer
            
        Returns:
            bool: User response
        """
        suffix = " (Y/n)" if default else " (y/N)"
        response = input(prompt + suffix + ": ").strip().lower()
        
        if not response:
            return default
            
        return response in ['y', 'yes']
        
    def _update_define(self, content, key, value):
        """Update a #define statement in content.
        
        Args:
            content: File content string
            key: Define key
            value: New value
            
        Returns:
            str: Updated content
        """
        import re
        
        # Handle boolean values
        if isinstance(value, bool):
            value = "true" if value else "false"
            
        pattern = rf'^(#define\s+{key}\s+).*$'
        
        # Use a lambda function to avoid issues with numeric backreferences
        def replacement_func(match):
            return match.group(1) + str(value)
        
        return re.sub(pattern, replacement_func, content, flags=re.MULTILINE)
        
    def _extract_define_value(self, content, key):
        """Extract value from #define statement.
        
        Args:
            content: File content string
            key: Define key
            
        Returns:
            int or None: Extracted value
        """
        import re
        
        pattern = rf'^#define\s+{key}\s+(\d+)'
        match = re.search(pattern, content, re.MULTILINE)
        
        if match:
            try:
                return int(match.group(1))
            except ValueError:
                pass
                
        return None