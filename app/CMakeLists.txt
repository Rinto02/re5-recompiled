# re5 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(re5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(RE5_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(re5 WIN32 ${RE5_SOURCES})
else()
    add_executable(re5 ${RE5_SOURCES})
endif()

rexglue_setup_target(re5)
