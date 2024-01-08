file (GLOB SOURCES *.cpp)

message("source ${SOURCES}")
link_directories(${LLVM_LIBRARY_DIRS})

add_library(MltaLib SHARED ${SOURCES})

target_include_directories(MltaLib PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include")
target_link_libraries(MltaLib
        ${llvm_libs}
        ${ZSTD_LIBRARY}
        )

install(TARGETS MltaLib LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/lib)