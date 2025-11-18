#!/bin/bash

# MongoDB TUI - Linux Build Script
echo -e "\033[36mMongoDB TUI - Linux Build\033[0m"
echo -e "\033[36m==========================\033[0m"
echo ""

# Function to print colored messages
print_success() { echo -e "\033[32m$1\033[0m"; }
print_error() { echo -e "\033[31m$1\033[0m"; }
print_warning() { echo -e "\033[33m$1\033[0m"; }
print_info() { echo -e "\033[36m$1\033[0m"; }

# Check if vcpkg is available
USE_VCPKG=false
if [ -n "$VCPKG_ROOT" ] && [ -d "$VCPKG_ROOT" ]; then
    print_success "VCPKG_ROOT: $VCPKG_ROOT"
    USE_VCPKG=true
elif [ -d "$HOME/vcpkg" ]; then
    export VCPKG_ROOT="$HOME/vcpkg"
    print_success "VCPKG_ROOT: $VCPKG_ROOT"
    USE_VCPKG=true
else
    print_warning "vcpkg not found, will use system packages"
fi

# Check for required tools
if ! command -v gcc &> /dev/null && ! command -v clang &> /dev/null; then
    print_error "ERROR: No C compiler found (gcc or clang required)"
    exit 1
fi

if command -v gcc &> /dev/null; then
    CC_COMPILER=$(which gcc)
    print_success "GCC found: $CC_COMPILER"
elif command -v clang &> /dev/null; then
    CC_COMPILER=$(which clang)
    print_success "Clang found: $CC_COMPILER"
fi

if ! command -v cmake &> /dev/null; then
    print_error "ERROR: CMake not found"
    print_info "Install with: sudo apt install cmake (Debian/Ubuntu)"
    print_info "           or: sudo dnf install cmake (Fedora)"
    print_info "           or: sudo pacman -S cmake (Arch)"
    exit 1
fi
print_success "CMake found: $(which cmake)"

# Check for make or ninja
if command -v ninja &> /dev/null; then
    GENERATOR="Ninja"
    print_success "Ninja found: $(which ninja)"
elif command -v make &> /dev/null; then
    GENERATOR="Unix Makefiles"
    print_success "Make found: $(which make)"
else
    print_error "ERROR: Neither make nor ninja found"
    exit 1
fi

# Clean previous build
if [ -d "_build" ]; then
    print_warning "Cleaning previous build..."
    rm -rf _build
fi

# Configure CMake
print_info "Configuring with CMake ($GENERATOR)..."
echo ""

if [ "$USE_VCPKG" = true ]; then
    TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    print_info "Toolchain: $TOOLCHAIN"

    cmake -S . -B _build \
        -G "$GENERATOR" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$CC_COMPILER"
else
    print_info "Using system packages (no vcpkg)"

    cmake -S . -B _build \
        -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$CC_COMPILER"
fi

if [ $? -ne 0 ]; then
    echo ""
    print_error "ERROR: Configuration failed!"
    echo ""
    print_warning "Make sure you have the required dependencies:"
    print_info "  Debian/Ubuntu: sudo apt install libmongoc-dev libncurses-dev"
    print_info "  Fedora:        sudo dnf install mongo-c-driver-devel ncurses-devel"
    print_info "  Arch:          sudo pacman -S mongo-c-driver ncurses"
    echo ""
    print_warning "Or you can use the Makefile directly:"
    print_info "  make"
    exit 1
fi

# Build
echo ""
print_warning "Building..."
cmake --build _build --config Release

if [ $? -ne 0 ]; then
    echo ""
    print_error "ERROR: Build failed!"
    exit 1
fi

echo ""
print_success "SUCCESS! Build completed."
echo ""
print_info "Executable: _build/mongodb-tui"
echo ""
print_warning "Run with:"
echo "  cd _build"
echo "  ./mongodb-tui"
echo ""
