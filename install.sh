#!/bin/bash

# GlowLight Setup Installation Script
# Usage: curl -fsSL https://raw.githubusercontent.com/friedjof/GlowLight/master/install.sh | bash
# or: wget -qO- https://raw.githubusercontent.com/friedjof/GlowLight/master/install.sh | bash

set -e  # Exit on any error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
REPO_URL="https://github.com/friedjof/GlowLight.git"
INSTALL_DIR="$HOME/GlowLight"
PYTHON_MIN_VERSION="3.8"

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "${CYAN}"
    echo "╔══════════════════════════════════════════════════════════════════════════════╗"
    echo "║                                                                              ║"
    echo "║ ░██████╗░██╗░░░░░░█████╗░░██╗░░░░░░░██╗██╗░░░░░██╗░██████╗░██╗░░██╗████████╗ ║"
    echo "║ ██╔════╝░██║░░░░░██╔══██╗░██║░░██╗░░██║██║░░░░░██║██╔════╝░██║░░██║╚══██╔══╝ ║"
    echo "║ ██║░░██╗░██║░░░░░██║░░██║░╚██╗████╗██╔╝██║░░░░░██║██║░░██╗░███████║░░░██║░░░ ║"
    echo "║ ██║░░╚██╗██║░░░░░██║░░██║░░████╔═████║░██║░░░░░██║██║░░╚██╗██╔══██║░░░██║░░░ ║"
    echo "║ ╚██████╔╝███████╗╚█████╔╝░░╚██╔╝░╚██╔╝░███████╗██║╚██████╔╝██║░░██║░░░██║░░░ ║"
    echo "║ ░╚═════╝░╚══════╝░╚════╝░░░░╚═╝░░░╚═╝░░╚══════╝╚═╝░╚═════╝░╚═╝░░╚═╝░░░╚═╝░░░ ║"
    echo "║                                                                              ║"
    echo "║                        🌟 Bedside Lamp Setup System 🌟                       ║"
    echo "║                                                                              ║"
    echo "╚══════════════════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
    echo -e "${BLUE}🚀 Welcome to the GlowLight Installation Script!${NC}"
    echo -e "This script will download and set up the GlowLight mesh networking system."
    echo ""
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check Python version
check_python_version() {
    if command_exists python3; then
        local python_version=$(python3 -c "import sys; print('.'.join(map(str, sys.version_info[:2])))")
        local min_version_major=$(echo $PYTHON_MIN_VERSION | cut -d. -f1)
        local min_version_minor=$(echo $PYTHON_MIN_VERSION | cut -d. -f2)
        local current_version_major=$(echo $python_version | cut -d. -f1)
        local current_version_minor=$(echo $python_version | cut -d. -f2)
        
        if [ "$current_version_major" -gt "$min_version_major" ] || 
           ([ "$current_version_major" -eq "$min_version_major" ] && [ "$current_version_minor" -ge "$min_version_minor" ]); then
            print_success "Python $python_version found (minimum required: $PYTHON_MIN_VERSION)"
            return 0
        else
            print_error "Python $python_version found, but minimum required is $PYTHON_MIN_VERSION"
            return 1
        fi
    else
        print_error "Python 3 not found"
        return 1
    fi
}

# Function to install dependencies on different systems
install_dependencies() {
    print_status "Checking and installing dependencies..."
    
    # Detect OS
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # Linux
        if command_exists apt-get; then
            # Debian/Ubuntu
            print_status "Detected Debian/Ubuntu system"
            if ! command_exists git; then
                print_status "Installing git..."
                sudo apt-get update && sudo apt-get install -y git
            fi
            if ! command_exists python3; then
                print_status "Installing Python 3..."
                sudo apt-get install -y python3 python3-pip python3-venv
            fi
            if ! command_exists pip3; then
                print_status "Installing pip..."
                sudo apt-get install -y python3-pip
            fi
        elif command_exists dnf; then
            # Fedora
            print_status "Detected Fedora system"
            if ! command_exists git; then
                print_status "Installing git..."
                sudo dnf install -y git
            fi
            if ! command_exists python3; then
                print_status "Installing Python 3..."
                sudo dnf install -y python3 python3-pip
            fi
        elif command_exists pacman; then
            # Arch Linux
            print_status "Detected Arch Linux system"
            if ! command_exists git; then
                print_status "Installing git..."
                sudo pacman -S --noconfirm git
            fi
            if ! command_exists python3; then
                print_status "Installing Python 3..."
                sudo pacman -S --noconfirm python python-pip
            fi
        else
            print_warning "Unknown Linux distribution. Please install git and python3 manually."
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        print_status "Detected macOS system"
        if ! command_exists git; then
            print_error "Git not found. Please install Xcode Command Line Tools:"
            print_error "xcode-select --install"
            exit 1
        fi
        if ! command_exists python3; then
            if command_exists brew; then
                print_status "Installing Python 3 via Homebrew..."
                brew install python
            else
                print_error "Python 3 not found. Please install Python 3 from https://python.org or install Homebrew."
                exit 1
            fi
        fi
    else
        print_warning "Unknown operating system. Please ensure git and python3 are installed."
    fi
}

