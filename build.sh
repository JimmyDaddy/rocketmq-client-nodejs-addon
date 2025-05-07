#!/usr/bin/env bash

set -e

basepath=$(
  cd $(dirname "$0")
  pwd
)

echo $basepath

homebrew=$(brew --prefix)

InstallMusl() {
  local MUSL_VERSION="musl-1.2.5"
  local MUSL_URL="https://musl.libc.org/releases/$MUSL_VERSION.tar.gz"
  local MUSL_FILE="$MUSL_VERSION.tar.gz"
  local MUSL_DIR="$basepath/toolchain/musl"
  local TOOLCHAIN_DIR="$basepath/toolchain"

  # Create toolchain directory if it doesn't exist
  if [ ! -d "$TOOLCHAIN_DIR" ]; then
    mkdir -p "$TOOLCHAIN_DIR"
  fi

  # Check if musl tarball exists
  if [ -f "$TOOLCHAIN_DIR/$MUSL_FILE" ]; then
    echo "musl tarball found, verifying integrity..."
    pushd "$TOOLCHAIN_DIR" > /dev/null

    # Verify tarball integrity
    if tar -tzf "$MUSL_FILE" > /dev/null 2>&1; then
      echo "musl tarball is valid"

      # Extract if musl directory doesn't exist
      if [ ! -d "$MUSL_DIR" ]; then
        echo "Extracting musl..."
        tar -xzf "$MUSL_FILE"
        mv "$MUSL_VERSION" musl
      else
        echo "musl is already extracted"
      fi

      popd > /dev/null
      return
    else
      echo "musl tarball is corrupted, will download again"
      rm "$MUSL_FILE"
    fi

    popd > /dev/null
  fi

  # Download and install musl if not present or corrupted
  if [ ! -d "$MUSL_DIR" ]; then
    pushd "$TOOLCHAIN_DIR" > /dev/null

    echo "Downloading musl version $MUSL_VERSION..."
    curl -OL "$MUSL_URL"

    echo "Extracting musl..."
    tar -xzf "$MUSL_FILE"
    mv "$MUSL_VERSION" musl

    popd > /dev/null
    echo "musl installation complete"
  fi
}

InstallZig() {
  local ZIG_VERSION="zig-macos-aarch64-0.15.0-dev.471+369177f0b"
  local ZIG_URL="https://ziglang.org/builds/$ZIG_VERSION.tar.xz"
  local ZIG_FILE="$ZIG_VERSION.tar.xz"
  local ZIG_DIR="$basepath/toolchain/zig"
  local TOOLCHAIN_DIR="$basepath/toolchain"

  # Create toolchain directory if it doesn't exist
  if [ ! -d "$TOOLCHAIN_DIR" ]; then
    mkdir -p "$TOOLCHAIN_DIR"
  fi

  # Check if Zig package exists
  if [ -f "$TOOLCHAIN_DIR/$ZIG_FILE" ]; then
    echo "Zig package found, verifying integrity..."
    pushd "$TOOLCHAIN_DIR" > /dev/null

    # Verify package integrity
    if tar -tf "$ZIG_FILE" > /dev/null 2>&1; then
      echo "Zig package is valid"

      # Extract if Zig directory doesn't exist
      if [ ! -d "$ZIG_DIR" ]; then
        echo "Extracting Zig..."
        tar -xf "$ZIG_FILE"
        mv "$ZIG_VERSION" zig
      else
        echo "Zig is already extracted"
      fi

      popd > /dev/null
      return
    else
      echo "Zig package is corrupted, will download again"
      rm "$ZIG_FILE"
    fi

    popd > /dev/null
  fi

  # Download and install Zig if not present or corrupted
  if [ ! -d "$ZIG_DIR" ]; then
    pushd "$TOOLCHAIN_DIR" > /dev/null

    echo "Downloading Zig version $ZIG_VERSION..."
    curl -OL "$ZIG_URL"

    echo "Extracting Zig..."
    tar -xf "$ZIG_FILE"
    mv "$ZIG_VERSION" zig

    popd > /dev/null
    echo "Zig installation complete"
  fi
}

BuildForLinux() {
  export CMAKE_SYSTEM_NAME=$OS

  case "$1" in
    aarch64|x86_64)
      export CMAKE_SYSTEM_PROCESSOR=$1
      ;;
    *)
      echo "Error: Unsupported architecture. Only aarch64 and x86_64 are supported."
      exit 1
      ;;
  esac

  export HOST="$CMAKE_SYSTEM_PROCESSOR-linux-musl"

  export ZIG_PATH="$basepath/toolchain/zig"
  export MUSL_LIBC_PATH="$basepath/toolchain/musl/lib/libc.a"
  export MUSL_CROSS_PATH="$homebrew/opt/$CMAKE_SYSTEM_PROCESSOR-unknown-linux-musl/bin"

  export CC="$ZIG_PATH/zig cc -target $HOST"
  export CXX="$ZIG_PATH/zig c++ -target $HOST"
  export LINK="$ZIG_PATH/zig c++ -target $HOST -shared"
  export AR="$MUSL_CROSS_PATH/$HOST-ar"
  export RANLIB="$MUSL_CROSS_PATH/$HOST-ranlib"
  export STRIP="$MUSL_CROSS_PATH/$HOST-strip"

  export uname=$OS

  export CFLAGS="-fPIC"
  export CXXFLAGS="-fPIC"

  # build rocketmq
  ./deps/rocketmq/build.sh

  # build musl
  pushd toolchain/musl
  ./configure
  make -j
  popd

  # build rocketmq.node
  npx cmake-js compile -- \
    --CDCMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}" \
    --CDCMAKE_SYSTEM_PROCESSOR="${CMAKE_SYSTEM_PROCESSOR}" \
    --CDCMAKE_SHARED_LINKER_FLAGS="-fPIC" \
    --CDCMAKE_SKIP_DEPENDENCY_TRACKING=ON \
    --verbose
}

