cmake_minimum_required(VERSION 3.5.0)
project(SpectrePass)

set(DEFAULT_LLVM_DIR "../llvm-project-14.0.0.src/build/lib/cmake/llvm")
set(LLVM_DIR ${DEFAULT_LLVM_DIR} CACHE PATH "Path to LLVMConfig.cmake")

message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")


option(DEBUG_LOG "to print every instruction's state" ON)
option(STAT_INFO "to stat lightslh's analysis info" ON)

if(DEBUG_LOG)
    add_definitions(-DDEBUG_LOG)
endif()

if(STAT_INFO)
    add_definitions(-DSTAT_INFO)
endif()

include_directories(include)
add_definitions(-DLLVM_DISABLE_ABI_BREAKING_CHECKS_ENFORCING=1)

file(GLOB SOURCES "src/*.cpp")
add_compile_options(-fPIC)
find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

add_library(SpectrePass SHARED ${SOURCES})
set_target_properties(SpectrePass PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)

