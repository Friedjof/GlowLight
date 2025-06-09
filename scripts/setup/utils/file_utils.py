"""
File Utilities

Utility functions for file operations and path handling.
"""

import os
from pathlib import Path


def check_file_exists(file_path):
    """Check if a file exists.
    
    Args:
        file_path: Path to the file to check
        
    Returns:
        bool: True if file exists, False otherwise
    """
    return Path(file_path).exists()


def ensure_directory_exists(directory_path):
    """Ensure a directory exists, creating it if necessary.
    
    Args:
        directory_path: Path to the directory
        
    Returns:
        bool: True if directory exists or was created successfully
    """
    try:
        Path(directory_path).mkdir(parents=True, exist_ok=True)
        return True
    except Exception:
        return False


def get_project_root():
    """Get the project root directory.
    
    Returns:
        Path: Project root directory
    """
    # Start from current file and go up until we find platformio.ini
    current = Path(__file__).parent
    while current != current.parent:
        if (current / "platformio.ini").exists():
            return current
        current = current.parent
    
    # Fallback to going up from scripts directory
    return Path(__file__).parent.parent.parent


def get_config_file_path():
    """Get the path to the GlowConfig.h file.
    
    Returns:
        Path: Path to GlowConfig.h
    """
    return get_project_root() / "include" / "GlowConfig.h"


def get_template_config_path():
    """Get the path to the GlowConfig.h template.
    
    Returns:
        Path: Path to GlowConfig.h-template
    """
    return get_project_root() / "include" / "GlowConfig.h-template"


def is_platformio_project(directory=None):
    """Check if the given directory (or current) is a PlatformIO project.
    
    Args:
        directory: Directory to check (defaults to project root)
        
    Returns:
        bool: True if it's a PlatformIO project
    """
    if directory is None:
        directory = get_project_root()
    
    return (Path(directory) / "platformio.ini").exists()