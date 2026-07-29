"""
Git Manager

Handles git-related operations like .gitignore management.
"""

import os
import re
from pathlib import Path

from core.error_handler import ErrorHandler
from core.validator import ConfigValidator
from ui.ascii_art_fixed import ASCIIArt


class GitManager:
    """Manages git-related operations for the project."""
    
    GITIGNORE_FILE = ".gitignore"
    
    # Lines to add to .gitignore
    GITIGNORE_ADDITIONS = [
        "",
        "# PlatformIO",
        ".platformio/",
        ".pio/",
        "",
        "# GlowLight Configuration", 
        "include/GlowConfig.h",
        "include/backups/",
        ""
    ]
    
    def __init__(self):
        """Initialize git manager."""
        self.gitignore_path = Path(self.GITIGNORE_FILE)
        
    def is_git_repo(self):
        """Check if current directory is a git repository.
        
        Returns:
            bool: True if git repository
        """
        return Path(".git").exists()
        
    def needs_gitignore_update(self):
        """Check if .gitignore needs to be updated.
        
        Returns:
            bool: True if update needed
        """
        if not self.gitignore_path.exists():
            return True
            
        try:
            with open(self.gitignore_path, 'r') as f:
                content = f.read()
                
            # Check if PlatformIO entries already exist
            return ".platformio/" not in content or "include/GlowConfig.h" not in content
            
        except Exception:
            return True
            
    def update_gitignore(self):
        """Update .gitignore with necessary entries.
        
        Returns:
            bool: True if successful
        """
        try:
            # Read existing content
            existing_content = ""
            if self.gitignore_path.exists():
                with open(self.gitignore_path, 'r') as f:
                    existing_content = f.read().rstrip()
                    
            # Check what needs to be added
            additions_needed = []
            for line in self.GITIGNORE_ADDITIONS:
                if line.strip() and line.strip() not in existing_content:
                    additions_needed.extend(self._get_section_for_line(line))
                    
            if not additions_needed:
                ASCIIArt.show_info(".gitignore is already up to date")
                return True
                
            # Add new content
            with open(self.gitignore_path, 'w') as f:
                if existing_content:
                    f.write(existing_content + "\n")
                    
                f.write("\n".join(additions_needed))
                
            ASCIIArt.show_success(".gitignore updated successfully")
            return True
            
        except Exception as e:
            ErrorHandler.handle_error(e, "updating .gitignore")
            return False
            
    def create_gitignore(self):
        """Create a new .gitignore file.
        
        Returns:
            bool: True if successful
        """
        try:
            with open(self.gitignore_path, 'w') as f:
                f.write("# GlowLight Project\n")
                f.write("\n".join(self.GITIGNORE_ADDITIONS))
                
            ASCIIArt.show_success(".gitignore created")
            return True
            
        except Exception as e:
            ErrorHandler.handle_error(e, "creating .gitignore")
            return False
            
    def backup_gitignore(self):
        """Create backup of existing .gitignore.
        
        Returns:
            str or None: Backup file path if successful
        """
        if not self.gitignore_path.exists():
            return None
            
        try:
            backup_path = Path(f"{self.GITIGNORE_FILE}.backup")
            
            # Read existing content
            with open(self.gitignore_path, 'r') as f:
                content = f.read()
                
            # Write backup
            with open(backup_path, 'w') as f:
                f.write(content)
            os.chmod(backup_path, 0o600)
                
            ASCIIArt.show_info(f"Backup created: {backup_path}")
            return str(backup_path)
            
        except Exception as e:
            ErrorHandler.handle_error(e, "backing up .gitignore")
            return None
            
    def show_gitignore_status(self):
        """Show current .gitignore status."""
        ASCIIArt.show_separator("Git Status")
        
        print(f"📁 Repository: {'✅ Yes' if self.is_git_repo() else '❌ Not a git repo'}")
        print(f"📄 .gitignore: {'✅ Exists' if self.gitignore_path.exists() else '❌ Missing'}")
        
        if self.gitignore_path.exists():
            needs_update = self.needs_gitignore_update()
            print(f"🔄 Update needed: {'✅ No' if not needs_update else '⚠️  Yes'}")
            
            if needs_update:
                print("\n📋 Missing entries:")
                self._show_missing_entries()
        else:
            print("📋 Will create new .gitignore with PlatformIO entries")
            
    def _get_section_for_line(self, line):
        """Get the complete section for a gitignore line.
        
        Args:
            line: Line to check
            
        Returns:
            list: Section lines to add
        """
        if line.strip() == ".platformio/":
            return [
                "",
                "# PlatformIO", 
                ".platformio/",
                ".pio/"
            ]
        elif line.strip() == "include/GlowConfig.h":
            return [
                "",
                "# GlowLight Configuration",
                "include/GlowConfig.h", 
                "include/backups/"
            ]
        else:
            return [line]
            
    def _show_missing_entries(self):
        """Show what entries are missing from .gitignore."""
        if not self.gitignore_path.exists():
            return
            
        try:
            with open(self.gitignore_path, 'r') as f:
                content = f.read()
                
            for line in self.GITIGNORE_ADDITIONS:
                if line.strip() and line.strip() not in content:
                    print(f"   - {line}")
                    
        except Exception:
            print("   - Could not read .gitignore")


