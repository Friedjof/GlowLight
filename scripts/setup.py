#!/usr/bin/env python3
"""
GlowLight Setup Script

This script provides a complete setup experience for the GlowLight project.
It handles PlatformIO installation, configuration management, building, and flashing.

Usage: python scripts/setup.py

Requirements: Python 3.6+, no additional packages needed (uses only standard library)
"""

import os
import sys
import subprocess
from pathlib import Path

def main():
    """Main entry point for the GlowLight setup script."""
    try:
        # Get directories
        script_dir = Path(__file__).parent.absolute()
        project_root = script_dir.parent
        setup_dir = script_dir / "setup"
        
        # Change to setup directory for proper imports
        original_cwd = os.getcwd()
        os.chdir(setup_dir)
        
        # Add setup directory to path
        sys.path.insert(0, str(setup_dir))
        
        try:
            # Import after changing directory
            from cli_main import GlowLightCLI
            from core.error_handler import ErrorHandler
            
            # Change back to project root for operations
            os.chdir(project_root)
            
            # Initialize and run the CLI
            cli = GlowLightCLI()
            cli.run()
            
        finally:
            # Restore original directory
            os.chdir(original_cwd)
            
    except KeyboardInterrupt:
        print("\n\n🚪 Setup interrupted by user. Goodbye!")
        sys.exit(0)
    except Exception as e:
        print(f"❌ Critical error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
