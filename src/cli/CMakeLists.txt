cmake_minimum_required(VERSION 3.10)

project(libcimbar_cli_exe)

set(SOURCES
    libcimbar_cli.cpp
)

add_executable(libcimbar_cli
    ${SOURCES}
)

target_link_libraries(libcimbar_cli
    libcimbar
    ${CIMBAR_OPENCV_LIBS}
)

install(
    TARGETS libcimbar_cli
    RUNTIME DESTINATION bin
)