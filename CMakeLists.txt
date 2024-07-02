cmake_minimum_required(VERSION 3.23)
project(IndirectCallAnalyzer)

set(CMAKE_CXX_STANDARD 14)

# 下面2个编译命令非常重要，不要删，不然链接不到LLVMSupport等库
set(CMAKE_CXX_FLAGS "-Wno-error=deprecated-declarations -Wno-deprecated-declarations")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# add -std=gnu++14
set(CMAKE_CXX_EXTENSIONS ON)

set(COMMON_FLAGS "-fPIC -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -O3 -fno-rtti")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS} -O3")

# include LLVM
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLVM binaries: ${LLVM_TOOLS_BINARY_DIR}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}")
include(LLVMConfig)
include(AddLLVM)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
message(STATUS "Looking for headers in: ${LLVM_INCLUDE_DIRS}")

link_directories(${LLVM_LIBRARY_DIRS})
message(STATUS "Looking for libraries in: ${LLVM_LIBRARY_DIRS}")

add_definitions(${LLVM_DEFINITIONS})

include_directories("${CMAKE_SOURCE_DIR}/include")
llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support transformutils)

## ZSTD
find_library(ZSTD_LIBRARY NAMES zstd PATHS ${ZSTD_PATH})
message("-- ZSTD: ${ZSTD_LIBRARY}")

add_subdirectory(lib)
add_subdirectory(tools)