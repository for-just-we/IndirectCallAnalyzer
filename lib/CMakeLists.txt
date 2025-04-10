file (GLOB SOURCES Passes/*.cpp Utils/*.cpp)

message("source ${SOURCES}")
link_directories(${LLVM_LIBRARY_DIRS})
add_library(MltaLib SHARED ${SOURCES})

target_include_directories(MltaLib PRIVATE "${CMAKE_SOURCE_DIR}/include")
if(DEFINED ZSTD_LIBRARIES)
    message("linking MltaLib to: ${llvm_libs}, ${ZSTD_LIBRARIES}")
    target_link_libraries(MltaLib ${llvm_libs} ${ZSTD_LIBRARIES})
else()
    message("linking MltaLib to: ${llvm_libs}")
    target_link_libraries(MltaLib ${llvm_libs})
endif()

install(TARGETS MltaLib LIBRARY DESTINATION ${CMAKE_BINARY_DIR}/lib)