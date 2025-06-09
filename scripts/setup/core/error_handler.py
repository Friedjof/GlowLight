"""
Error Handler Module

Provides comprehensive error handling with helpful user messages and recovery suggestions.
"""

import sys
import traceback
from pathlib import Path


class ErrorHandler:
    """Centralized error handling with user-friendly messages."""
    
    GITHUB_ISSUES_URL = "https://github.com/friedjof/GlowLight/issues"
    
    @staticmethod
    def handle_error(error, context=""):
        """Handle non-critical errors with recovery suggestions."""
        print(f"\n❌ Error: {str(error)}")
        
        if context:
            print(f"📍 Context: {context}")
            
        # Provide specific solutions for common errors
        ErrorHandler._provide_solutions(error)
        
    @staticmethod
    def handle_critical_error(error):
        """Handle critical errors that require script termination."""
        print(f"\n💥 Critical Error: {str(error)}")
        print("\n📋 Error Details:")
        print("-" * 40)
        traceback.print_exc()
        print("-" * 40)
        
        print(f"\n🐛 If this error persists, please report it at:")
        print(f"   {ErrorHandler.GITHUB_ISSUES_URL}")
        print("\n   Include the error details above in your report.")
        
    @staticmethod
    def _provide_solutions(error):
        """Provide specific solutions based on error type."""
        error_str = str(error).lower()
        
        if "permission denied" in error_str:
            print("\n💡 Solution suggestions:")
            print("   • Run with sudo privileges if needed")
            print("   • Check file permissions")
            print("   • Ensure ESP32 device is not in use by another program")
            
        elif "no such file or directory" in error_str:
            print("\n💡 Solution suggestions:")
            print("   • Verify the file path exists")
            print("   • Check if PlatformIO is properly installed")
            print("   • Ensure you're in the correct project directory")
            
        elif "device not found" in error_str or "serial" in error_str:
            print("\n💡 Solution suggestions:")
            print("   • Check if ESP32 is connected via USB")
            print("   • Try a different USB cable or port")
            print("   • Check if device drivers are installed")
            print("   • Disconnect and reconnect the device")
            
        elif "platformio" in error_str:
            print("\n💡 Solution suggestions:")
            print("   • Reinstall PlatformIO using the setup script")
            print("   • Check your Python installation")
            print("   • Verify internet connection for package downloads")
            
        elif "compilation" in error_str or "build" in error_str:
            print("\n💡 Solution suggestions:")
            print("   • Check your GlowConfig.h configuration")
            print("   • Verify all pin assignments are valid")
            print("   • Try cleaning the build cache")
            
        else:
            print(f"\n🐛 Unknown error. Please report at: {ErrorHandler.GITHUB_ISSUES_URL}")


class ValidationError(Exception):
    """Custom exception for validation errors."""
    pass


class PlatformIOError(Exception):
    """Custom exception for PlatformIO-related errors."""
    pass


class DeviceError(Exception):
    """Custom exception for device-related errors."""
    pass


class ConfigurationError(Exception):
    """Custom exception for configuration-related errors."""
    pass


class SetupError(Exception):
    """Custom exception for general setup-related errors."""
    pass