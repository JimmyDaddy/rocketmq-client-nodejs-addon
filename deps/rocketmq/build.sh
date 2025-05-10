#!/usr/bin/env bash
#
# RocketMQ C++ Client Build Script
# 
# This script builds the RocketMQ C++ client and its dependencies.
# It handles downloading and compiling spdlog, libevent, jsoncpp, zlib, and googletest.
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Exit on error, but allow for controlled error handling
set -e

# Get the base path of the script
BASEPATH=$(cd $(dirname "$0") && pwd)

# Directories
DOWN_DIR="${BASEPATH}/tmp_down_dir"
BUILD_DIR="${BASEPATH}/tmp_build_dir"
INSTALL_LIB_DIR="${BASEPATH}/bin"

# Dependency file patterns
FNAME_SPDLOG="spdlog*.zip"
FNAME_LIBEVENT="libevent*.zip"
FNAME_JSONCPP="jsoncpp*.zip"
FNAME_ZLIB="zlib*.zip"
FNAME_GTEST="googletest*.tar.gz"

# Dependency versions
FNAME_SPDLOG_DOWN="v1.13.0.zip"
FNAME_LIBEVENT_DOWN="release-2.1.12-stable.zip"
FNAME_JSONCPP_DOWN="1.9.5.zip"
FNAME_ZLIB_DOWN="1.3.1.zip"
FNAME_GTEST_DOWN="release-1.10.0.tar.gz"

# Build configuration
NEED_BUILD_SPDLOG=1
NEED_BUILD_JSONCPP=1
NEED_BUILD_LIBEVENT=1
NEED_BUILD_ZLIB=1
TEST=0
CODECOV=0
CPU_NUM=4

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

# Create a directory if it doesn't exist
ensure_dir() {
  if [ ! -d "$1" ]; then
    mkdir -p "$1"
    log_info "Created directory: $1"
  fi
}

# Check if version1 is less than version2
version_lt() {
  test "$(printf '%s\n' "$@" | sort -V | head -n 1)" != "$2"
}

# Check CMake version
check_cmake_version() {
  local cmake_version=$(cmake --version | head -n 1 | awk '{ print $3; }')
  if version_lt "${cmake_version}" "3.15"; then
    log_error "CMake version 3.15 or higher is required. Current version: ${cmake_version}"
    exit 1
  else
    log_info "CMake version ${cmake_version} is compatible"
  fi
}

# Parse command line arguments
parse_arguments() {
  for var in "$@"; do
    case "$var" in
    noLog)
      NEED_BUILD_SPDLOG=0
      ;;
    noJson)
      NEED_BUILD_JSONCPP=0
      ;;
    noEvent)
      NEED_BUILD_LIBEVENT=0
      ;;
    noZlib)
      NEED_BUILD_ZLIB=0
      ;;
    codecov)
      CODECOV=1
      ;;
    test)
      TEST=1
      ;;
    esac
  done
}

# Print build parameters
print_build_params() {
  print_section "Build Configuration"

  if [ $NEED_BUILD_SPDLOG -eq 0 ]; then
    log_info "Skipping spdlog build"
  else
    log_info "Building spdlog library"
  fi

  if [ $NEED_BUILD_LIBEVENT -eq 0 ]; then
    log_info "Skipping libevent build"
  else
    log_info "Building libevent library"
  fi

  if [ $NEED_BUILD_JSONCPP -eq 0 ]; then
    log_info "Skipping jsoncpp build"
  else
    log_info "Building jsoncpp library"
  fi

  if [ $NEED_BUILD_ZLIB -eq 0 ]; then
    log_info "Skipping zlib build"
  else
    log_info "Building zlib library"
  fi

  if [ $TEST -eq 1 ]; then
    log_info "Unit tests will be built and executed"

    if [ $CODECOV -eq 1 ]; then
      log_info "Code coverage will be enabled for tests"
    fi
  else
    log_info "Unit tests will not be built"
  fi
}

# Initialize by parsing arguments
parse_arguments "$@"

