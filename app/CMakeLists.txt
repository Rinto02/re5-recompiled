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

if(ANDROID)
    add_library(re5 SHARED ${RE5_SOURCES})
    set_target_properties(re5 PROPERTIES OUTPUT_NAME "main")
elseif(WIN32)
    add_executable(re5 WIN32 ${RE5_SOURCES})
else()
    add_executable(re5 ${RE5_SOURCES})
endif()

rexglue_setup_target(re5)

# rex_app.cpp (compiled into this target) includes imgui.h, but rexruntime links imgui privately, so its
# include path never reaches this target. Take the path alone: imgui's objects are already inside rexruntime.
if(TARGET imgui)
    target_include_directories(re5 PRIVATE $<TARGET_PROPERTY:imgui,INTERFACE_INCLUDE_DIRECTORIES>)
endif()
