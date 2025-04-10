
add_executable(ica ICA.cpp)
target_link_libraries(ica MltaLib ${llvm_libs} ${ZSTD_LIBRARIES})
set_target_properties(ica
        PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;${LLVM_LIBRARY_DIRS}"
        )
install(TARGETS ica
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )