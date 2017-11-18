# includ this file before project(...)

cmake_minimum_required(VERSION 3.0)

if (DEFINED ENV{HUNTER_ROOT})
    message(STATUS "Environment variable HUNTER_ROOT is set to $ENV{HUNTER_ROOT}.")
else()
    message(STATUS "Environment variable HUNTER_ROOT is not set. I'm Trying to use ${CMAKE_SOURCE_DIR}/hunter_repo")
    set(HUNTER_ROOT ${CMAKE_SOURCE_DIR}/hunter_repo)
endif ()

include(HunterGate)
HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.19.122.tar.gz"
    SHA1 "9bbe5e7fcfb52edecc0251f819d5b995fa069c28"
)
