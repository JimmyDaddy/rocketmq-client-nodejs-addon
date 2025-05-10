#!/usr/bin/env bash
#
# RocketMQ Node.js Addon Build Script
# 
# This script builds the RocketMQ Node.js addon for different platforms and architectures.
# It handles dependency installation, compilation, and packaging of the addon.
#

# Exit on error
set -e

# Get the base path of the script
BASEPATH=$(cd $(dirname "$0") && pwd)
HOMEBREW=$(brew --prefix)

# Configuration
MUSL_VERSION="musl-1.2.5"
ZIG_VERSION="zig-macos-aarch64-0.15.0-dev.471+369177f0b"

# Directories
TOOLCHAIN_DIR="$BASEPATH/toolchain"
MUSL_DIR="$TOOLCHAIN_DIR/musl"
ZIG_DIR="$TOOLCHAIN_DIR/zig"
DEBUG_DIR="$BASEPATH/Debug"
RELEASE_DIR="$BASEPATH/Release"
BUILD_FILE="$BASEPATH/build/rocketmq.node"

# URLs
MUSL_URL="https://musl.libc.org/releases/$MUSL_VERSION.tar.gz"
ZIG_URL="https://ziglang.org/builds/$ZIG_VERSION.tar.xz"

# Files
MUSL_FILE="$MUSL_VERSION.tar.gz"
ZIG_FILE="$ZIG_VERSION.tar.xz"

# Log levels
LOG_INFO="\033[0;32m[INFO]\033[0m"
LOG_WARN="\033[0;33m[WARN]\033[0m"
LOG_ERROR="\033[0;31m[ERROR]\033[0m"

#
# Utility Functions
#

# Print a message with a specific log level
log() {
  local level="$1"
  local message="$2"
  echo -e "$level $message"
}

# Print an info message
log_info() {
  log "$LOG_INFO" "$1"
}

# Print a warning message
log_warn() {
  log "$LOG_WARN" "$1"
}

# Print an error message
log_error() {
  log "$LOG_ERROR" "$1"
}

# Print a section header
print_section() {
  echo ""
  echo "====================================================================="
  echo "  $1"
  echo "====================================================================="
  echo ""
}

# Check if a command exists
command_exists() {
  command -v "$1" >/dev/null 2>&1
}

# Create a directory if it doesn't exist
ensure_dir() {
  if [ ! -d "$1" ]; then
    mkdir -p "$1"
    log_info "Created directory: $1"
  fi
}

#
# Dependency Installation Functions
#

# Install Musl libc
install_musl() {
  print_section "Installing Musl libc"

  ensure_dir "$TOOLCHAIN_DIR"

  # Check if musl is already installed
  if [ -d "$MUSL_DIR" ] && [ -f "$MUSL_DIR/lib/libc.a" ]; then
    log_info "Musl libc is already installed"
    return 0
  fi

  # Check if musl tarball exists and is valid
  if [ -f "$TOOLCHAIN_DIR/$MUSL_FILE" ]; then
    log_info "Musl tarball found, verifying integrity..."
    pushd "$TOOLCHAIN_DIR" > /dev/null

    if tar -tzf "$MUSL_FILE" > /dev/null 2>&1; then
      log_info "Musl tarball is valid"

      # Extract if musl directory doesn't exist
      if [ ! -d "$MUSL_DIR" ]; then
        log_info "Extracting musl..."
        tar -xzf "$MUSL_FILE"
        mv "$MUSL_VERSION" musl
      else
        log_info "Musl is already extracted"
      fi

      popd > /dev/null
      return 0
    else
      log_warn "Musl tarball is corrupted, will download again"
      rm "$MUSL_FILE"
    fi

    popd > /dev/null
  fi

  # Download and install musl if not present or corrupted
  if [ ! -d "$MUSL_DIR" ]; then
    pushd "$TOOLCHAIN_DIR" > /dev/null

    log_info "Downloading Musl version $MUSL_VERSION..."
    curl -OL "$MUSL_URL"

    log_info "Extracting Musl..."
    tar -xzf "$MUSL_FILE"
    mv "$MUSL_VERSION" musl

    popd > /dev/null
    log_info "Musl installation complete"
  fi
}

# Install Zig compiler
install_zig() {
  print_section "Installing Zig compiler"

  ensure_dir "$TOOLCHAIN_DIR"

  # Check if Zig is already installed
  if [ -d "$ZIG_DIR" ] && [ -f "$ZIG_DIR/zig" ]; then
    log_info "Zig compiler is already installed"
    return 0
  fi

  # Check if Zig package exists and is valid
  if [ -f "$TOOLCHAIN_DIR/$ZIG_FILE" ]; then
    log_info "Zig package found, verifying integrity..."
    pushd "$TOOLCHAIN_DIR" > /dev/null

    if tar -tf "$ZIG_FILE" > /dev/null 2>&1; then
      log_info "Zig package is valid"

      # Extract if Zig directory doesn't exist
      if [ ! -d "$ZIG_DIR" ]; then
        log_info "Extracting Zig..."
        tar -xf "$ZIG_FILE"
        mv "$ZIG_VERSION" zig
      else
        log_info "Zig is already extracted"
      fi

      popd > /dev/null
      return 0
    else
      log_warn "Zig package is corrupted, will download again"
      rm "$ZIG_FILE"
    fi

    popd > /dev/null
  fi

  # Download and install Zig if not present or corrupted
  if [ ! -d "$ZIG_DIR" ]; then
    pushd "$TOOLCHAIN_DIR" > /dev/null

    log_info "Downloading Zig version $ZIG_VERSION..."
    curl -OL "$ZIG_URL"

    log_info "Extracting Zig..."
    tar -xf "$ZIG_FILE"
    mv "$ZIG_VERSION" zig

    popd > /dev/null
    log_info "Zig installation complete"
  fi
}