# Prepare the build environment
prepare_environment() {
  print_section "Preparing Build Environment"

  cd "${BASEPATH}"

  # Create download directory
  ensure_dir "${DOWN_DIR}"

  # Move any downloaded dependency archives to the download directory
  if [ -e ${FNAME_SPDLOG} ]; then
    log_info "Moving spdlog archive to download directory"
    mv -f ${BASEPATH}/${FNAME_SPDLOG} "${DOWN_DIR}"
  fi

  if [ -e ${FNAME_LIBEVENT} ]; then
    log_info "Moving libevent archive to download directory"
    mv -f ${BASEPATH}/${FNAME_LIBEVENT} "${DOWN_DIR}"
  fi

  if [ -e ${FNAME_JSONCPP} ]; then
    log_info "Moving jsoncpp archive to download directory"
    mv -f ${BASEPATH}/${FNAME_JSONCPP} "${DOWN_DIR}"
  fi

  if [ -e ${FNAME_GTEST} ]; then
    log_info "Moving googletest archive to download directory"
    mv -f ${BASEPATH}/${FNAME_GTEST} "${DOWN_DIR}"
  fi

  # Create build directory
  ensure_dir "${BUILD_DIR}"

  # Create installation directory
  ensure_dir "${INSTALL_LIB_DIR}"

  log_info "Build environment prepared successfully"
}

# Build spdlog library
build_spdlog() {
  print_section "Building spdlog Library"

  if [ $NEED_BUILD_SPDLOG -eq 0 ]; then
    log_info "Skipping spdlog build as requested"
    return 0
  fi

  if [ -d "${INSTALL_LIB_DIR}/include/spdlog" ]; then
    log_info "spdlog is already installed, skipping build"
    return 0
  fi

  log_info "Preparing to build spdlog"

  # Check if spdlog archive exists, download if not
  if [ -e ${DOWN_DIR}/${FNAME_SPDLOG} ]; then
    log_info "Using existing spdlog archive"
  else
    log_info "Downloading spdlog ${FNAME_SPDLOG_DOWN}"
    wget "https://github.com/gabime/spdlog/archive/${FNAME_SPDLOG_DOWN}" -O "${DOWN_DIR}/spdlog-${FNAME_SPDLOG_DOWN}"
  fi

  log_info "Extracting spdlog archive"
  unzip -o ${DOWN_DIR}/${FNAME_SPDLOG} -d "${DOWN_DIR}"

  # Find the spdlog directory
  spdlog_dir=$(ls -d ${DOWN_DIR}/spdlog* | grep -v zip)

  log_info "Installing spdlog headers to ${INSTALL_LIB_DIR}/include"
  cp -r "${spdlog_dir}/include" "${INSTALL_LIB_DIR}"

  log_info "spdlog build completed successfully"
}

# Build libevent library
build_libevent() {
  print_section "Building libevent Library"

  if [ $NEED_BUILD_LIBEVENT -eq 0 ]; then
    log_info "Skipping libevent build as requested"
    return 0
  fi

  if [ -d "${INSTALL_LIB_DIR}/include/event2" ]; then
    log_info "libevent is already installed, skipping build"
    return 0
  fi

  log_info "Preparing to build libevent"

  # Check if libevent archive exists, download if not
  if [ -e ${DOWN_DIR}/${FNAME_LIBEVENT} ]; then
    log_info "Using existing libevent archive"
  else
    log_info "Downloading libevent ${FNAME_LIBEVENT_DOWN}"
    wget "https://github.com/libevent/libevent/archive/${FNAME_LIBEVENT_DOWN}" -O "${DOWN_DIR}/libevent-${FNAME_LIBEVENT_DOWN}"
  fi

  log_info "Extracting libevent archive"
  unzip -o ${DOWN_DIR}/${FNAME_LIBEVENT} -d "${DOWN_DIR}"

  # Find the libevent directory
  libevent_dir=$(ls -d ${DOWN_DIR}/libevent* | grep -v zip)

  log_info "Building libevent static library"
  pushd "${libevent_dir}"
  log_info "Running autogen.sh"
  ./autogen.sh

  log_info "Configuring libevent build"
  ./configure --host="${HOST}" --disable-openssl --enable-static=yes --enable-shared=no CFLAGS=-fPIC CPPFLAGS=-fPIC --prefix="${INSTALL_LIB_DIR}"

  log_info "Compiling libevent (using $CPU_NUM CPU cores)"
  make -j $CPU_NUM

  log_info "Installing libevent"
  make install
  popd

  log_info "libevent build completed successfully"
}

