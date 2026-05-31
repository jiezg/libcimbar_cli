cmake_minimum_required(VERSION 3.10)

project(libcimbar_cli)

set(DISABLE_TESTS ON)

if(MSVC)
    add_compile_options(/FI "iso646.h")
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${libcimbar_cli_SOURCE_DIR}/dist" CACHE PATH "..." FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)

set(libcimbar_ref_SOURCE_DIR "${libcimbar_cli_SOURCE_DIR}/../libcimbar_ref" CACHE PATH "Path to libcimbar_ref source")

add_definitions("-DLIBCIMBAR_PROJECT_ROOT=\"${libcimbar_ref_SOURCE_DIR}\"")

if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif()

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# 优先使用 find_package 提供的变量
if(OpenCV_LIBS)
    set(CIMBAR_OPENCV_LIBS ${OpenCV_LIBS})
elseif(OpenCV_LIBRARIES)
    set(CIMBAR_OPENCV_LIBS ${OpenCV_LIBRARIES})
else()
    # 回退：手动指定库名，find_library 会自动搜索
    set(CIMBAR_OPENCV_LIBS opencv_calib3d opencv_features2d opencv_flann opencv_imgcodecs opencv_imgproc opencv_photo opencv_core)
endif()

if(MINGW AND OpenCV_DIR)
    # MinGW 下确保能找到 OpenCV 的导入库
    link_directories("${OpenCV_DIR}/x64/mingw/lib" "${OpenCV_DIR}/lib")
endif()

set(CPPFILESYSTEM "")

set (PROJECTS
    ${libcimbar_ref_SOURCE_DIR}/src/lib/bit_file
    ${libcimbar_ref_SOURCE_DIR}/src/lib/chromatic_adaptation
    ${libcimbar_ref_SOURCE_DIR}/src/lib/cimb_translator
    ${libcimbar_ref_SOURCE_DIR}/src/lib/compression
    ${libcimbar_ref_SOURCE_DIR}/src/lib/encoder
    ${libcimbar_ref_SOURCE_DIR}/src/lib/extractor
    ${libcimbar_ref_SOURCE_DIR}/src/lib/fountain
    ${libcimbar_ref_SOURCE_DIR}/src/lib/serialize
    ${libcimbar_ref_SOURCE_DIR}/src/lib/util

    ${libcimbar_ref_SOURCE_DIR}/src/third_party_lib/base91
    ${libcimbar_ref_SOURCE_DIR}/src/third_party_lib/cxxopts
    ${libcimbar_ref_SOURCE_DIR}/src/third_party_lib/intx
    ${libcimbar_ref_SOURCE_DIR}/src/third_party_lib/libcorrect
    ${libcimbar_ref_SOURCE_DIR}/src/third_party_lib/libpopcnt
    ${libcimbar_ref_SOURCE_DIR}/src/third_party_lib/wirehair
    ${libcimbar_ref_SOURCE_DIR}/src/third_party_lib/zstd
)

set(PROJECTS
    ${PROJECTS}

    src/dll
    src/cli
)

include_directories(
    ${libcimbar_ref_SOURCE_DIR}/src/lib
    ${libcimbar_ref_SOURCE_DIR}/src/third_party_lib
    ${libcimbar_cli_SOURCE_DIR}/src/dll
)

foreach(proj ${PROJECTS})
    # 取目录最后一级作为二进制输出目录名，避免 Windows 路径问题
    get_filename_component(proj_abs ${proj} ABSOLUTE)
    get_filename_component(bin_dir ${proj_abs} NAME)
    # 附加父目录前缀避免重名
    get_filename_component(proj_parent ${proj_abs} DIRECTORY)
    get_filename_component(parent_name ${proj_parent} NAME)
    set(bin_dir "${parent_name}_${bin_dir}")
    add_subdirectory(${proj} build/${bin_dir})
endforeach()