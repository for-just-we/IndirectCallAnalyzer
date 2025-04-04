cmake_minimum_required(VERSION 3.23)
project(IndirectCallAnalyzer)

set(CMAKE_CXX_STANDARD 17)

# 下面2个编译命令非常重要，不要删，不然链接不到LLVMSupport等库
set(CMAKE_CXX_FLAGS "-Wno-error=deprecated-declarations -Wno-deprecated-declarations")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# add -std=gnu++14
set(CMAKE_CXX_EXTENSIONS ON)

set(COMMON_FLAGS "-fPIC -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -O3 -fno-rtti")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS} -O3")

# include LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "LLVM STATUS:
  Version       ${LLVM_VERSION}
  Includes      ${LLVM_INCLUDE_DIRS}
  Libraries     ${LLVM_LIBRARY_DIRS}
  Build type    ${LLVM_BUILD_TYPE}
  Dynamic lib   ${LLVM_LINK_LLVM_DYLIB}"
        )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_DIR}")
include(LLVMConfig)
include(AddLLVM)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
message(STATUS "Looking for headers in: ${LLVM_INCLUDE_DIRS}")

link_directories(${LLVM_LIBRARY_DIRS})
message(STATUS "Looking for libraries in: ${LLVM_LIBRARY_DIRS}")

add_definitions(${LLVM_DEFINITIONS})

include_directories("${CMAKE_SOURCE_DIR}/include")

# Check if LLVM was built generating the single libLLVM.so shared library file or as separate static libraries
if(LLVM_LINK_LLVM_DYLIB)
    message(STATUS "Linking to LLVM dynamic shared library object")
    set(llvm_libs LLVM)
else()
    message(STATUS "Linking to separate LLVM static libraries")
    llvm_map_components_to_libnames(llvm_libs
            bitwriter
            core
            ipo
            irreader
            instcombine
            instrumentation
            target
            linker
            analysis
            scalaropts
            support
            )
endif()

## ZSTD
if(NOT LLVM_LINK_LLVM_DYLIB)
    if(DEFINED ZSTD_PATH)
        message("searching ZSTD in: ${ZSTD_PATH}")
        # If ZSTD_PATH is defined, look for zstd in that directory
        find_library(ZSTD_LIBRARIES NAMES zstd HINTS ${ZSTD_PATH}/lib)
    else()
        # If ZSTD_PATH is not defined, look for zstd in system paths
        find_library(ZSTD_LIBRARIES NAMES zstd)
    endif()
    message("--find zstd lib: ${ZSTD_LIBRARIES}")
    message("--find zstd include: ${ZSTD_INCLUDE_DIRS}")
endif()

add_subdirectory(lib)
add_subdirectory(tools)

install(DIRECTORY include/ DESTINATION include)