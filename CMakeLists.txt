set(CMAKE_MINIMUM_VERSION 3.1)
cmake_minimum_required(VERSION ${CMAKE_MINIMUM_VERSION})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(InitHunter)

project(httpfs)
set(HTTPC_LIB_NAME "http_client")
set(HTTPFS_LIB_NAME "http_file_server")
set(HTTPC_NAME "httpc")

include(ucm)
include(DefaultOptions)

hunter_add_package(Boost COMPONENTS system filesystem thread)
find_package(Boost CONFIG REQUIRED system filesystem thread)

hunter_add_package(nlohmann_json)
find_package(nlohmann_json CONFIG REQUIRED)

include(DownloadAsio)
include_directories(${ASIO_INCLUDE_DIR})

include(DownloadCxxopts)
include_directories(${CXXOPTS_INCLUDE_DIR})

include(DownloadLUrlParser)
include_directories(${LURLPARSER_INCLUDE_DIR})
set(LIBRARIES ${LURLPARSER_LIBRARIES})

#The following variable requires CMake 3.1
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

ucm_add_dirs(src/libhttpc TO LIBHTTPC_SRC_LIST)
add_library(${HTTPC_LIB_NAME} STATIC ${LIBHTTPC_SRC_LIST})
add_dependencies(${HTTPC_LIB_NAME} external_asio)
target_link_libraries(${HTTPC_LIB_NAME} Threads::Threads ${LURLPARSER_LIBRARIES})

ucm_add_dirs(src/httpc TO HTTPC_SRC_LIST)
add_executable(${HTTPC_NAME} ${HTTPC_SRC_LIST})
add_dependencies(${HTTPC_NAME} external_cxxopts)
target_link_libraries(${HTTPC_NAME} ${LURLPARSER_LIBRARIES} ${HTTPC_LIB_NAME})

ucm_add_dirs(src/libhttpfs TO LIBHTTPFS_SRC_LIST)
add_library(${HTTPFS_LIB_NAME} STATIC ${LIBHTTPFS_SRC_LIST})
target_link_libraries(${HTTPFS_LIB_NAME} Boost::system Boost::filesystem nlohmann_json Boost::thread ${HTTPC_LIB_NAME})

ucm_add_dirs(src/httpfs TO HTTPFS_SRC_LIST)
add_executable(${PROJECT_NAME} ${HTTPFS_SRC_LIST})
add_dependencies(${PROJECT_NAME} external_cxxopts)
target_link_libraries(${PROJECT_NAME} PUBLIC ${LURLPARSER_LIBRARIES} Boost::system Boost::filesystem nlohmann_json ${HTTPFS_LIB_NAME})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/resources" "${PROJECT_BINARY_DIR}/resources")

add_executable(partial_data test/partial_data.cpp)
target_link_libraries(partial_data Threads::Threads)

#install(TARGETS ${PROJECT_NAME} DESTINATION "bin")
#install(DIRECTORY "${PROJECT_SOURCE_DIR}/sample_files" DESTINATION "bin")