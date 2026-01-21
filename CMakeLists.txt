cmake_minimum_required(VERSION 3.26)

project(rocketmq)

add_definitions(-DNAPI_VERSION=9)
add_definitions(-DNODE_RUNTIME=node)
add_definitions(-DBUILDING_NODE_EXTENSION)

option(ROCKETMQ_FORCE_STATIC_MUSL "Link against a provided static musl libc archive" OFF)
option(ROCKETMQ_ENABLE_COVERAGE "Enable coverage flags" OFF)
option(ROCKETMQ_USE_STUB "Use RocketMQ stub library for tests" OFF)

set(NPX_CMD npx)

execute_process(
        COMMAND ${NPX_CMD} cmake-js print-cmakejs-include
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE CMAKE_JS_INC
)

message(STATUS "CMake.js configurations: INC=${CMAKE_JS_INC}")

include_directories(${CMAKE_JS_INC})

if(ROCKETMQ_USE_STUB)
    add_definitions(-DROCKETMQ_USE_STUB=1)
    set(ROCKETMQ_STUB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/test/mocks/rocketmq")
    file(GLOB ROCKETMQ_STUB_SOURCES "${ROCKETMQ_STUB_ROOT}/src/*.cpp")
    add_library(rocketmq_stub STATIC ${ROCKETMQ_STUB_SOURCES})
    set_target_properties(rocketmq_stub PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(rocketmq_stub PUBLIC "${ROCKETMQ_STUB_ROOT}/include")
    include_directories("${ROCKETMQ_STUB_ROOT}/include")
    set(ROCKETMQ_LIB rocketmq_stub)
else()
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/rocketmq/include)
    set(ROCKETMQ_LIB ${CMAKE_CURRENT_SOURCE_DIR}/deps/rocketmq/bin/librocketmq.a)
endif()

file(GLOB SOURCE_FILES "src/*.cpp")
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
set_target_properties(${PROJECT_NAME}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
target_link_libraries(${PROJECT_NAME} ${ROCKETMQ_LIB})

set(CMAKE_DEPENDS_USE_COMPILER FALSE)
set(CMAKE_SKIP_DEPENDENCY_TRACKING TRUE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
    if(ROCKETMQ_FORCE_STATIC_MUSL)
        set(MUSL_LIBC_PATH $ENV{MUSL_LIBC_PATH})
        if(NOT MUSL_LIBC_PATH)
            message(FATAL_ERROR "ROCKETMQ_FORCE_STATIC_MUSL=ON but MUSL_LIBC_PATH is not set")
        endif()
        if(NOT EXISTS "${MUSL_LIBC_PATH}")
            message(FATAL_ERROR "musl libc archive not found at ${MUSL_LIBC_PATH}")
        endif()
        target_link_libraries(${PROJECT_NAME} ${MUSL_LIBC_PATH})
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fPIC")
        set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED TRUE)
    endif()
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -stdlib=libc++")
endif()

if(ROCKETMQ_ENABLE_COVERAGE)
    add_definitions(-DROCKETMQ_COVERAGE=1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fprofile-arcs -ftest-coverage")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()