# Build jsoncpp library
build_jsoncpp() {
  print_section "Building jsoncpp Library"

  if [ $NEED_BUILD_JSONCPP -eq 0 ]; then
    log_info "Skipping jsoncpp build as requested"
    return 0
  fi

  if [ -d "${INSTALL_LIB_DIR}/include/jsoncpp" ]; then
    log_info "jsoncpp is already installed, skipping build"
    return 0
  fi

  log_info "Preparing to build jsoncpp"

  # Check if jsoncpp archive exists, download if not
  if [ -e ${DOWN_DIR}/${FNAME_JSONCPP} ]; then
    log_info "Using existing jsoncpp archive"
  else
    log_info "Downloading jsoncpp ${FNAME_JSONCPP_DOWN}"
    wget "https://github.com/open-source-parsers/jsoncpp/archive/${FNAME_JSONCPP_DOWN}" -O "${DOWN_DIR}/jsoncpp-${FNAME_JSONCPP_DOWN}"
  fi

  log_info "Extracting jsoncpp archive"
  unzip -o ${DOWN_DIR}/${FNAME_JSONCPP} -d "${DOWN_DIR}"

  # Find the jsoncpp directory
  jsoncpp_dir=$(ls -d ${DOWN_DIR}/jsoncpp* | grep -v zip)

  log_info "Building jsoncpp static library"

  log_info "Configuring jsoncpp build with CMake"
  cmake -S "${jsoncpp_dir}" -B "${jsoncpp_dir}/build" \
    -DCMAKE_AR="${AR}" \
    -DCMAKE_RANLIB="${RANLIB}" \
    -DCMAKE_LINKER="${LINK}" \
    -DCMAKE_SYSTEM_PROCESSOR="${CMAKE_SYSTEM_PROCESSOR}" \
    -DCMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}" \
    -DJSONCPP_WITH_POST_BUILD_UNITTEST=0 \
    -DCMAKE_CXX_FLAGS=-fPIC \
    -DBUILD_STATIC_LIBS=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_LIB_DIR}"

  log_info "Compiling jsoncpp"
  cmake --build "${jsoncpp_dir}/build"

  log_info "Installing jsoncpp"
  cmake --install "${jsoncpp_dir}/build"

  # Handle special cases for library location
  if [ ! -f "${INSTALL_LIB_DIR}/lib/libjsoncpp.a" ]; then
    log_warn "libjsoncpp.a not found in expected location, checking alternative locations"

    if [ -f "${INSTALL_LIB_DIR}/lib/x86_64-linux-gnu/libjsoncpp.a" ]; then
      log_info "Found libjsoncpp.a in lib/x86_64-linux-gnu, copying to lib/"
      cp "${INSTALL_LIB_DIR}/lib/x86_64-linux-gnu/libjsoncpp.a" "${INSTALL_LIB_DIR}/lib/"
    fi

    if [ -f "${INSTALL_LIB_DIR}/lib64/libjsoncpp.a" ]; then
      log_info "Found libjsoncpp.a in lib64, copying to lib/"
      cp "${INSTALL_LIB_DIR}/lib64/libjsoncpp.a" "${INSTALL_LIB_DIR}/lib/"
    fi
  fi

  log_info "jsoncpp build completed successfully"
}

# Build zlib library
build_zlib() {
  print_section "Building zlib Library"

  if [ $NEED_BUILD_ZLIB -eq 0 ]; then
    log_info "Skipping zlib build as requested"
    return 0
  fi

  if [ -d "${INSTALL_LIB_DIR}/include/zlib" ]; then
    log_info "zlib is already installed, skipping build"
    return 0
  fi

  log_info "Preparing to build zlib"

  # Check if zlib archive exists, download if not
  if [ -e ${DOWN_DIR}/${FNAME_ZLIB} ]; then
    log_info "Using existing zlib archive"
  else
    log_info "Downloading zlib ${FNAME_ZLIB_DOWN}"
    wget "https://github.com/madler/zlib/releases/download/v1.3.1/zlib131.zip" -O "${DOWN_DIR}/zlib-1.3.1.zip"
  fi

  log_info "Extracting zlib archive"
  unzip -o ${DOWN_DIR}/${FNAME_ZLIB} -d "${DOWN_DIR}"

  # Find the zlib directory
  zlib_dir=$(ls -d ${DOWN_DIR}/zlib* | grep -v zip)

  log_info "Building zlib static library"
  pushd "${zlib_dir}"

  log_info "Configuring zlib build"
  export CFLAGS="-fPIC"
  ./configure --static --prefix="${INSTALL_LIB_DIR}"

  log_info "Compiling zlib (using $CPU_NUM CPU cores)"
  make -j $CPU_NUM

  log_info "Installing zlib"
  make install
  popd

  log_info "zlib build completed successfully"
}

