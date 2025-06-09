"""
Git Manager

Handles git-related operations like .gitignore management.
"""

import os
from pathlib import Path

from core.error_handler import ErrorHandler
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
                "MESH_ON",
                "MESH_PREFIX"
            ]
            
            for define in required_defines:
                if f"#define {define}" not in content:
                    ASCIIArt.show_error(f"Template missing: #define {define}")
                    return False
                    
            return True
            
        except Exception as e:
            ErrorHandler.handle_error(e, "validating config template")
            return False