#
# Build Functions
#

# Build for Linux
build_for_linux() {
  local arch="$1"

  print_section "Building for Linux ($arch)"

  # Validate architecture
  case "$arch" in
    aarch64|x86_64)
      export CMAKE_SYSTEM_PROCESSOR=$arch
      ;;
    *)
      log_error "Unsupported architecture. Only aarch64 and x86_64 are supported."
      exit 1
      ;;
  esac

  export CMAKE_SYSTEM_NAME="Linux"
  export HOST="$CMAKE_SYSTEM_PROCESSOR-linux-musl"

  # Set up toolchain
  export ZIG_PATH="$BASEPATH/toolchain/zig"
  export MUSL_LIBC_PATH="$BASEPATH/toolchain/musl/lib/libc.a"
  export MUSL_CROSS_PATH="$HOMEBREW/opt/$CMAKE_SYSTEM_PROCESSOR-unknown-linux-musl/bin"

  # Set up compiler and linker
  export CC="$ZIG_PATH/zig cc -target $HOST -fno-sanitize=undefined"
  export CXX="$ZIG_PATH/zig c++ -target $HOST -fno-sanitize=undefined"
  export LINK="$ZIG_PATH/zig c++ -target $HOST -shared -fno-sanitize=undefined"
  export AR="$MUSL_CROSS_PATH/$HOST-ar"
  export RANLIB="$MUSL_CROSS_PATH/$HOST-ranlib"
  export STRIP="$MUSL_CROSS_PATH/$HOST-strip"

  export uname="Linux"
  export CFLAGS="-fPIC"
  export CXXFLAGS="-fPIC"

  # Build RocketMQ C++ client
  log_info "Building RocketMQ C++ client..."
  ./deps/rocketmq/build.sh

  # Build Musl
  log_info "Building Musl libc..."
  pushd toolchain/musl
  ./configure
  make -j
  popd

  # Build RocketMQ Node.js addon
  log_info "Building RocketMQ Node.js addon..."
  npx cmake-js compile -- \
    --CDCMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}" \
    --CDCMAKE_SYSTEM_PROCESSOR="${CMAKE_SYSTEM_PROCESSOR}" \
    --CDCMAKE_SHARED_LINKER_FLAGS="-fPIC" \
    --CDCMAKE_SKIP_DEPENDENCY_TRACKING=ON \
    --verbose
}

# Build for macOS (Darwin)
build_for_darwin() {
  print_section "Building for macOS (universal binary)"

  # Set up compiler and linker for universal binary (arm64 + x86_64)
  export CC="/usr/bin/cc -arch arm64 -arch x86_64"
  export CXX="/usr/bin/c++ -arch arm64 -arch x86_64"
  export LINK="/usr/bin/ld"
  export AR="/usr/bin/ar"
  export RANLIB="/usr/bin/ranlib"
  export STRIP="/usr/bin/strip"

  # Build RocketMQ C++ client
  log_info "Building RocketMQ C++ client..."
  ./deps/rocketmq/build.sh

  # Build RocketMQ Node.js addon
  log_info "Building RocketMQ Node.js addon..."
  npx cmake-js compile -- \
    --CDCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    --CDCMAKE_OSX_DEPLOYMENT_TARGET=11 \
    --CDCMAKE_SKIP_DEPENDENCY_TRACKING=ON \
    --verbose
}

# Create release files
create_release() {
  local target="$1"

  print_section "Creating release for $target"

  if [ -z "$target" ]; then
    log_error "Release argument is missing."
    return 1
  fi

  local debug_file="$DEBUG_DIR/$target-rocketmq.node"
  local release_file="$RELEASE_DIR/$target-rocketmq.node"

  ensure_dir "$DEBUG_DIR"
  ensure_dir "$RELEASE_DIR"

  log_info "Copying build file to debug directory..."
  rsync -av --checksum "$BUILD_FILE" "$debug_file"

  log_info "Stripping debug symbols for release..."
  if [ "$OS" == "Darwin" ]; then
    $STRIP -x "$debug_file" -o "$release_file"
  else
    $STRIP "$debug_file" -o "$release_file"
  fi

  log_info "Release created: $release_file"
}

