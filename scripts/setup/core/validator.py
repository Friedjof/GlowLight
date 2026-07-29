"""
Configuration Validator

Validates GPIO pins, configurations, and other setup parameters.
"""

import hashlib
import re

from core.error_handler import ValidationError
from ui.ascii_art_fixed import ASCIIArt


class ConfigValidator:
    """Validates configuration parameters for GlowLight."""

    GROUP_KEY_HEX_LENGTH = 64
    MAX_GROUP_NODES = 8
    
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
        
    def validate_communication_config(self, communication_config):
        """Validate ESP-NOW communication configuration.
        
        Args:
            communication_config: Dictionary of communication settings
            
        Returns:
            bool: True if valid
        """
        enabled = communication_config.get('MESH_ON')
        if not isinstance(enabled, bool):
            ASCIIArt.show_error("ESP-NOW synchronization must be enabled or disabled")
            return False

        if not enabled:
            return True

        for setting in ('GLOW_SYNC_FOLLOW_DEFAULT', 'GLOW_SYNC_PUBLISH_DEFAULT'):
            if not isinstance(communication_config.get(setting, enabled), bool):
                ASCIIArt.show_error(f"{setting} must be enabled or disabled")
                return False

        channel = communication_config.get('ESPNOW_CHANNEL')
        if not isinstance(channel, int) or not 1 <= channel <= 13:
            ASCIIArt.show_error("ESP-NOW channel must be between 1 and 13")
            return False

        group_key = communication_config.get('GLOW_GROUP_KEY_HEX')
        if self.normalize_group_key(group_key) is None:
            ASCIIArt.show_error(
                "Group key must be exactly 64 hexadecimal characters and cannot be all-zero"
            )
            return False

        max_nodes = communication_config.get('GLOW_MAX_GROUP_NODES')
        if max_nodes != self.MAX_GROUP_NODES:
            ASCIIArt.show_error(f"Maximum group nodes must be {self.MAX_GROUP_NODES}")
            return False

        ASCIIArt.show_success("ESP-NOW configuration is valid!")
        return True

    def validate_portal_config(self, portal_config):
        """Validate the physically activated provisioning access point."""
        enabled = portal_config.get('GLOW_PORTAL_ENABLED')
        if not isinstance(enabled, bool):
            ASCIIArt.show_error("Captive portal must be enabled or disabled")
            return False
        if not enabled:
            return True
        password = portal_config.get('GLOW_PORTAL_PASSWORD')
        if not isinstance(password, str) or not 8 <= len(password) <= 63:
            ASCIIArt.show_error("Portal password must contain 8-63 characters")
            return False
        if password == 'PROVISION_WITH_SETUP':
            ASCIIArt.show_error("Portal password has not been provisioned")
            return False
        return True

    def validate_ota_config(self, ota_config):
        """Validate password-protected infrastructure-WiFi updates."""
        enabled = ota_config.get('GLOW_OTA_ENABLED')
        if not isinstance(enabled, bool):
            ASCIIArt.show_error("OTA must be enabled or disabled")
            return False
        if not enabled:
            return True
        password = ota_config.get('GLOW_OTA_PASSWORD')
        if not isinstance(password, str) or not 12 <= len(password) <= 63:
            ASCIIArt.show_error("OTA password must contain 12-63 characters")
            return False
        if password == 'PROVISION_WITH_SETUP':
            ASCIIArt.show_error("OTA password has not been provisioned")
            return False
        return True

    def validate_wifi_config(self, wifi_config):
        """Validate the infrastructure WiFi connection.

        Mirrors DeviceConfig::validate() in lib/ConfigService, so the setup
        cannot write a configuration the firmware will reject at boot.
        """
        enabled = wifi_config.get('WIFI_ON')
        if not isinstance(enabled, bool):
            ASCIIArt.show_error("Infrastructure WiFi must be enabled or disabled")
            return False

        hostname = wifi_config.get('GLOW_HOSTNAME')
        if not isinstance(hostname, str) or not re.fullmatch(
                r'[A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?', hostname):
            ASCIIArt.show_error(
                "Hostname must be 1-63 letters, digits or dashes and may not "
                "start or end with a dash"
            )
            return False

        if not enabled:
            return True

        ssid = wifi_config.get('WIFI_SSID')
        if not isinstance(ssid, str) or not 1 <= len(ssid) <= 32:
            ASCIIArt.show_error("WiFi SSID must contain 1-32 characters")
            return False

        password = wifi_config.get('WIFI_PASSWORD', '')
        if not isinstance(password, str) or len(password) > 63:
            ASCIIArt.show_error("WiFi password must contain at most 63 characters")
            return False
        if password and len(password) < 8:
            ASCIIArt.show_error(
                "WiFi password must contain at least 8 characters (WPA2 minimum)"
            )
            return False
        return True

    def validate_mqtt_config(self, mqtt_config):
        """Validate the Home Assistant broker connection."""
        enabled = mqtt_config.get('GLOW_MQTT_ENABLED')
        if not isinstance(enabled, bool):
            ASCIIArt.show_error("Home Assistant must be enabled or disabled")
            return False
        if not enabled:
            return True

        host = mqtt_config.get('GLOW_MQTT_HOST')
        if not isinstance(host, str) or not re.fullmatch(r'[A-Za-z0-9.\-:]{1,253}', host):
            ASCIIArt.show_error(
                "Broker host must be a hostname or address without spaces"
            )
            return False

        port = mqtt_config.get('GLOW_MQTT_PORT')
        if not isinstance(port, int) or isinstance(port, bool) or not 1 <= port <= 65535:
            ASCIIArt.show_error("Broker port must be between 1 and 65535")
            return False

        prefix = mqtt_config.get('GLOW_MQTT_DISCOVERY_PREFIX')
        if not isinstance(prefix, str) or not 1 <= len(prefix) <= 48:
            ASCIIArt.show_error("Discovery prefix must contain 1-48 characters")
            return False

        for key in ('GLOW_MQTT_USER', 'GLOW_MQTT_PASSWORD'):
            value = mqtt_config.get(key, '')
            if not isinstance(value, str) or len(value) > 64:
                ASCIIArt.show_error(f"{key} must be a string of at most 64 characters")
                return False
        return True

    @classmethod
    def normalize_group_key(cls, group_key):
        """Return a canonical group key, or None when it is unsafe/invalid."""
        if not isinstance(group_key, str):
            return None

        if not re.fullmatch(rf'[0-9a-fA-F]{{{cls.GROUP_KEY_HEX_LENGTH}}}', group_key):
            return None

        normalized = group_key.lower()
        if normalized == '0' * cls.GROUP_KEY_HEX_LENGTH:
            return None

        return normalized

    @classmethod
    def format_group_key(cls, group_key):
        """Format a key for safe display without exposing the full secret."""
        normalized = cls.normalize_group_key(group_key)
        if normalized is None:
            return "<not provisioned>"

        fingerprint = hashlib.sha256(bytes.fromhex(normalized)).hexdigest()[:12]
        return f"********{normalized[-8:]} (SHA-256 {fingerprint})"
        
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
