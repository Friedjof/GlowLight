"""
PlatformIO Manager

Handles PlatformIO installation, project management, building, and flashing.
"""

import os
import subprocess
import sys
from pathlib import Path
import configparser

from core.error_handler import ErrorHandler, PlatformIOError
from core.validator import PlatformIOValidator
from ui.ascii_art_fixed import ASCIIArt, Colors
from ui.progress import ProgressBar, SpinnerProgress


class PlatformIOManager:
    """Manages PlatformIO installation and operations."""
    
    PLATFORMIO_DIR = Path.home() / ".platformio"
    PLATFORMIO_EXECUTABLE = PLATFORMIO_DIR / "penv" / "bin" / "platformio"
    
    def __init__(self):
        """Initialize PlatformIO manager."""
        self.validator = PlatformIOValidator()
        
    def is_installed(self):
        """Check if PlatformIO is installed.
        
        Returns:
            bool: True if PlatformIO is installed
        """
        # Check if executable exists
        if self.PLATFORMIO_EXECUTABLE.exists():
            return True
            
        # Try to find pio in PATH
        try:
            result = subprocess.run(['pio', '--version'], 
                                 capture_output=True, 
                                 text=True, 
                                 timeout=5)
            return result.returncode == 0
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return False
    
    def is_platformio_installed(self):
        """Check if PlatformIO is installed (alias for is_installed).
        
        Returns:
            bool: True if PlatformIO is installed
        """
        return self.is_installed()
            
    def install(self):
        """Install PlatformIO using the official installer script.
        
        Returns:
            bool: True if installation successful
        """
        try:
            # Check if already installed first
            if self.is_installed():
                ASCIIArt.show_success("PlatformIO is already installed!")
                return True
                
            ASCIIArt.show_info("Installing PlatformIO using official installer...")
            ASCIIArt.show_info("This may take several minutes on first install...")
            
            import tempfile
            import urllib.request
            
            # Create temporary directory for installer
            with tempfile.TemporaryDirectory() as temp_dir:
                installer_path = os.path.join(temp_dir, "get-platformio.py")
                
                spinner = SpinnerProgress("Downloading PlatformIO installer...")
                spinner.start()
                
                try:
                    # Download the official installer
                    installer_url = "https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py"
                    urllib.request.urlretrieve(installer_url, installer_path)
                    spinner.stop()
                    ASCIIArt.show_success("Installer downloaded successfully")
                except Exception as e:
                    spinner.stop()
                    ASCIIArt.show_error(f"Failed to download installer: {e}")
                    return False
                
                # Run the installer
                spinner = SpinnerProgress("Installing PlatformIO...")
                spinner.start()
                
                try:
                    result = subprocess.run([sys.executable, installer_path], 
                                          capture_output=True, 
                                          text=True,
                                          timeout=600)  # 10 minute timeout
                    
                    spinner.stop()
                    
                    if result.returncode != 0:
                        ASCIIArt.show_error("PlatformIO installation failed!")
                        print(f"Error output:\n{result.stderr}")
                        if result.stdout:
                            print(f"Output:\n{result.stdout}")
                        return False
                    
                    ASCIIArt.show_success("PlatformIO installation completed!")
                    
                    # Verify installation
                    if self.is_installed():
                        ASCIIArt.show_success("PlatformIO installation verified successfully!")
                        
                        # Show installation info
                        print(f"\n{Colors.BLUE}📁 Installation Details:{Colors.RESET}")
                        print(f"   • PlatformIO installed to: ~/.platformio")
                        print(f"   • Command available as: pio")
                        print(f"   • You may need to restart your terminal or run:")
                        print(f"     export PATH=$PATH:~/.platformio/penv/bin")
                        
                        return True
                    else:
                        ASCIIArt.show_warning("Installation completed but verification failed")
                        ASCIIArt.show_info("You may need to restart your terminal or add PlatformIO to your PATH")
                        return False
                        
                except subprocess.TimeoutExpired:
                    spinner.stop()
                    ASCIIArt.show_error("Installation timed out (10 minutes)")
                    return False
                except Exception as e:
                    spinner.stop()
                    ASCIIArt.show_error(f"Installation failed: {e}")
                    return False
                
        except Exception as e:
            ErrorHandler.handle_error(e, "installing PlatformIO")
            return False
            
    def install_platformio(self):
        """Install PlatformIO (alias for install method).
        
        Returns:
            bool: True if installation successful
        """
        return self.install()
    
    def get_boards(self):
        """Get available boards from platformio.ini.
        
        Returns:
            list: List of available board environments
        """
        try:
            config = configparser.ConfigParser()
            config.read('platformio.ini')
            
            boards = []
            for section in config.sections():
                if section.startswith('env:'):
                    board_name = section[4:]  # Remove 'env:' prefix
                    boards.append(board_name)
                    
            return boards
            
        except Exception as e:
            ErrorHandler.handle_error(e, "reading platformio.ini")
            return []
            
    def build(self, environment=None, show_progress=True):
        """Build the project.
        
        Args:
            environment: Specific environment to build (None for all)
            show_progress: Whether to show progress indication
            
        Returns:
            bool: True if build successful
        """
        try:
            cmd = [self._get_pio_executable(), 'run']
            
            if environment:
                cmd.extend(['--environment', environment])
                
            ASCIIArt.show_info(f"Building project{f' for {environment}' if environment else ''}...")
            
            if show_progress:
                spinner = SpinnerProgress("Compiling firmware...")
                spinner.start()
                
            result = subprocess.run(cmd, 
                                  capture_output=True, 
                                  text=True)
            
            if show_progress:
                spinner.stop()
                
            if result.returncode == 0:
                ASCIIArt.show_success("Build completed successfully!")
                return True
            else:
                ASCIIArt.show_error("Build failed!")
                print(f"Error output:\n{result.stderr}")
                self._suggest_build_fixes(result.stderr)
                return False
                
        except Exception as e:
            ErrorHandler.handle_error(e, "building project")
            return False
            
    def flash(self, environment, port=None, show_progress=True):
        """Flash firmware to device.
        
        Args:
            environment: Environment to flash
            port: Serial port (None for auto-detect)
            show_progress: Whether to show progress
            
        Returns:
            bool: True if flash successful
        """
        try:
            cmd = [
                self._get_pio_executable(), 'run', 
                '--target', 'upload',
                '--environment', environment
            ]
            
            if port:
                cmd.extend(['--upload-port', port])
                
            ASCIIArt.show_info(f"Flashing {environment} to device...")
            
            if show_progress:
                progress = ProgressBar(total=100, width=40)
                
                # Simulate progress (real progress tracking would need platform-specific parsing)
                import threading
                import time
                
                def simulate_progress():
                    for i in range(101):
                        progress.update(i, "Flashing firmware...")
                        time.sleep(0.1)
                        
                progress_thread = threading.Thread(target=simulate_progress)
                progress_thread.daemon = True
                progress_thread.start()
                
            result = subprocess.run(cmd, 
                                  capture_output=True, 
                                  text=True)
            
            if show_progress:
                progress_thread.join()
                progress.finish("Flash complete!")
                
            if result.returncode == 0:
                ASCIIArt.show_success(f"Firmware flashed successfully to {port or 'auto-detected port'}!")
                return True
            else:
                ASCIIArt.show_error("Flash failed!")
                print(f"Error output:\n{result.stderr}")
                self._suggest_flash_fixes(result.stderr)
                return False
                
        except Exception as e:
            ErrorHandler.handle_error(e, "flashing firmware")
            return False
            
    def clean(self, environment=None):
        """Clean build files.
        
        Args:
            environment: Specific environment to clean (None for all)
            
        Returns:
            bool: True if clean successful
        """
        try:
            cmd = [self._get_pio_executable(), 'run', '--target', 'clean']
            
            if environment:
                cmd.extend(['--environment', environment])
                
            ASCIIArt.show_info("Cleaning build files...")
            
            result = subprocess.run(cmd, 
                                  capture_output=True, 
                                  text=True)
            
            if result.returncode == 0:
                ASCIIArt.show_success("Build files cleaned!")
                return True
            else:
                ASCIIArt.show_error("Clean failed!")
                print(f"Error: {result.stderr}")
                return False
                
        except Exception as e:
            ErrorHandler.handle_error(e, "cleaning build files")
            return False
            
    def open_monitor(self, environment, port=None, baud=115200):
        """Open serial monitor.
        
        Args:
            environment: Environment name
            port: Serial port (None for auto-detect)
            baud: Baud rate
            
        Returns:
            bool: True if monitor started
        """
        try:
            cmd = [
                self._get_pio_executable(), 'device', 'monitor',
                '--environment', environment,
                '--baud', str(baud)
            ]
            
            if port:
                cmd.extend(['--port', port])
                
            ASCIIArt.show_info(f"Opening serial monitor on {port or 'auto-detected port'}...")
            ASCIIArt.show_info("Press Ctrl+C to exit monitor")
            
            # Run monitor in foreground (blocking)
            subprocess.run(cmd)
            return True
            
        except KeyboardInterrupt:
            ASCIIArt.show_info("Serial monitor closed")
            return True
        except Exception as e:
            ErrorHandler.handle_error(e, "opening serial monitor")
            return False
            
    def _get_pio_executable(self):
        """Get path to PlatformIO executable.
        
        Returns:
            str: Path to pio executable
        """
        # Try local installation first
        if self.PLATFORMIO_EXECUTABLE.exists():
            return str(self.PLATFORMIO_EXECUTABLE)
            
        # Try global installation
        return 'pio'
        
    def _suggest_build_fixes(self, error_output):
        """Suggest fixes for build errors.
        
        Args:
            error_output: Error output from build
        """
        error_lower = error_output.lower()
        
        if "no such file" in error_lower and "glowconfig.h" in error_lower:
            ASCIIArt.show_info("💡 Missing GlowConfig.h - run configuration first!")
            
        elif "undeclared identifier" in error_lower:
            ASCIIArt.show_info("💡 Check your pin definitions in GlowConfig.h")
            
        elif "library" in error_lower and "not found" in error_lower:
            ASCIIArt.show_info("💡 Try: pio lib install")
            
    def _suggest_flash_fixes(self, error_output):
        """Suggest fixes for flash errors.
        
        Args:
            error_output: Error output from flash
        """
        error_lower = error_output.lower()
        
        if "permission denied" in error_lower:
            ASCIIArt.show_info("💡 Try adding user to dialout group: sudo usermod -a -G dialout $USER")
            
        elif "device not found" in error_lower:
            ASCIIArt.show_info("💡 Check USB connection and try different port")
            
        elif "failed to connect" in error_lower:
            ASCIIArt.show_info("💡 Hold BOOT button while connecting, then release")
            
    def reset_device(self, port: str) -> bool:
        """
        Reset a device using serial DTR/RTS lines or esptool.
        
        Args:
            port: Serial port of the device
            
        Returns:
            bool: True if reset was successful, False otherwise
        """
        try:
            print(f"🔄 Resetting device on {port}...")
            
            # Try using esptool for ESP32 reset first
            try:
                cmd = ["python", "-m", "esptool", "--port", port, "--chip", "esp32", "run"]
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    timeout=10
                )
                
                if result.returncode == 0:
                    print(f"✅ Device reset successfully using esptool")
                    return True
                    
            except (subprocess.TimeoutExpired, FileNotFoundError):
                pass
                
            # Fallback: Try using serial DTR/RTS lines
            try:
                import serial
                import time
                
                with serial.Serial(port, 115200, timeout=1) as ser:
                    # Reset using DTR and RTS lines
                    ser.setDTR(False)
                    ser.setRTS(True)
                    time.sleep(0.1)
                    ser.setRTS(False)
                    time.sleep(0.1)
                    ser.setDTR(True)
                    time.sleep(0.1)
                
                print(f"✅ Device reset successfully using serial lines")
                return True
                
            except Exception as serial_error:
                print(f"❌ Serial reset failed: {serial_error}")
                
            # Final fallback: Try pio device monitor reset command
            try:
                cmd = [self._get_pio_executable(), "device", "monitor", "--port", port, "--echo", "--filter", "send_on_enter", "--eol", "CR"]
                
                # Send Ctrl+C to reset (if monitoring)
                process = subprocess.Popen(
                    cmd,
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True
                )
                
                # Send reset command and close
                process.stdin.write("\x03")  # Ctrl+C
                process.stdin.close()
                
                try:
                    process.wait(timeout=3)
                    print(f"✅ Device reset successfully using monitor")
                    return True
                except subprocess.TimeoutExpired:
                    process.kill()
                    
            except Exception:
                pass
                
            print(f"❌ Device reset failed - all methods exhausted")
            return False
                
        except Exception as e:
            print(f"❌ Device reset error: {e}")
            return False
    
    def show_platformio_info(self):
        """Display comprehensive PlatformIO information and status."""
        ASCIIArt.show_info("Gathering PlatformIO information...")
        
        print("\n" + "="*60)
        print("🔧 PLATFORMIO INFORMATION")
        print("="*60)
        
        # Check installation status
        if self.is_installed():
            print("✅ PlatformIO Status: Installed")
            
            # Get version information
            try:
                result = subprocess.run(['pio', '--version'], 
                                      capture_output=True, text=True, timeout=10)
                if result.returncode == 0:
                    print(f"📦 Version: {result.stdout.strip()}")
                else:
                    print("❌ Could not get version information")
            except Exception:
                print("❌ Could not get version information")
            
            # Show installation path
            if self.PLATFORMIO_EXECUTABLE.exists():
                print(f"📁 Installation Path: {self.PLATFORMIO_DIR}")
            
            # Check core packages
            try:
                result = subprocess.run(['pio', 'platform', 'list', '--installed'], 
                                      capture_output=True, text=True, timeout=15)
                if result.returncode == 0 and result.stdout.strip():
                    print("\n📦 Installed Platforms:")
                    platforms = result.stdout.strip().split('\n')
                    for platform in platforms[:5]:  # Show first 5
                        if platform.strip():
                            print(f"   • {platform.strip()}")
                    if len(platforms) > 5:
                        print(f"   ... and {len(platforms) - 5} more")
                else:
                    print("📦 Installed Platforms: None")
            except Exception:
                print("📦 Installed Platforms: Could not retrieve")
                
        else:
            print("❌ PlatformIO Status: Not installed")
            print("💡 Use the installation option to install PlatformIO")
        
        # Project specific information
        print(f"\n🏗️  Project Information:")
        project_root = Path.cwd()
        platformio_ini = project_root / "platformio.ini"
        
        if platformio_ini.exists():
            print(f"✅ Project Type: PlatformIO project")
            print(f"📁 Project Root: {project_root}")
            
            # Parse platformio.ini for basic info
            try:
                import configparser
                config = configparser.ConfigParser()
                config.read(platformio_ini)
                
                if 'env:esp32-c3-devkitm-1' in config:
                    env = config['env:esp32-c3-devkitm-1']
                    print(f"🎯 Target Board: {env.get('board', 'esp32-c3-devkitm-1')}")
                    print(f"🔧 Framework: {env.get('framework', 'arduino')}")
                    if 'upload_port' in env:
                        print(f"🔌 Upload Port: {env['upload_port']}")
                    if 'monitor_port' in env:
                        print(f"📺 Monitor Port: {env['monitor_port']}")
                        
            except Exception as e:
                print(f"⚠️  Could not parse project configuration: {e}")
        else:
            print(f"❌ Project Type: Not a PlatformIO project")
            print(f"💡 Make sure you're in the GlowLight project directory")
        
        print("\n" + "="*60)
    
    def monitor_serial(self, port, baud_rate=115200):
        """Start serial monitoring using PlatformIO.
        
        Args:
            port: Serial port to monitor
            baud_rate: Baud rate for serial communication
            
        Returns:
            subprocess.Popen: Monitor process or None if failed
        """
        try:
            # Use PlatformIO's built-in serial monitor
            cmd = ['pio', 'device', 'monitor', '--port', port, '--baud', str(baud_rate)]
            
            print(f"🔍 Starting serial monitor on {port} at {baud_rate} baud...")
            print("📝 Press Ctrl+C to stop monitoring\n")
            
            # Start the monitor process
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True
            )
            
            return process
            
        except FileNotFoundError:
            print("❌ PlatformIO not found. Please install PlatformIO first.")
            return None
        except Exception as e:
            print(f"❌ Failed to start serial monitor: {e}")
            return None
    
    def build_project(self, project_path: str, environment: str = "esp32c3") -> bool:
        """
        Build a PlatformIO project in the specified directory.
        
        Args:
            project_path: Path to the project directory
            environment: PlatformIO environment to build (default: esp32c3)
            
        Returns:
            bool: True if build successful
        """
        try:
            # Change to project directory
            original_cwd = os.getcwd()
            os.chdir(project_path)
            
            try:
                return self.build(environment)
            finally:
                # Restore original directory
                os.chdir(original_cwd)
                
        except Exception as e:
            ErrorHandler.handle_error(e, f"building project at {project_path}")
            return False

    def flash_project(self, project_path: str, port: str = None, environment: str = "esp32c3") -> bool:
        """Flash project firmware to device.
        
        Args:
            project_path: Path to the project directory
            port: Serial port for flashing
            environment: PlatformIO environment to flash (default: esp32c3)
            
        Returns:
            bool: True if flash successful
        """
        try:
            # Change to project directory
            original_cwd = os.getcwd()
            os.chdir(project_path)
            
            try:
                return self.flash(environment, port)
            finally:
                # Restore original directory
                os.chdir(original_cwd)
                
        except Exception as e:
            ErrorHandler.handle_error(e, f"flashing project at {project_path}")
            return False
    
    def clean_project(self, project_path: str, environment: str = "esp32c3") -> bool:
        """
        Clean build files for a PlatformIO project.
        
        Args:
            project_path: Path to the project directory
            environment: PlatformIO environment to clean (default: esp32c3)
            
        Returns:
            bool: True if clean successful
        """
        try:
            # Change to project directory
            original_cwd = os.getcwd()
            os.chdir(project_path)
            
            try:
                return self.clean(environment)
            finally:
                # Restore original directory
                os.chdir(original_cwd)
                
        except Exception as e:
            ErrorHandler.handle_error(e, f"cleaning project at {project_path}")
            return False