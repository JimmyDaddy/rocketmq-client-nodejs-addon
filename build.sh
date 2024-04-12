#!/usr/bin/env bash

set -ex

basepath=$(
  cd $(dirname "$0")
  pwd
)

echo $basepath

InstallMusl() {
  local MUSL_VERSION="musl-1.2.5"
  if [ ! -d "$basepath/toolchain" ] ; then
    mkdir -p "$basepath/toolchain"
  fi
  if [ ! -d "$basepath/toolchain/musl" ] ; then
    pushd "$basepath/toolchain" > /dev/null

    echo "Downloading musl version $MUSL_VERSION..."
    curl -OL https://musl.libc.org/releases/$MUSL_VERSION.tar.gz

    tar -xzf $MUSL_VERSION.tar.gz
    mv $MUSL_VERSION musl

    popd > /dev/null
  fi
}

InstallZig() {
  local ZIG_VERSION="zig-macos-aarch64-0.12.0-dev.3633+f7a76bdfe"
  if [ ! -d "$basepath/toolchain" ] ; then
    mkdir -p "$basepath/toolchain"
  fi
  if [ ! -d "$basepath/toolchain/zig" ] ; then
    pushd "$basepath/toolchain" > /dev/null

    curl -OL https://ziglang.org/builds/$ZIG_VERSION.tar.xz

    tar -xf $ZIG_VERSION.tar.xz
    mv $ZIG_VERSION zig

    popd > /dev/null
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
  export MUSL_CROSS_PATH="/opt/homebrew/opt/musl-cross/bin"

  export CC="$ZIG_PATH/zig cc -target $HOST"
  export CXX="$ZIG_PATH/zig c++ -target $HOST"
  export LINK="$ZIG_PATH/zig c++ -target $HOST"
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
  cmake-js compile -- --CDCMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}" --CDCMAKE_SYSTEM_PROCESSOR="${CMAKE_SYSTEM_PROCESSOR}" --verbose
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
    cmake-js compile -- --CDCMAKE_OSX_ARCHITECTURES="arm64;x86_64" --DCMAKE_OSX_DEPLOYMENT_TARGET=11 --verbose
}

Release() {
  if [ -z "$1" ]; then
    echo "Error: Release argument is missing."
    return 1
  fi

  if [ ! -d "$basepath/release" ]; then
    mkdir "$basepath/release"
  fi

  cp "$basepath/build/rocketmq.node" "$basepath/release/$1-rocketmq-no-strip.node"
  $STRIP -x "$basepath/release/$1-rocketmq-no-strip.node" -o "$basepath/release/$1-rocketmq.node"
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

InstallMusl
InstallZig

if [ -z "$1" ]; then
  export OS=$(uname)
else
  export OS=$1
fi

case "$OS" in
  "Linux")
Clean && BuildForLinux "x86_64" && Release "linux-x86_64"
Clean && BuildForLinux "aarch64" && Release "linux-aarch64"
exit 0
    ;;
  "Darwin")
Clean && BuildForDarwin && Release "darwin"
exit 0
    ;;
  *)
    echo "Error: Unsupported operating system. Only Linux and Darwin are supported."
    exit 1
    ;;
esac
