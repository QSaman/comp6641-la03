# Try to download and compile LUrlParser library
# Once done this will define
#
# 1. LURLPARSER_INCLUDE_DIR
# 2. LURLPARSER_LIBRARIES

set(LURLPARSER_INSTALL_LOCATION "${PROJECT_BINARY_DIR}/external/LURLPARSER")

include(ExternalProject)

ExternalProject_Add(external_lurlparser
URL https://github.com/QSaman/LUrlParser/archive/master.zip
CMAKE_ARGS "${CMAKE_ARGS};"
        "-DCMAKE_INSTALL_PREFIX=${LURLPARSER_INSTALL_LOCATION};"
)

set(LURLPARSER_INCLUDE_DIR ${LURLPARSER_INSTALL_LOCATION}/include)

add_library(lurlparser STATIC IMPORTED)
add_dependencies(lurlparser external_lurlparser)
set(lib_name ${CMAKE_STATIC_LIBRARY_PREFIX}LUrlParser${CMAKE_STATIC_LIBRARY_SUFFIX})
set_property(TARGET lurlparser PROPERTY IMPORTED_LOCATION ${LURLPARSER_INSTALL_LOCATION}/lib/${lib_name})
set(LURLPARSER_LIBRARIES lurlparser)