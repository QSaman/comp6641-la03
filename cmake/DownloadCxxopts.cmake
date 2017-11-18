# Try to download cxxopts library
# Once done this will define
#
# 1. CXXOPTS_INCLUDE_DIR

set(CXXOPTS_INSTALL_LOCATION "${PROJECT_BINARY_DIR}/external/cxxopts")

include(ExternalProject)
ExternalProject_Add(external_cxxopts
        URL https://github.com/jarro2783/cxxopts/archive/v1.4.3.zip
        CMAKE_ARGS "${CMAKE_ARGS};"
        "-DCMAKE_INSTALL_PREFIX=${CXXOPTS_INSTALL_LOCATION};"
        "-DCXXOPTS_BUILD_EXAMPLES=OFF;"
        "-DCXXOPTS_BUILD_TESTS=OFF;")

                
set(CXXOPTS_INCLUDE_DIR ${CXXOPTS_INSTALL_LOCATION}/include)
