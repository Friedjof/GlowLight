"""
Configuration Validator

Validates GPIO pins, configurations, and other setup parameters.
"""

from core.error_handler import ValidationError
from ui.ascii_art_fixed import ASCIIArt


class ConfigValidator:
    """Validates configuration parameters for GlowLight."""
    
    # ESP32-C3 valid GPIO pins
    ESP32C3_VALID_PINS = list(range(0, 22))  # GPIO 0-21
    
    # Pins that should be avoided (boot/flash pins)
    ESP32C3_RESERVED_PINS = [2, 8, 9]  # Boot button, flash pins
    
    def __init__(self):
        """Initialize validator."""
        pass
        
    def validate_pin_number(self, pin):
        """Validate a GPIO pin number for ESP32-C3.
        
        Args:
            pin: Pin number to validate
            
        Returns:
            bool: True if valid
        """
        if not isinstance(pin, int):
            return False
            
        if pin not in self.ESP32C3_VALID_PINS:
            return False
            
        if pin in self.ESP32C3_RESERVED_PINS:
            ASCIIArt.show_warning(f"Pin {pin} is reserved for system use!")
            ASCIIArt.show_info("It may work but could cause issues during boot/flash")
            response = input("⚠️  Use this pin anyway? (y/N): ").strip().lower()
            return response == 'y'
            
        return True
        
    def validate_pins(self, pin_config):
        """Validate complete pin configuration.
        
        Args:
            pin_config: Dictionary of pin assignments
            
        Returns:
            bool: True if all pins are valid
        """
        # Check individual pin validity
        for pin_name, pin_number in pin_config.items():
            if not self.validate_pin_number(pin_number):
                ASCIIArt.show_error(f"Invalid pin for {pin_name}: {pin_number}")
                return False
                
        # Check for pin conflicts
        used_pins = list(pin_config.values())
        duplicates = self._find_duplicates(used_pins)
        
        if duplicates:
            ASCIIArt.show_error("Pin conflict detected!")
            for pin in duplicates:
                conflicting_names = [name for name, num in pin_config.items() if num == pin]
                ASCIIArt.show_error(f"  Pin {pin} used by: {', '.join(conflicting_names)}")
            return False
            
        # Check I2C pin compatibility
        if not self._validate_i2c_pins(
            pin_config.get('DISTANCE_SENSOR_SDA'),
            pin_config.get('DISTANCE_SENSOR_SCL')
        ):
            return False
            
        ASCIIArt.show_success("Pin configuration is valid!")
        return True
        
    def validate_mesh_config(self, mesh_config):
        """Validate mesh network configuration.
        
        Args:
            mesh_config: Dictionary of mesh settings
            
        Returns:
            bool: True if valid
        """
        # Check mesh prefix
        prefix = mesh_config.get('MESH_PREFIX', '').strip('"')
        if len(prefix) < 1 or len(prefix) > 32:
            ASCIIArt.show_error("Mesh prefix must be 1-32 characters")
            return False
            
        # Check mesh password
        password = mesh_config.get('MESH_PASSWORD', '').strip('"')
        if len(password) < 8:
            ASCIIArt.show_error("Mesh password must be at least 8 characters")
            return False
            
        ASCIIArt.show_success("Mesh configuration is valid!")
        return True
        
    def validate_port_path(self, port_path):
        """Validate serial port path.
        
        Args:
            port_path: Path to serial port
            
        Returns:
            bool: True if valid
        """
        import os
        
        if not port_path:
            return False
            
        # Check if port exists
        if not os.path.exists(port_path):
            return False
            
        # Check if it looks like a serial port
        valid_prefixes = ['/dev/ttyUSB', '/dev/ttyACM', '/dev/cu.', 'COM']
        
        return any(port_path.startswith(prefix) for prefix in valid_prefixes)
        
    def _find_duplicates(self, pin_list):
        """Find duplicate pins in a list.
        
        Args:
            pin_list: List of pin numbers
            
        Returns:
            list: List of duplicate pins
        """
        seen = set()
        duplicates = set()
        
        for pin in pin_list:
            if pin in seen:
                duplicates.add(pin)
            seen.add(pin)
            
        return list(duplicates)
        
    def _validate_i2c_pins(self, sda_pin, scl_pin):
        """Validate I2C pin selection.
        
        Args:
            sda_pin: SDA pin number
            scl_pin: SCL pin number
            
        Returns:
            bool: True if valid I2C configuration
        """
        if sda_pin is None or scl_pin is None:
            return True  # Skip if pins not provided
            
        # SDA and SCL must be different
        if sda_pin == scl_pin:
            ASCIIArt.show_error("SDA and SCL pins cannot be the same!")
            return False
            
        # Both should be valid GPIO pins
        if not (self.validate_pin_number(sda_pin) and self.validate_pin_number(scl_pin)):
            return False
            
        # Recommend certain pins for I2C (though any GPIO can work)
        recommended_i2c_pins = [4, 5, 6, 7, 8, 9, 10]
        
        if sda_pin not in recommended_i2c_pins or scl_pin not in recommended_i2c_pins:
            ASCIIArt.show_warning("Consider using pins 4-10 for better I2C performance")
            
        return True


class PlatformIOValidator:
    """Validates PlatformIO installation and environment."""
    
    @staticmethod
    def validate_installation_path(path):
        """Validate PlatformIO installation path.
        
        Args:
            path: Path to check
            
        Returns:
            bool: True if valid installation
        """
        import os
        from pathlib import Path
        
        path_obj = Path(path)
        
        # Check if directory exists
        if not path_obj.exists():
            return False
            
        # Check for expected subdirectories
        expected_dirs = ['penv', 'packages', 'platforms']
        
        for expected_dir in expected_dirs:
            if not (path_obj / expected_dir).exists():
                return False
                
        return True
        
    @staticmethod
    def validate_project_structure():
        """Validate current directory is a valid PlatformIO project.
        
        Returns:
            bool: True if valid project
        """
        import os
        from pathlib import Path
        
        required_files = ['platformio.ini']
        required_dirs = ['src', 'include']
        
        # Check required files
        for required_file in required_files:
            if not Path(required_file).exists():
                return False
                
        # Check required directories
        for required_dir in required_dirs:
            if not Path(required_dir).exists():
                return False
                
        return True