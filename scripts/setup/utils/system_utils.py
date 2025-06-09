"""
System Utilities

System information and platform detection utilities.
"""

import platform
import subprocess
import sys
import os


def get_system_info():
    """Get comprehensive system information.
    
    Returns:
        dict: System information including OS, Python version, etc.
    """
    return {
        'os': platform.system(),
        'os_version': platform.version(),
        'platform': platform.platform(),
        'architecture': platform.architecture()[0],
        'python_version': sys.version,
        'python_executable': sys.executable,
        'working_directory': os.getcwd()
    }


def is_windows():
    """Check if running on Windows.
    
    Returns:
        bool: True if Windows
    """
    return platform.system().lower() == 'windows'


def is_linux():
    """Check if running on Linux.
    
    Returns:
        bool: True if Linux
    """
    return platform.system().lower() == 'linux'


def is_macos():
    """Check if running on macOS.
    
    Returns:
        bool: True if macOS
    """
    return platform.system().lower() == 'darwin'


def get_python_version():
    """Get Python version information.
    
    Returns:
        tuple: (major, minor, micro) version numbers
    """
    return sys.version_info[:3]


def check_python_version(min_version=(3, 6)):
    """Check if Python version meets minimum requirements.
    
    Args:
        min_version: Minimum required version tuple
        
    Returns:
        bool: True if version is sufficient
    """
    return sys.version_info[:2] >= min_version


def run_command(command, capture_output=True, timeout=30):
    """Run a system command safely.
    
    Args:
        command: Command to run (string or list)
        capture_output: Whether to capture output
        timeout: Command timeout in seconds
        
    Returns:
        subprocess.CompletedProcess: Command result
    """
    try:
        if isinstance(command, str):
            command = command.split()
            
        result = subprocess.run(
            command,
            capture_output=capture_output,
            text=True,
            timeout=timeout,
            check=False
        )
        return result
    except subprocess.TimeoutExpired:
        raise Exception(f"Command timed out after {timeout} seconds: {' '.join(command)}")
    except Exception as e:
        raise Exception(f"Failed to run command: {e}")


def which(program):
    """Check if a program is available in PATH.
    
    Args:
        program: Program name to check
        
    Returns:
        str: Path to program if found, None otherwise
    """
    try:
        result = subprocess.run(['which', program], capture_output=True, text=True)
        if result.returncode == 0:
            return result.stdout.strip()
    except:
        pass
    return None