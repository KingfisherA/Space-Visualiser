# Build script for Space Visualiser
#
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build                  # build/SV/SV
#   cmake --build build --target SVApp   # build/SV/Space Visualiser.app (macOS)
#
# The Dahlia graphics engine is built from source alongside Space Visualiser, into build/Dahlia.

# 3.24 for $<LINK_LIBRARY:FRAMEWORK,...>, as Dahlia
cmake_minimum_required(VERSION 3.24...4.0)

project(SpaceVisualiser VERSION 1.0.0 LANGUAGES C CXX)
if(APPLE)
  # Trackpad support (src/SV/Platform/Trackpad.mm) and Dahlia's Metal backend are Objective-C++
  enable_language(OBJCXX)
endif()

# ---- Build type -----------------------------------------------------------------------------------
get_property(SV_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
set(SV_BUILD_TYPES Debug Release)
if(SV_MULTI_CONFIG)
  set(CMAKE_CONFIGURATION_TYPES ${SV_BUILD_TYPES} CACHE STRING "" FORCE)
else()
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
  endif()
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${SV_BUILD_TYPES})
  if(NOT CMAKE_BUILD_TYPE IN_LIST SV_BUILD_TYPES)
    message(FATAL_ERROR "CMAKE_BUILD_TYPE must be one of: ${SV_BUILD_TYPES} (got '${CMAKE_BUILD_TYPE}')")
  endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Options --------------------------------------------------------------------------------------
set(SV_DAHLIA_DIR "${PROJECT_SOURCE_DIR}/../Dahlia" CACHE PATH "Path to the Dahlia graphics engine source")
# Planet textures. Overridable at runtime with --data <dir>
set(SV_DATA_DIR "${PROJECT_SOURCE_DIR}/Data Files/Textures" CACHE PATH "Directory containing the planet textures")

# ---- Dahlia ---------------------------------------------------------------------------------------
get_filename_component(SV_DAHLIA_DIR "${SV_DAHLIA_DIR}" REALPATH)
if(NOT EXISTS "${SV_DAHLIA_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "Cannot find Dahlia at \"${SV_DAHLIA_DIR}\". Please specify it with -DSV_DAHLIA_DIR=...")
endif()
message(STATUS "Using Dahlia from \"${SV_DAHLIA_DIR}\"")
add_subdirectory("${SV_DAHLIA_DIR}" Dahlia)

# ---- Space Visualiser -----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(SVEmbedFile)

add_subdirectory(src/SV SV)
