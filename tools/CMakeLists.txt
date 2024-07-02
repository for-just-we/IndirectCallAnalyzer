
add_executable(IndirectCallAnalyzer ICA.cpp)
target_link_libraries(IndirectCallAnalyzer MltaLib ${llvm_libs} ${ZSTD_LIBRARY})