class ProjectStructureValidator:
    """Validates project structure for common issues."""
    
    @staticmethod
    def validate_directory_structure():
        """Validate that we're in a proper GlowLight project.
        
        Returns:
            bool: True if valid project structure
        """
        required_files = [
            "platformio.ini",
            "src/main.cpp", 
            "include/GlowConfig.h-template"
        ]
        
        required_dirs = [
            "src",
            "include", 
            "lib"
        ]
        
        # Check files
        for file_path in required_files:
            if not Path(file_path).exists():
                ASCIIArt.show_error(f"Missing required file: {file_path}")
                return False
                
        # Check directories
        for dir_path in required_dirs:
            if not Path(dir_path).is_dir():
                ASCIIArt.show_error(f"Missing required directory: {dir_path}")
                return False
                
        return True
        
    @staticmethod
    def check_config_template():
        """Check if configuration template exists and is valid.
        
        Returns:
            bool: True if template is valid
        """
        template_path = Path("include/GlowConfig.h-template")
        
        if not template_path.exists():
            ASCIIArt.show_error("Configuration template not found!")
            ASCIIArt.show_info("Expected: include/GlowConfig.h-template")
            return False
            
        try:
            with open(template_path, 'r') as f:
                content = f.read()
                
            # Check for required defines
            required_defines = [
                "LED_DATA_PIN",
                "BUTTON_PIN", 
                "DISTANCE_SENSOR_SDA",
                "DISTANCE_SENSOR_SCL",
                "WIFI_ON",
                "WIFI_SSID",
                "WIFI_PASSWORD",
                "GLOW_HOSTNAME",
                "MESH_ON",
                "GLOW_SYNC_FOLLOW_DEFAULT",
                "GLOW_SYNC_PUBLISH_DEFAULT",
                "GLOW_PORTAL_ENABLED",
                "GLOW_PORTAL_PASSWORD",
                "GLOW_OTA_ENABLED",
                "GLOW_OTA_PASSWORD",
                "GLOW_MQTT_ENABLED",
                "GLOW_MQTT_HOST",
                "GLOW_MQTT_PORT",
                "GLOW_MQTT_USER",
                "GLOW_MQTT_PASSWORD",
                "GLOW_MQTT_DISCOVERY_PREFIX",
                "ESPNOW_CHANNEL",
                "GLOW_GROUP_KEY_HEX",
                "GLOW_MAX_GROUP_NODES",
                "GLOW_NODE_TIMEOUT",
                "HARTBEAT_INTERVAL",
                "LEVEL_UPDATE_INTERVAL"
            ]
            
            for define in required_defines:
                if f"#define {define}" not in content:
                    ASCIIArt.show_error(f"Template missing: #define {define}")
                    return False

            mesh_match = re.search(
                r'^#define\s+MESH_ON\s+(true|false)\b', content, re.MULTILINE
            )
            channel_match = re.search(
                r'^#define\s+ESPNOW_CHANNEL\s+(\d+)\b', content, re.MULTILINE
            )
            follow_match = re.search(
                r'^#define\s+GLOW_SYNC_FOLLOW_DEFAULT\s+(true|false)\b',
                content,
                re.MULTILINE,
            )
            publish_match = re.search(
                r'^#define\s+GLOW_SYNC_PUBLISH_DEFAULT\s+(true|false)\b',
                content,
                re.MULTILINE,
            )
            max_nodes_match = re.search(
                r'^#define\s+GLOW_MAX_GROUP_NODES\s+(\d+)\b', content, re.MULTILINE
            )
            key_match = re.search(
                r'^#define\s+GLOW_GROUP_KEY_HEX\s+"([^"]*)"\s*$',
                content,
                re.MULTILINE,
            )
            portal_enabled_match = re.search(
                r'^#define\s+GLOW_PORTAL_ENABLED\s+(true|false)\b',
                content,
                re.MULTILINE,
            )
            portal_password_match = re.search(
                r'^#define\s+GLOW_PORTAL_PASSWORD\s+"([^"]*)"\s*$',
                content,
                re.MULTILINE,
            )
            ota_enabled_match = re.search(
                r'^#define\s+GLOW_OTA_ENABLED\s+(true|false)\b',
                content,
                re.MULTILINE,
            )
            ota_password_match = re.search(
                r'^#define\s+GLOW_OTA_PASSWORD\s+"([^\"]*)"\s*$',
                content,
                re.MULTILINE,
            )

            if not mesh_match:
                ASCIIArt.show_error("Template has an invalid MESH_ON value")
                return False
            if not follow_match or not publish_match:
                ASCIIArt.show_error("Template has invalid sync policy defaults")
                return False
            if not channel_match or not 1 <= int(channel_match.group(1)) <= 13:
                ASCIIArt.show_error("Template ESPNOW_CHANNEL must be between 1 and 13")
                return False
            if (not max_nodes_match or
                    int(max_nodes_match.group(1)) != ConfigValidator.MAX_GROUP_NODES):
                ASCIIArt.show_error(
                    f"Template GLOW_MAX_GROUP_NODES must be {ConfigValidator.MAX_GROUP_NODES}"
                )
                return False
            if not key_match:
                ASCIIArt.show_error("Template GLOW_GROUP_KEY_HEX must be a quoted C string")
                return False
            if not portal_enabled_match or not portal_password_match:
                ASCIIArt.show_error("Template has invalid captive portal settings")
                return False
            if portal_enabled_match.group(1) == 'true' and not ConfigValidator().validate_portal_config({
                    'GLOW_PORTAL_ENABLED': True,
                    'GLOW_PORTAL_PASSWORD': portal_password_match.group(1),
            }):
                ASCIIArt.show_error("Template has unsafe captive portal settings")
                return False
            if not ota_enabled_match or not ota_password_match:
                ASCIIArt.show_error("Template has invalid OTA settings")
                return False
            if ota_enabled_match.group(1) == 'true' and not ConfigValidator().validate_ota_config({
                    'GLOW_OTA_ENABLED': True,
                    'GLOW_OTA_PASSWORD': ota_password_match.group(1),
            }):
                ASCIIArt.show_error("Template has unsafe OTA settings")
                return False

            if mesh_match.group(1) == 'true':
                communication_config = {
                    'MESH_ON': True,
                    'GLOW_SYNC_FOLLOW_DEFAULT': follow_match.group(1) == 'true',
                    'GLOW_SYNC_PUBLISH_DEFAULT': publish_match.group(1) == 'true',
                    'ESPNOW_CHANNEL': int(channel_match.group(1)),
                    'GLOW_GROUP_KEY_HEX': key_match.group(1),
                    'GLOW_MAX_GROUP_NODES': int(max_nodes_match.group(1)),
                }
                if not ConfigValidator().validate_communication_config(communication_config):
                    ASCIIArt.show_error("Template has invalid mesh configuration")
                    return False
                    
            return True
            
        except Exception as e:
            ErrorHandler.handle_error(e, "validating config template")
            return False
