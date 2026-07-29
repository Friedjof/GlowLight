"""
Configuration Manager

Handles GlowConfig.h creation, modification, and template management.
"""

import getpass
import os
import re
import secrets
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
        self.backup_dir.mkdir(mode=0o700, exist_ok=True)
        os.chmod(self.backup_dir, 0o700)
        
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
            os.chmod(self.config_path, 0o600)
            ASCIIArt.show_success("Configuration file created from template!")
            return True
            
        except Exception as e:
            ErrorHandler.handle_error(e, "creating configuration from template")
            return False
            
    def configure_esp_now(self):
        """Configure ESP-NOW synchronization settings."""
        ASCIIArt.show_separator("ESP-NOW Configuration")
        
        print("\n🌐 GlowLight uses ESP-NOW to synchronize multiple lamps.")
        print("   This allows lamps to synchronize modes and effects.")
        
        enabled = self._ask_yes_no(
            "\n🔗 Do you want to enable wireless synchronization?",
            default=True
        )
        
        communication_config = {
            'MESH_ON': enabled,
            'GLOW_SYNC_FOLLOW_DEFAULT': enabled,
            'GLOW_SYNC_PUBLISH_DEFAULT': enabled,
        }
        
        if enabled:
            while True:
                response = input("📡 ESP-NOW WiFi channel (1-13) [1]: ").strip()

                try:
                    communication_config['ESPNOW_CHANNEL'] = int(response or 1)
                except ValueError:
                    ASCIIArt.show_error("Please enter a number between 1 and 13")
                    continue

                group_key = self._provision_group_key()
                communication_config['GLOW_GROUP_KEY_HEX'] = group_key
                communication_config['GLOW_MAX_GROUP_NODES'] = self.validator.MAX_GROUP_NODES

                if self.validator.validate_communication_config(communication_config):
                    ASCIIArt.show_info(
                        "Group key: " + self.validator.format_group_key(group_key)
                    )
                    break
        else:
            ASCIIArt.show_info("Wireless synchronization disabled")
            
        return communication_config

    def configure_portal(self):
        """Configure the physically activated captive portal."""
        ASCIIArt.show_separator("Captive Portal Configuration")
        enabled = self._ask_yes_no(
            "\n🌐 Enable the setup portal when the button is held during boot?",
            default=False,
        )
        portal_config = {'GLOW_PORTAL_ENABLED': enabled}
        if enabled:
            password = secrets.token_urlsafe(12)
            portal_config['GLOW_PORTAL_PASSWORD'] = password
            if not self.validator.validate_portal_config(portal_config):
                return None
            ASCIIArt.show_warning(
                "Portal password (shown once; store it securely): " + password
            )
        return portal_config

    def configure_ota(self):
        """Configure password-protected firmware updates over WiFi."""
        ASCIIArt.show_separator("OTA Configuration")
        enabled = self._ask_yes_no(
            "\n⬆️  Enable firmware updates over infrastructure WiFi?",
            default=False,
        )
        ota_config = {'GLOW_OTA_ENABLED': enabled}
        if enabled:
            password = secrets.token_urlsafe(16)
            ota_config['GLOW_OTA_PASSWORD'] = password
            if not self.validator.validate_ota_config(ota_config):
                return None
            ASCIIArt.show_warning(
                "OTA username: glowlight\n"
                "OTA password (shown once; store it securely): " + password
            )
        return ota_config

    def _current_define_is_true(self, key):
        """Read a boolean define from the configuration currently on disk.

        _extract_define_value only understands numbers, so the boolean is read
        directly here.
        """
        try:
            if not self.config_path.exists():
                return False
            content = self.config_path.read_text()
        except (OSError, AttributeError):
            return False
        match = re.search(rf'^#define\s+{key}\s+(true|false)\b', content, re.MULTILINE)
        return match is not None and match.group(1) == 'true'

    def configure_wifi(self):
        """Configure the infrastructure WiFi connection."""
        ASCIIArt.show_separator("WiFi Configuration")
        print("\n📶 Infrastructure WiFi is needed for Home Assistant and OTA updates.")
        print("   All lamps of a group must join the same access point, because")
        print("   ESP-NOW and WiFi share one radio channel.")

        enabled = self._ask_yes_no(
            "\n📶 Connect this lamp to your WiFi?",
            default=False,
        )
        while True:
            wifi_config = {'WIFI_ON': enabled}
            if enabled:
                wifi_config['WIFI_SSID'] = input("📡 WiFi SSID: ").strip()
                wifi_config['WIFI_PASSWORD'] = getpass.getpass(
                    "🔑 WiFi password (empty for an open network): "
                )
            wifi_config['GLOW_HOSTNAME'] = input(
                "🏷️  Hostname [glowlight]: "
            ).strip() or "glowlight"

            if self.validator.validate_wifi_config(wifi_config):
                return wifi_config

    def configure_mqtt(self, wifi_enabled=None):
        """Configure the Home Assistant connection over an MQTT broker.

        Args:
            wifi_enabled: Whether infrastructure WiFi is on. Home Assistant
                cannot work without it, and a configuration that enables both
                MQTT and no WiFi is rejected by the firmware as a whole.
        """
        ASCIIArt.show_separator("Home Assistant Configuration")

        if wifi_enabled is None:
            wifi_enabled = self._current_define_is_true('WIFI_ON')
        if not wifi_enabled:
            ASCIIArt.show_warning(
                "Home Assistant needs infrastructure WiFi, which is switched off.\n"
                "Enable WiFi first; leaving Home Assistant disabled for now."
            )
            return {'GLOW_MQTT_ENABLED': False}

        enabled = self._ask_yes_no(
            "\n🏠 Publish this lamp to Home Assistant over MQTT?",
            default=False,
        )
        mqtt_config = {'GLOW_MQTT_ENABLED': enabled}
        if not enabled:
            return mqtt_config

        while True:
            mqtt_config['GLOW_MQTT_HOST'] = input(
                "🖧  Broker host (for example mqtt.local): "
            ).strip()
            port = input("🔌 Broker port [1883]: ").strip() or "1883"
            mqtt_config['GLOW_MQTT_PORT'] = int(port) if port.isdigit() else 0
            mqtt_config['GLOW_MQTT_USER'] = input(
                "👤 Broker username (empty for anonymous): "
            ).strip()
            mqtt_config['GLOW_MQTT_PASSWORD'] = getpass.getpass(
                "🔑 Broker password (empty for none): "
            )
            mqtt_config['GLOW_MQTT_DISCOVERY_PREFIX'] = input(
                "🔎 Discovery prefix [homeassistant]: "
            ).strip() or "homeassistant"

            if self.validator.validate_mqtt_config(mqtt_config):
                return mqtt_config

    def _provision_group_key(self):
        """Create a group key or securely collect one for an existing group."""
        while True:
            action = input(
                "🔐 Create a new group or join an existing group? [C/j]: "
            ).strip().lower()

            if action in ('', 'c', 'create'):
                return secrets.token_hex(32)

            if action in ('j', 'join'):
                entered_key = getpass.getpass(
                    "🔑 Existing group key (exactly 64 hex characters): "
                )
                normalized = self.validator.normalize_group_key(entered_key)
                if normalized is not None:
                    return normalized
                ASCIIArt.show_error(
                    "Invalid group key. Enter exactly 64 hexadecimal characters; "
                    "all-zero and default keys are not allowed."
                )
                continue

            ASCIIArt.show_error("Please enter 'c' to create or 'j' to join")
        
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
        
    def apply_configuration(self, communication_config, pin_config):
        """Apply configuration changes to the config file.
        
        Args:
            communication_config: ESP-NOW communication configuration dict
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
                
            # Apply communication configuration
            for key, value in communication_config.items():
                content = self._update_define(content, key, value)
                
            # Apply pin configuration  
            for key, value in pin_config.items():
                content = self._update_define(content, key, value)
                
            # Write updated config
            with open(self.config_path, 'w') as f:
                f.write(content)
            os.chmod(self.config_path, 0o600)
                
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
        os.chmod(backup_path, 0o600)
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
        
    # Minimum length for the defines that hold a password. Everything else that
    # is a string only has to survive quoting.
    PASSWORD_MINIMUMS = {
        'GLOW_PORTAL_PASSWORD': 8,
        'GLOW_OTA_PASSWORD': 12,
    }

    # Defines the setup may add to a configuration that predates them. A test
    # keeps this in sync with include/GlowConfig.h-template.
    KNOWN_DEFINES = frozenset({
        'MESH_ON', 'ESPNOW_CHANNEL', 'GLOW_GROUP_KEY_HEX', 'GLOW_MAX_GROUP_NODES',
        'GLOW_SYNC_FOLLOW_DEFAULT', 'GLOW_SYNC_PUBLISH_DEFAULT',
        'WIFI_ON', 'WIFI_SSID', 'WIFI_PASSWORD', 'GLOW_HOSTNAME',
        'GLOW_PORTAL_ENABLED', 'GLOW_PORTAL_PASSWORD',
        'GLOW_OTA_ENABLED', 'GLOW_OTA_PASSWORD',
        'GLOW_MQTT_ENABLED', 'GLOW_MQTT_HOST', 'GLOW_MQTT_PORT',
        'GLOW_MQTT_USER', 'GLOW_MQTT_PASSWORD', 'GLOW_MQTT_DISCOVERY_PREFIX',
    })

    def _render_define_value(self, key, value):
        """Render a Python value as a C preprocessor token.

        The decision follows the type of the value, not the name of the define:
        a string is always quoted. Deciding by name meant every new string
        setting had to be remembered here, and the MQTT broker host went into
        the header bare until it broke the build.
        """
        if isinstance(value, bool):
            return "true" if value else "false"

        if key == 'GLOW_GROUP_KEY_HEX':
            normalized = self.validator.normalize_group_key(value)
            if normalized is None:
                raise ConfigurationError("Refusing to write an invalid group key")
            return f'"{normalized}"'

        if isinstance(value, int):
            return str(value)

        if not isinstance(value, str):
            raise ConfigurationError(
                f"Refusing to write {key}: unsupported value type "
                f"{type(value).__name__}"
            )

        if any(character in value for character in '"\\\n\r'):
            raise ConfigurationError(
                f"Refusing to write {key}: value contains characters that "
                "cannot be quoted"
            )

        minimum = self.PASSWORD_MINIMUMS.get(key)
        if minimum is not None and not minimum <= len(value) <= 63:
            raise ConfigurationError(f"Refusing to write an invalid {key}")

        return f'"{value}"'

    def _update_define(self, content, key, value):
        """Update a #define statement in content.

        Args:
            content: File content string
            key: Define key
            value: New value
            
        Returns:
            str: Updated content
        """
        rendered = self._render_define_value(key, value)

        pattern = rf'^(#define\s+{key}\s+).*$'

        # Use a lambda function to avoid issues with numeric backreferences
        def replacement_func(match):
            return match.group(1) + rendered

        updated_content, replacements = re.subn(
            pattern, replacement_func, content, flags=re.MULTILINE
        )
        if replacements == 0 and key in self.KNOWN_DEFINES:
            separator = '' if updated_content.endswith('\n') else '\n'
            return f"{updated_content}{separator}#define {key} {rendered}\n"
        return updated_content

    # Every define holding a secret. Anything listed here is blanked before a
    # configuration is printed or shown from a backup.
    SECRET_DEFINES = (
        'WIFI_PASSWORD',
        'GLOW_PORTAL_PASSWORD',
        'GLOW_OTA_PASSWORD',
        'GLOW_MQTT_PASSWORD',
    )

    def redact_config_content(self, content):
        """Redact the group key and every password before displaying a config."""
        pattern = r'^(#define\s+GLOW_GROUP_KEY_HEX\s+)"?([^"\s]+)"?.*$'

        def replacement(match):
            safe_value = self.validator.format_group_key(match.group(2))
            return match.group(1) + f'"{safe_value}"'

        redacted = re.sub(pattern, replacement, content, flags=re.MULTILINE)
        for define in self.SECRET_DEFINES:
            redacted = re.sub(
                rf'^(#define\s+{define}\s+)"?[^"]*"?.*$',
                r'\1"<redacted>"',
                redacted,
                flags=re.MULTILINE,
            )
        return redacted
        
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
