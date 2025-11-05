cmake_minimum_required(VERSION 3.29)

project(rocketmq)

add_definitions(-DNAPI_VERSION=9)
add_definitions(-DNODE_RUNTIME=node)
add_definitions(-DBUILDING_NODE_EXTENSION)

set(NPX_CMD npx)

execute_process(
        COMMAND ${NPX_CMD} cmake-js print-cmakejs-include
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE CMAKE_JS_INC
)

message(STATUS "CMake.js configurations: INC=${CMAKE_JS_INC}")

include_directories(${CMAKE_JS_INC})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/deps/rocketmq/include)

file(GLOB SOURCE_FILES "src/*.cpp")
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
set_target_properties(${PROJECT_NAME}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
target_link_libraries(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/deps/rocketmq/bin/librocketmq.a)

set(CMAKE_DEPENDS_USE_COMPILER FALSE)
set(CMAKE_SKIP_DEPENDENCY_TRACKING TRUE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MUSL_LIBC_PATH $ENV{MUSL_LIBC_PATH})
    if(NOT MUSL_LIBC_PATH)
        message(FATAL_ERROR "MUSL_LIBC_PATH environment variable is not set")
    endif()
    if(NOT EXISTS "${MUSL_LIBC_PATH}")
        message(FATAL_ERROR "musl libc archive not found at ${MUSL_LIBC_PATH}")
    endif()
    target_link_libraries(${PROJECT_NAME} ${MUSL_LIBC_PATH})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fPIC")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS_NO_SHARED TRUE)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -stdlib=libc++")
endif()