# Clean build artifacts
clean() {
  print_section "Cleaning build artifacts"

  log_info "Cleaning RocketMQ C++ client artifacts..."
  rm -rf ./deps/rocketmq/bin
  rm -rf ./deps/rocketmq/libs/signature/lib
  rm -rf ./deps/rocketmq/tmp_build_dir

  log_info "Cleaning dependency build directories..."
  find ./deps/rocketmq/tmp_down_dir -type d -name "jsoncpp*" -exec rm -rf {} \; 2>/dev/null || true
  find ./deps/rocketmq/tmp_down_dir -type d -name "libevent-release*" -exec rm -rf {} \; 2>/dev/null || true
  find ./deps/rocketmq/tmp_down_dir -type d -name "spdlog*" -exec rm -rf {} \; 2>/dev/null || true
  find ./deps/rocketmq/tmp_down_dir -type d -name "zlib*" -exec rm -rf {} \; 2>/dev/null || true

  log_info "Cleaning Musl build artifacts..."
  rm -rf ./toolchain/musl/obj
  rm -rf ./toolchain/musl/*.o 2>/dev/null || true

  log_info "Clean complete"
}

#
# Command-line Interface Functions
#

# Print help message
print_help() {
  cat << EOF
Usage: $0 [command] [options]

Commands:
  build [OS]            Build for the specified OS (Linux or Darwin)
                        If OS is not specified, builds for both Linux and Darwin
  clean                 Clean build artifacts
  help                  Show this help message

Options:
  --arch ARCH          Specify the architecture (x86_64 or aarch64)
  --verbose            Display commands as they are executed
  --help               Show this help message

Examples:
  $0 build Linux --arch x86_64    Build for Linux with x86_64 architecture
  $0 build Darwin                 Build for macOS (universal binary)
  $0 build                        Build for both Linux and Darwin
  $0 clean                        Clean build artifacts

EOF
  exit 0
}

# Parse command-line arguments
parse_args() {
  local command=""
  local os="$(uname)"
  local arch=""
  local verbose=0

  # Handle no arguments
  if [[ $# -eq 0 ]]; then
    export OS=$os
    export ARCH=$arch
    export COMMAND=$command
    export VERBOSE=$verbose
    return
  fi


  # Parse arguments
  while [[ $# -gt 0 ]]; do
    case "$1" in
      build)
        command="build"
        shift
        if [[ $# -gt 0 && ("$1" == "Linux" || "$1" == "Darwin") ]]; then
          os="$1"
          shift
        else
          # If no OS specified after build command, set a flag to build for both
          os="*"
        fi
        ;;
      clean)
        command="clean"
        shift
        ;;
      help)
        print_help
        ;;
      --arch)
        if [[ $# -gt 1 ]]; then
          arch="$2"
          shift 2
        else
          log_error "--arch requires an argument."
          exit 1
        fi
        ;;
      --verbose)
        verbose=1
        set -x
        shift
        ;;
      --help)
        print_help
        ;;
      *)
        log_error "Unknown option: $1"
        print_help
        ;;
    esac
  done

  # Export variables for use in other functions
  export OS=$os
  export ARCH=$arch
  export COMMAND=$command
  export VERBOSE=$verbose

  # Execute command
  case "$command" in
    "build")
      case "$os" in
        "Linux")
          build_linux_targets
          ;;
        "Darwin")
          build_darwin_target
          ;;
        "*")
          build_all_targets
          ;;
        *)
          log_error "Unsupported operating system. Only Linux and Darwin are supported."
          exit 1
          ;;
      esac
      ;;
    "clean")
      clean
      exit 0
      ;;
    *)
      # Default to building for current OS if no command specified
      case "$os" in
        "Linux")
          build_linux_targets
          ;;
        "Darwin")
          build_darwin_target
          ;;
        *)
          log_error "Unsupported operating system. Only Linux and Darwin are supported."
          exit 1
          ;;
      esac
      ;;
  esac
}

# Build for all supported Linux architectures
build_linux_targets() {
  install_musl
  install_zig

  if [[ -n "$ARCH" ]]; then
    # Build for specific architecture
    case "$ARCH" in
      aarch64|x86_64)
        clean && build_for_linux "$ARCH" && create_release "linux-$ARCH"
        ;;
      *)
        log_error "Unsupported architecture. Only aarch64 and x86_64 are supported."
        exit 1
        ;;
    esac
  else
    # Build for all supported architectures
    local architectures=("x86_64" "aarch64")

    for arch in "${architectures[@]}"; do
      clean && build_for_linux "$arch" && create_release "linux-$arch"
    done
  fi
}

# Build for macOS (Darwin)
build_darwin_target() {
  install_musl
  install_zig

  clean && build_for_darwin && create_release "darwin-universal"
}

# Build for both Linux and Darwin
build_all_targets() {
  print_section "Building for both Linux and Darwin"

  log_info "Starting Linux build..."
  build_linux_targets

  log_info "Starting macOS build..."
  build_darwin_target

  log_info "All builds completed successfully"
}

# Main entry point
parse_args "$@"
