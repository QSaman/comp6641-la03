# Try to download Asio library
# Once done this will define
#
# 1. ASIO_INCLUDE_DIR

set(ASIO_INSTALL_LOCATION "${PROJECT_BINARY_DIR}/external/asio")

add_definitions(-DASIO_STANDALONE)

include(ExternalProject)
ExternalProject_Add(external_asio
        URL https://github.com/chriskohlhoff/asio/archive/asio-1-11-0.zip
        INSTALL_DIR "${ASIO_INSTALL_LOCATION}"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory  "<SOURCE_DIR>/asio/include" "${ASIO_INSTALL_LOCATION}/include")

                
set(ASIO_INCLUDE_DIR ${ASIO_INSTALL_LOCATION}/include)
