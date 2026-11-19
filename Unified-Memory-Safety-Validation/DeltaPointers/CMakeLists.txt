cmake_minimum_required(VERSION 3.16)
project(DeltaPointers)

set(ADDRSPACE_BITS 32)

set(DEFAULT_LLVM_DIR "../../llvm-project-14.0.0.src/build/lib/cmake/llvm")
set(LLVM_DIR ${DEFAULT_LLVM_DIR} CACHE PATH "Path to LLVMConfig.cmake")

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LT_LLVM_INSTALL_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Use the same C++ standard as LLVM does
set(CMAKE_CXX_STANDARD 14 CACHE STRING "")

# LLVM is normally built without RTTI. Be consistent with that.
if (NOT LLVM_ENABLE_RTTI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif ()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall\
    -fdiagnostics-color=always -Wno-everything -g ")

# Infra builtin sources
include_directories("passes/include")

set(INFRA_SRCS
    "passes/Common.cpp"
    "passes/Allocation.cpp"
    "passes/SizeofTypes.cpp"
    "passes/CustomInliner.cpp"
    "passes/DeferGlobalInit.cpp"
    "passes/ExpandConstGlobalUses.cpp"
)


set(PASS_SRCS
    "passes/AddressSpace.cpp"
    "passes/CheckAddressSpace.cpp"
    "passes/DebugNegArith.cpp"
    "passes/DeltaTagAlloc.cpp"
    "passes/DeltaTagProp.cpp"
    "passes/GlobalOpt.cpp"
    "passes/MagicTags.cpp"
    "passes/MaskPointers.cpp"
    "passes/ReinterpretedPointers.cpp"
    "passes/ReplaceAddressTakenMalloc.cpp"
    "passes/RuntimeStats.cpp"
    "passes/SafeAllocs.cpp"
    "passes/SafeAllocsOld.cpp"
    "passes/TagGlobals.cpp"
    "passes/TagGlobalsConst.cpp"
    "passes/UboundBranch.cpp"
)

add_library(deltapointers MODULE
    ${PASS_SRCS}
    ${INFRA_SRCS}
)

target_compile_options(deltapointers PRIVATE
    -fno-rtti
    -Wall
    -Wno-unused-variable
)

set(LIBSHRINK_SRCS
    "libshrink/shrink.c"
    "libshrink/libstatic.c"
    "libshrink/linux_syscall_support.h"
    "libshrink/shrink.h"
)

add_library(shrink_rt STATIC ${LIBSHRINK_SRCS})

set_target_properties(shrink_rt PROPERTIES
        COMPILE_FLAGS "-fpic -Wall -Wextra -g -O2 -DADDRSPACE_BITS=${ADDRSPACE_BITS}"
        )
