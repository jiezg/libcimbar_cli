cmake_minimum_required(VERSION 3.10)

project(libcimbar_dll)

set(SOURCES
    libcimbar_export.h
    libcimbar_encode.cpp
    libcimbar_decode.cpp
)

add_library(libcimbar SHARED
    ${SOURCES}
)

set_target_properties(libcimbar PROPERTIES 
    PREFIX ""
    OUTPUT_NAME "libcimbar"
)

target_compile_definitions(libcimbar PRIVATE LIBCIMBAR_DLL_EXPORTS)

target_link_libraries(libcimbar
    cimb_translator
    extractor
    correct_static
    wirehair
    zstd
    ${CIMBAR_OPENCV_LIBS}
)

install(
    TARGETS libcimbar
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)