BuildForDarwin() {
    export CC="/usr/bin/cc -arch arm64 -arch x86_64"
    export CXX="/usr/bin/c++ -arch arm64 -arch x86_64"
    export LINK="/usr/bin/ld"
    export AR="/usr/bin/ar"
    export RANLIB="/usr/bin/ranlib"
    export STRIP="/usr/bin/strip"

    # build rocketmq
    ./deps/rocketmq/build.sh

    # build rocketmq.node
    npx cmake-js compile -- \
      --CDCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      --CDCMAKE_OSX_DEPLOYMENT_TARGET=11 \
      --CDCMAKE_SKIP_DEPENDENCY_TRACKING=ON \
      --verbose
}

Release() {
  local target="$1"

  if [ -z "$target" ]; then
    echo "Error: Release argument is missing."
    return 1
  fi

  local debug_dir="$basepath/Debug"
  local release_dir="$basepath/Release"
  local build_file="$basepath/build/rocketmq.node"
  local debug_file="$debug_dir/$target-rocketmq.node"
  local release_file="$release_dir/$target-rocketmq.node"

  mkdir -p "$debug_dir" "$release_dir"

  rsync -av --checksum "$build_file" "$debug_file"

  if [ "$OS" == "Darwin" ]; then
    $STRIP -x "$debug_file" -o "$release_file"
  else
    $STRIP "$debug_file" -o "$release_file"
  fi
}

Clean() {
  rm -rf ./deps/rocketmq/bin
  rm -rf ./deps/rocketmq/libs/signature/lib

  rm -rf ./deps/rocketmq/tmp_build_dir

  find ./deps/rocketmq/tmp_down_dir -type d -name "jsoncpp*" -exec rm -rf {} \; || true
  find ./deps/rocketmq/tmp_down_dir -type d -name "libevent-release*" -exec rm -rf {} \; || true
  find ./deps/rocketmq/tmp_down_dir -type d -name "spdlog*" -exec rm -rf {} \; || true
  find ./deps/rocketmq/tmp_down_dir -type d -name "zlib*" -exec rm -rf {} \; || true

  rm -rf ./toolchain/musl/obj
  rm -rf ./toolchain/musl/*.o || true
}

print_help() {
  echo "Usage: $0 [command] [options]"
  echo ""
  echo "Commands:"
  echo "  build [OS]            Build for the specified OS (Linux or Darwin)"
  echo ""
  echo "Options:"
  echo "  --arch ARCH          Specify the architecture (x86_64 or aarch64)"
  echo "  --verbose            Display commands as they are executed"
  echo "  --help               Show this help message"
  echo ""
  echo "Examples:"
  echo "  $0 build Linux --arch x86_64    Build for Linux with x86_64 architecture"
  echo "  $0 build Darwin                 Build for macOS (universal binary)"
  echo "  $0 Linux                        Legacy syntax: build for Linux (all architectures)"
  echo ""
  exit 0
}

parse_args() {
  local command=""
  local os="$(uname)"
  local arch=""
  local verbose=0

  if [[ $# -eq 0 ]]; then
    export OS=$os
    export ARCH=$arch
    export COMMAND=$command
    export VERBOSE=$verbose
    return
  fi

  if [[ "$1" == "Linux" || "$1" == "Darwin" ]]; then
    os="$1"
    command="build"
    shift
  fi

  while [[ $# -gt 0 ]]; do
    case "$1" in
      build)
        command="build"
        shift
        if [[ $# -gt 0 && ("$1" == "Linux" || "$1" == "Darwin") ]]; then
          os="$1"
          shift
        fi
        ;;
      --arch)
        if [[ $# -gt 1 ]]; then
          arch="$2"
          shift 2
        else
          echo "Error: --arch requires an argument."
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
        echo "Error: Unknown option: $1"
        print_help
        ;;
    esac
  done

  export OS=$os
  export ARCH=$arch
  export COMMAND=$command
  export VERBOSE=$verbose

  case "$os" in
    "Linux")
      build_linux_targets
      ;;
    "Darwin")
      build_darwin_target
      ;;
    *)
      echo "Error: Unsupported operating system. Only Linux and Darwin are supported."
      exit 1
      ;;
  esac
}

build_linux_targets() {
  InstallMusl
  InstallZig

  if [[ -n "$ARCH" ]]; then
    case "$ARCH" in
      aarch64|x86_64)
        Clean && BuildForLinux "$ARCH" && Release "linux-$ARCH"
        ;;
      *)
        echo "Error: Unsupported architecture. Only aarch64 and x86_64 are supported."
        exit 1
        ;;
    esac
  else
    local architectures=("x86_64" "aarch64")

    for arch in "${architectures[@]}"; do
      Clean && BuildForLinux "$arch" && Release "linux-$arch"
    done
  fi
  exit 0
}

build_darwin_target() {
  InstallMusl
  InstallZig

  Clean && BuildForDarwin && Release "darwin-universal"
  exit 0
}

parse_args "$@"