# Function to clone or update repository
setup_repository() {
    print_status "Setting up GlowLight repository..."
    
    if [ -d "$INSTALL_DIR" ]; then
        print_status "GlowLight directory already exists at $INSTALL_DIR"
        read -p "Do you want to update it? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            print_status "Updating existing repository..."
            cd "$INSTALL_DIR"
            git pull origin master || {
                print_error "Failed to update repository"
                exit 1
            }
        else
            print_status "Using existing repository"
            cd "$INSTALL_DIR"
        fi
    else
        print_status "Cloning GlowLight repository to $INSTALL_DIR..."
        git clone "$REPO_URL" "$INSTALL_DIR" || {
            print_error "Failed to clone repository"
            exit 1
        }
        cd "$INSTALL_DIR"
    fi
    
    # Verify repository contents
    print_status "Verifying repository contents..."
    if [ ! -d "scripts" ]; then
        print_error "Repository seems incomplete - missing scripts directory"
        print_status "Repository contents:"
        ls -la
        exit 1
    fi
    
    if [ ! -f "platformio.ini" ]; then
        print_warning "platformio.ini not found - this might not be the correct repository"
    fi
    
    print_success "Repository ready at $INSTALL_DIR"
}

# Function to check system requirements
check_requirements() {
    print_status "Checking system requirements..."
    
    # Check Python
    if ! check_python_version; then
        print_error "Python version check failed"
        exit 1
    fi
    
    # Check git
    if ! command_exists git; then
        print_error "Git is required but not installed"
        return 1
    fi
    
    # Check for Python modules that might be needed
    print_status "Checking Python environment..."
    python3 -c "import sys, os, subprocess, pathlib" 2>/dev/null || {
        print_warning "Some basic Python modules may not be available"
    }
    
    print_success "System requirements check passed"
    return 0
}

# Function to run setup
run_setup() {
    print_status "Starting GlowLight setup system..."
    echo ""
    
    # Make sure we're in the right directory
    cd "$INSTALL_DIR"
    
    # Check if setup script exists
    if [ ! -f "scripts/setup.py" ]; then
        print_error "Setup script not found at scripts/setup.py"
        print_status "Repository contents:"
        ls -la scripts/ 2>/dev/null || print_warning "scripts/ directory not found"
        print_status "Current directory: $(pwd)"
        print_status "Trying to locate setup script..."
        
        # Try to find the setup script
        setup_script=""
        if [ -f "scripts/setup.py" ]; then
            setup_script="scripts/setup.py"
        elif [ -f "setup.py" ]; then
            setup_script="setup.py"
        elif [ -f "scripts/setup/cli_main.py" ]; then
            setup_script="scripts/setup/cli_main.py"
        else
            print_error "Could not locate setup script in any expected location"
            exit 1
        fi
        
        print_status "Found setup script at: $setup_script"
    else
        setup_script="scripts/setup.py"
    fi
    
    # Make setup script executable if needed
    chmod +x "$setup_script" 2>/dev/null || true
    
    # Run the setup script
    print_status "Executing: python3 $setup_script"
    python3 "$setup_script"
}

# Function to show completion message
show_completion() {
    echo ""
    print_success "GlowLight installation completed!"
    echo ""
    echo -e "${CYAN}📁 Project location: ${NC}$INSTALL_DIR"
    echo -e "${CYAN}🚀 To run setup again: ${NC}cd $INSTALL_DIR && python3 scripts/setup.py"
    echo -e "${CYAN}📖 Documentation: ${NC}https://github.com/friedjof/GlowLight"
    echo ""
    echo -e "${GREEN}Thank you for using GlowLight! 🌟${NC}"
}

# Function to handle cleanup on exit
cleanup() {
    if [ $? -ne 0 ]; then
        print_error "Installation failed!"
        echo ""
        echo "For help, please visit: https://github.com/friedjof/GlowLight/issues"
    fi
}

# Main installation function
main() {
    # Set up error handling
    trap cleanup EXIT
    
    # Show header
    print_header
    
    # Check if running as root (not recommended)
    if [ "$EUID" -eq 0 ]; then
        print_warning "Running as root is not recommended. Consider running as a regular user."
        read -p "Continue anyway? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_status "Installation cancelled."
            exit 1
        fi
    fi
    
    # Install dependencies
    install_dependencies
    
    # Check requirements
    check_requirements
    
    # Setup repository
    setup_repository
    
    # Run setup
    run_setup
    
    # Show completion message
    show_completion
}

# Run main function
main "$@"
