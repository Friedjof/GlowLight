"""
Configuration Workflow

Handles the complete configuration process for GlowLight.
"""

from pathlib import Path
from datetime import datetime

from ui.ascii_art_fixed import ASCIIArt, Colors
from core.error_handler import ErrorHandler


class ConfigurationWorkflow:
    """Manages the configuration workflow."""
    
    def __init__(self, config_manager):
        """Initialize configuration workflow.
        
        Args:
            config_manager: ConfigManager instance
        """
        self.config_manager = config_manager
        
    def run(self):
        """Run the complete configuration workflow."""
        try:
            ASCIIArt.clear_screen()
            ASCIIArt.show_logo()
            ASCIIArt.show_separator("Configuration Management")
            
            # Show current configuration status
            self._show_config_status()
            
            # Configuration menu
            while True:
                choice = self._show_config_menu()
                
                if choice == '1':
                    self._create_new_config()
                elif choice == '2':
                    self._modify_existing_config()
                elif choice == '3':
                    self._view_current_config()
                elif choice == '4':
                    self._backup_config()
                elif choice == '5':
                    break
                else:
                    ASCIIArt.show_error("Invalid choice. Please try again.")
                    
                input("\nPress Enter to continue...")
                ASCIIArt.clear_screen()
                
        except Exception as e:
            ErrorHandler.handle_error(e, "configuration workflow")
    
    def run_configuration_workflow(self):
        """Run the configuration workflow (alias for run).
        
        Provides the interface expected by the menu system.
        """
        return self.run()
            
    def _show_config_status(self):
        """Show current configuration status."""
        config_exists = self.config_manager.config_exists()
        template_exists = self.config_manager.template_exists()
        
        # Configuration Status Box
        print("\n" + "─" * 60)
        print("┌" + "─" * 58 + "┐")
        print("│" + " " * 17 + "📋 Configuration Status" + " " * 18 + "│")
        print("├" + "─" * 58 + "┤")
        print(f"│  📄 GlowConfig.h:     {'✅ Exists' if config_exists else '❌ Missing':<33} │")
        print(f"│  📝 Template:         {'✅ Available' if template_exists else '❌ Missing':<33} │")
        
        if config_exists:
            print("├" + "─" * 58 + "┤")
            # Show current configuration summary in the box
            self._show_current_config_summary_inline()
        
        print("└" + "─" * 58 + "┘")
            
    def _show_current_config_summary_inline(self):
        """Show summary of current configuration inline in the status box."""
        try:
            pins = self.config_manager._get_current_pins()
            
            print("│" + " " * 15 + "📟 Current Pin Configuration" + " " * 15 + "│")
            print("│" + " " * 58 + "│")
            print(f"│     LED Pin:          GPIO {pins['led_pin']:<29} │")
            print(f"│     Button Pin:       GPIO {pins['button_pin']:<29} │")
            print(f"│     SDA Pin:          GPIO {pins['sda_pin']:<29} │")
            print(f"│     SCL Pin:          GPIO {pins['scl_pin']:<29} │")
            
        except Exception:
            print("│" + " " * 7 + "(Could not read current configuration)" + " " * 12 + "│")
            
    def _show_current_config_summary(self):
        """Show summary of current configuration."""
        try:
            pins = self.config_manager._get_current_pins()
            
            print(f"\n📟 Current Pin Configuration:")
            print(f"   LED Pin: GPIO {pins['led_pin']}")
            print(f"   Button Pin: GPIO {pins['button_pin']}")
            print(f"   SDA Pin: GPIO {pins['sda_pin']}")
            print(f"   SCL Pin: GPIO {pins['scl_pin']}")
            
        except Exception:
            print("   (Could not read current configuration)")
            
    def _show_config_menu(self):
        """Show configuration menu and get choice."""
        print("\n")
        print("╔" + "═" * 58 + "╗")
        print("║" + " " * 18 + "🔧 CONFIGURATION MENU" + " " * 19 + "║")
        print("╠" + "═" * 58 + "╣")
        print("║                                                          ║")
        print("║  [1]  Create New Configuration                           ║")
        print("║  [2]  Modify Existing Configuration                      ║")
        print("║  [3]  View Current Configuration                         ║")
        print("║  [4]  Backup Configuration                               ║")
        print("║  [5]  Return to Main Menu                                ║")
        print("║                                                          ║")
        print("╚" + "═" * 58 + "╝")
        
        return input("\n🎯 Select an option (1-5): ").strip()
        
    def _create_new_config(self):
        """Create a new configuration from scratch."""
        ASCIIArt.clear_screen()
        ASCIIArt.show_separator("Create New Configuration")
        
        print("\n🔧 This will create a complete new configuration for your GlowLight.")
        print("   You'll configure mesh networking and GPIO pins.")
        
        if not self.config_manager.template_exists():
            ASCIIArt.show_error("Configuration template not found!")
            ASCIIArt.show_info("Make sure include/GlowConfig.h-template exists")
            return
            
        # Create config from template
        if not self.config_manager.create_from_template():
            return
            
        # Configure mesh networking
        print("\n" + "🌐" + " "*20 + "STEP 1: Mesh Configuration" + " "*20)
        mesh_config = self.config_manager.configure_mesh()
        
        if mesh_config is None:
            ASCIIArt.show_error("Mesh configuration cancelled")
            return
            
        # Configure GPIO pins
        print("\n" + "📟" + " "*20 + "STEP 2: GPIO Pin Configuration" + " "*18)
        pin_config = self.config_manager.configure_pins()
        
        if pin_config is None:
            ASCIIArt.show_error("Pin configuration cancelled")
            return
            
        # Apply configuration
        print("\n" + "💾" + " "*20 + "STEP 3: Applying Configuration" + " "*19)
        if self.config_manager.apply_configuration(mesh_config, pin_config):
            ASCIIArt.show_success("🎉 Configuration created successfully!")
            print("\n📋 Your GlowLight is now configured and ready to build!")
        else:
            ASCIIArt.show_error("Failed to apply configuration")
            
    def _modify_existing_config(self):
        """Modify existing configuration."""
        ASCIIArt.clear_screen()
        ASCIIArt.show_separator("Modify Configuration")
        
        if not self.config_manager.config_exists():
            ASCIIArt.show_error("No configuration file found!")
            ASCIIArt.show_info("Create a new configuration first")
            return
            
        print("\n🔧 Modify specific parts of your configuration:")
        
        while True:
            print("\n" + "="*40)
            print("🔧 MODIFICATION MENU")
            print("="*40)
            print("[1]  Modify Mesh Settings")
            print("[2]  Modify GPIO Pins")
            print("[3]  Complete Reconfiguration")
            print("[4]  Return to Configuration Menu")
            print("="*40)
            
            choice = input("\n🎯 Select option (1-4): ").strip()
            
            if choice == '1':
                mesh_config = self.config_manager.configure_mesh()
                if mesh_config:
                    self.config_manager.apply_configuration(mesh_config, {})
                    
            elif choice == '2':
                pin_config = self.config_manager.configure_pins()
                if pin_config:
                    self.config_manager.apply_configuration({}, pin_config)
                    
            elif choice == '3':
                self._create_new_config()
                break
                
            elif choice == '4':
                break
                
            else:
                ASCIIArt.show_error("Invalid choice")
                
    def _view_current_config(self):
        """View current configuration in detail."""
        ASCIIArt.clear_screen()
        ASCIIArt.show_separator("Current Configuration")
        
        if not self.config_manager.config_exists():
            ASCIIArt.show_error("No configuration file found!")
            return
            
        try:
            # Read and display configuration
            config_path = Path(self.config_manager.CONFIG_FILE)
            
            with open(config_path, 'r') as f:
                content = f.read()
                
            # Extract key values
            self._display_config_values(content)
            
            # Show pin diagram
            pins = self.config_manager._get_current_pins()
            ASCIIArt.show_pin_diagram(**pins)
            
            # Ask if user wants to see full file
            view_full = input("\n🔍 View complete configuration file? (y/N): ").strip().lower()
            
            if view_full == 'y':
                print("\n" + "="*60)
                print("📄 Complete GlowConfig.h:")
                print("="*60)
                print(content)
                print("="*60)
                
        except Exception as e:
            ErrorHandler.handle_error(e, "viewing configuration")
            
    def _display_config_values(self, content):
        """Display key configuration values."""
        import re
        
        print("\n📋 Key Configuration Values:")
        print("-" * 40)
        
        # Define important settings to show
        important_settings = [
            ('LED_DATA_PIN', 'LED Data Pin'),
            ('LED_NUM_LEDS', 'Number of LEDs'),
            ('BUTTON_PIN', 'Button Pin'),
            ('DISTANCE_SENSOR_SDA', 'Distance Sensor SDA'),
            ('DISTANCE_SENSOR_SCL', 'Distance Sensor SCL'),
            ('MESH_ON', 'Mesh Networking'),
            ('MESH_PREFIX', 'Mesh Network Name'),
        ]
        
        for define_name, display_name in important_settings:
            pattern = rf'^#define\s+{define_name}\s+(.+)$'
            match = re.search(pattern, content, re.MULTILINE)
            
            if match:
                value = match.group(1).strip()
                print(f"  {display_name}: {value}")
            else:
                print(f"  {display_name}: (not found)")
                
    def _backup_config(self):
        """Create backup of current configuration."""
        ASCIIArt.clear_screen()
        ASCIIArt.show_separator("Backup Configuration")
        
        if not self.config_manager.config_exists():
            ASCIIArt.show_error("No configuration file found to backup!")
            return
            
        while True:
            print(f"\n{Colors.CYAN}╔{'═'*58}╗{Colors.RESET}")
            print(f"{Colors.CYAN}║{Colors.RESET} {Colors.BOLD}💾 BACKUP CONFIGURATION MENU{Colors.RESET}                             {Colors.CYAN}║{Colors.RESET}")
            print(f"{Colors.CYAN}╠{'═'*58}╣{Colors.RESET}")
            print(f"{Colors.CYAN}║{Colors.RESET} [1] Create New Backup                                    {Colors.CYAN}║{Colors.RESET}")
            print(f"{Colors.CYAN}║{Colors.RESET} [2] View & Restore Backups                               {Colors.CYAN}║{Colors.RESET}")
            print(f"{Colors.CYAN}║{Colors.RESET} [3] List All Backups                                     {Colors.CYAN}║{Colors.RESET}")
            print(f"{Colors.CYAN}║{Colors.RESET} [b] Back to Configuration Menu                           {Colors.CYAN}║{Colors.RESET}")
            print(f"{Colors.CYAN}╚{'═'*58}╝{Colors.RESET}")
            
            choice = input(f"\n{Colors.CYAN}Select option: {Colors.RESET}").strip().lower()
            
            if choice == '1':
                self._create_new_backup()
            elif choice == '2':
                self._view_and_restore_backups()
            elif choice == '3':
                self._list_all_backups()
            elif choice in ['b', 'back']:
                break
            else:
                print(f"{Colors.RED}❌ Invalid option. Please try again.{Colors.RESET}")
                input(f"{Colors.CYAN}Press Enter to continue...{Colors.RESET}")
                ASCIIArt.clear_screen()
                
    def _create_new_backup(self):
        """Create a new backup of the current configuration."""
        print("\n💾 Creating backup of current configuration...")
        
        # Create backup
        self.config_manager._create_backup()
        
        ASCIIArt.show_success("Configuration backup created!")
        print(f"\n📁 Backups are stored in: {self.config_manager.backup_dir}")
        
        input(f"\n{Colors.CYAN}Press Enter to continue...{Colors.RESET}")
        ASCIIArt.clear_screen()
        
    def _view_and_restore_backups(self):
        """View and restore from available backups."""
        # Get the 10 most recent backups
        backup_files = list(self.config_manager.backup_dir.glob("*.h"))
        
        if not backup_files:
            ASCIIArt.show_error("No backup files found!")
            input(f"\n{Colors.CYAN}Press Enter to continue...{Colors.RESET}")
            ASCIIArt.clear_screen()
            return
            
        # Sort by modification time (newest first) and take first 10
        backup_files.sort(key=lambda x: x.stat().st_mtime, reverse=True)
        recent_backups = backup_files[:10]
        
        while True:
            print(f"\n{Colors.CYAN}╔{'═'*58}╗{Colors.RESET}")
            print(f"{Colors.CYAN}║{Colors.RESET} {Colors.BOLD}📋 VIEW & RESTORE BACKUPS{Colors.RESET}                                {Colors.CYAN}║{Colors.RESET}")
            print(f"{Colors.CYAN}╠{'═'*58}╣{Colors.RESET}")
            
            # List recent backups with numbers
            for i, backup_file in enumerate(recent_backups, 1):
                # Extract timestamp from filename
                name = backup_file.name
                if 'backup_' in name:
                    timestamp_part = name.split('backup_')[1].replace('.h', '')
                    try:
                        # Convert timestamp to readable format
                        dt = datetime.strptime(timestamp_part, "%Y%m%d_%H%M%S")
                        formatted_date = dt.strftime("%Y-%m-%d %H:%M:%S")
                        print(f"{Colors.CYAN}║{Colors.RESET}  {i:2}. {formatted_date}                                 {Colors.CYAN}║{Colors.RESET}")
                    except:
                        print(f"{Colors.CYAN}║{Colors.RESET}  {i:2}. {name:<46} {Colors.CYAN}║{Colors.RESET}")
                else:
                    print(f"{Colors.CYAN}║{Colors.RESET}  {i:2}. {name:<46} {Colors.CYAN}║{Colors.RESET}")
                    
            print(f"{Colors.CYAN}║{Colors.RESET}                                                          {Colors.CYAN}║{Colors.RESET}")
            print(f"{Colors.CYAN}║{Colors.RESET} [v] View backup content                                  {Colors.CYAN}║{Colors.RESET}")
            print(f"{Colors.CYAN}║{Colors.RESET} [r] Restore backup                                       {Colors.CYAN}║{Colors.RESET}")
            print(f"{Colors.CYAN}║{Colors.RESET} [b] Back to backup menu                                  {Colors.CYAN}║{Colors.RESET}")
            print(f"{Colors.CYAN}╚{'═'*58}╝{Colors.RESET}")
            
            choice = input(f"\n{Colors.CYAN}Select option: {Colors.RESET}").strip().lower()
            
            if choice == 'v':
                self._view_backup_content(recent_backups)
            elif choice == 'r':
                self._restore_backup(recent_backups)
            elif choice in ['b', 'back']:
                break
            else:
                print(f"{Colors.RED}❌ Invalid option. Please try again.{Colors.RESET}")
                input(f"{Colors.CYAN}Press Enter to continue...{Colors.RESET}")
                ASCIIArt.clear_screen()
                
    def _view_backup_content(self, backup_files: list):
        """View the content of a selected backup."""
        while True:
            backup_num = input(f"\n{Colors.CYAN}Enter backup number to view (1-{len(backup_files)}) or 'b' to go back: {Colors.RESET}").strip()
            
            if backup_num.lower() in ['b', 'back']:
                return
                
            try:
                backup_index = int(backup_num) - 1
                if 0 <= backup_index < len(backup_files):
                    backup_file = backup_files[backup_index]
                    
                    print(f"\n{Colors.BLUE}📄 Content of {backup_file.name}:{Colors.RESET}")
                    print("─" * 60)
                    
                    try:
                        with open(backup_file, 'r') as f:
                            content = f.read()
                            
                        # Show only the key configuration defines
                        lines = content.split('\n')
                        config_lines = []
                        for line in lines:
                            if any(keyword in line for keyword in ['#define LED_', '#define BUTTON_', '#define DISTANCE_', '#define MESH_']):
                                config_lines.append(line.strip())
                                
                        if config_lines:
                            for line in config_lines:
                                print(f"  {line}")
                        else:
                            # Show first 20 lines if no defines found
                            for line in lines[:20]:
                                print(f"  {line}")
                            if len(lines) > 20:
                                print(f"  ... ({len(lines) - 20} more lines)")
                                
                    except Exception as e:
                        ASCIIArt.show_error(f"Failed to read backup file: {e}")
                        
                    print("─" * 60)
                    input(f"\n{Colors.CYAN}Press Enter to continue...{Colors.RESET}")
                    return
                else:
                    print(f"{Colors.RED}❌ Invalid backup number. Please enter 1-{len(backup_files)}.{Colors.RESET}")
            except ValueError:
                print(f"{Colors.RED}❌ Invalid input. Please enter a number or 'b'.{Colors.RESET}")
                
    def _restore_backup(self, backup_files: list):
        """Restore a selected backup."""
        while True:
            backup_num = input(f"\n{Colors.CYAN}Enter backup number to restore (1-{len(backup_files)}) or 'b' to go back: {Colors.RESET}").strip()
            
            if backup_num.lower() in ['b', 'back']:
                return
                
            try:
                backup_index = int(backup_num) - 1
                if 0 <= backup_index < len(backup_files):
                    backup_file = backup_files[backup_index]
                    
                    # Extract timestamp for display
                    name = backup_file.name
                    if 'backup_' in name:
                        timestamp_part = name.split('backup_')[1].replace('.h', '')
                        try:
                            dt = datetime.strptime(timestamp_part, "%Y%m%d_%H%M%S")
                            formatted_date = dt.strftime("%Y-%m-%d %H:%M:%S")
                            display_name = f"backup from {formatted_date}"
                        except:
                            display_name = name
                    else:
                        display_name = name
                        
                    print(f"\n{Colors.YELLOW}⚠️  This will overwrite the current configuration!{Colors.RESET}")
                    print(f"Restore {display_name}?")
                    
                    confirm = input(f"{Colors.CYAN}Continue? (y/N): {Colors.RESET}").strip().lower()
                    
                    if confirm in ['y', 'yes']:
                        try:
                            # Create backup of current config before restoring
                            if self.config_manager.config_exists():
                                print("💾 Creating backup of current configuration...")
                                self.config_manager._create_backup()
                                
                            # Restore the selected backup
                            import shutil
                            shutil.copy2(backup_file, self.config_manager.config_path)
                            
                            ASCIIArt.show_success(f"Configuration restored from {display_name}!")
                            print(f"📁 Current configuration backed up before restoration")
                            
                        except Exception as e:
                            ASCIIArt.show_error(f"Failed to restore backup: {e}")
                            
                        input(f"\n{Colors.CYAN}Press Enter to continue...{Colors.RESET}")
                        return
                    else:
                        print("Restore cancelled.")
                        return
                else:
                    print(f"{Colors.RED}❌ Invalid backup number. Please enter 1-{len(backup_files)}.{Colors.RESET}")
            except ValueError:
                print(f"{Colors.RED}❌ Invalid input. Please enter a number or 'b'.{Colors.RESET}")
                
    def _list_all_backups(self):
        """List all available backups."""
        backup_files = list(self.config_manager.backup_dir.glob("*.h"))
        
        if not backup_files:
            ASCIIArt.show_error("No backup files found!")
        else:
            # Sort by modification time (newest first)
            backup_files.sort(key=lambda x: x.stat().st_mtime, reverse=True)
            
            print(f"\n📋 All available backups ({len(backup_files)} total):")
            print("─" * 60)
            
            for i, backup_file in enumerate(backup_files, 1):
                # Get file stats
                stat = backup_file.stat()
                mod_time = datetime.fromtimestamp(stat.st_mtime)
                size = stat.st_size
                
                # Extract timestamp from filename for better display
                name = backup_file.name
                if 'backup_' in name:
                    timestamp_part = name.split('backup_')[1].replace('.h', '')
                    try:
                        dt = datetime.strptime(timestamp_part, "%Y%m%d_%H%M%S")
                        formatted_date = dt.strftime("%Y-%m-%d %H:%M:%S")
                        print(f"  {i:2}. {formatted_date} ({size:,} bytes)")
                    except:
                        print(f"  {i:2}. {name} ({size:,} bytes)")
                else:
                    print(f"  {i:2}. {name} ({size:,} bytes)")
                    
            print("─" * 60)
            print(f"📁 Backups location: {self.config_manager.backup_dir}")
            
        input(f"\n{Colors.CYAN}Press Enter to continue...{Colors.RESET}")