# Build RocketMQ C++ client
build_rocketmq_client() {
  print_section "Building RocketMQ C++ Client"

  log_info "Configuring RocketMQ C++ client build with CMake"

  # Common CMake options
  local cmake_options=(
    -DCMAKE_AR="${AR}"
    -DCMAKE_RANLIB="${RANLIB}"
    -DCMAKE_LINKER="${LINK}"
    -DCMAKE_SYSTEM_PROCESSOR="${CMAKE_SYSTEM_PROCESSOR}"
    -DCMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}"
    -DLibevent_USE_STATIC_LIBS=ON
    -DJSONCPP_USE_STATIC_LIBS=ON
    -DZLIB_USE_STATIC_LIBS=ON
    -DBUILD_ROCKETMQ_STATIC=ON
    -DBUILD_ROCKETMQ_SHARED=OFF
  )

  # Add test options if needed
  if [ $TEST -eq 1 ]; then
    log_info "Enabling unit tests"
    cmake_options+=(-DRUN_UNIT_TEST=ON)

    if [ $CODECOV -eq 1 ]; then
      log_info "Enabling code coverage for tests"
      cmake_options+=(-DCODE_COVERAGE=ON)
    fi
  fi

  # Run CMake configure
  cmake -S "${BASEPATH}" -B "${BUILD_DIR}" "${cmake_options[@]}"

  log_info "Compiling RocketMQ C++ client"
  cmake --build "${BUILD_DIR}"

  # Uncomment if installation is needed
  # log_info "Installing RocketMQ C++ client"
  # cmake --install "${BUILD_DIR}"

  log_info "RocketMQ C++ client build completed successfully"
}

# Build Google Test library
build_googletest() {
  print_section "Building Google Test Library"

  if [ $TEST -eq 0 ]; then
    log_info "Skipping Google Test build as tests are disabled"
    return 0
  fi

  if [ -f "${INSTALL_LIB_DIR}/lib/libgtest.a" ]; then
    log_info "Google Test is already installed, skipping build"
    return 0
  fi

  log_info "Preparing to build Google Test"

  # Check if Google Test archive exists, download if not
  if [ -e ${DOWN_DIR}/${FNAME_GTEST} ]; then
    log_info "Using existing Google Test archive"
  else
    log_info "Downloading Google Test ${FNAME_GTEST_DOWN}"
    wget "https://github.com/abseil/googletest/archive/${FNAME_GTEST_DOWN}" -O "${DOWN_DIR}/googletest-${FNAME_GTEST_DOWN}"
  fi

  log_info "Extracting Google Test archive"
  tar -zxvf ${DOWN_DIR}/${FNAME_GTEST} -C "${DOWN_DIR}"

  # Find the Google Test directory
  gtest_dir=$(ls -d ${DOWN_DIR}/googletest* | grep -v tar)

  log_info "Building Google Test static library"

  log_info "Configuring Google Test build with CMake"
  cmake -S "${gtest_dir}" -B "${gtest_dir}/build" \
    -DCMAKE_AR="${AR}" \
    -DCMAKE_RANLIB="${RANLIB}" \
    -DCMAKE_LINKER="${LINK}" \
    -DCMAKE_CXX_FLAGS=-fPIC \
    -DBUILD_STATIC_LIBS=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_LIB_DIR}"

  log_info "Compiling Google Test"
  cmake --build "${gtest_dir}/build"

  log_info "Installing Google Test"
  cmake --install "${gtest_dir}/build"

  # Handle special cases for library location
  if [ ! -f "${INSTALL_LIB_DIR}/lib/libgtest.a" ]; then
    log_warn "libgtest.a not found in expected location, checking alternative locations"

    if [ -f "${INSTALL_LIB_DIR}/lib64/libgtest.a" ]; then
      log_info "Found libgtest.a in lib64, copying to lib/"
      cp ${INSTALL_LIB_DIR}/lib64/lib* "${INSTALL_LIB_DIR}/lib"
    fi
  fi

  log_info "Google Test build completed successfully"
}

# Run unit tests
run_tests() {
  print_section "Running Unit Tests"

  if [ $TEST -eq 0 ]; then
    log_info "Unit tests are disabled, skipping test execution"
    return 0
  fi

  log_info "Starting unit test execution"
  cd "${BUILD_DIR}"

  log_info "Running tests with CTest"
  ctest -V

  log_info "Unit tests completed"
}

# Check CMake version before proceeding
check_cmake_version

# Print build configuration
print_build_params

# Prepare the build environment
prepare_environment

# Build dependencies
build_spdlog
build_libevent
build_jsoncpp
build_zlib
build_googletest

# Build RocketMQ C++ client
build_rocketmq_client

# Run unit tests if enabled
run_tests

log_info "RocketMQ C++ client build process completed"
