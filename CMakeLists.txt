# CMake Build Script for the Space Visualiser project
cmake_minimum_required(VERSION 3.16)

set(SV_BUILD_TYPES "Debug" "Release")
if(CMAKE_CONFIGURATION_TYPES)
  message(NOTICE "---Multiconfiguration build generator selected---")
elseif(NOT CMAKE_BUILD_TYPE IN_LIST SV_BUILD_TYPES)
  message(FATAL_ERROR
    "Invalid build type '${CMAKE_BUILD_TYPE}'.
    CMAKE_BUILD_TYPE must be any one of the possible values:
    ${SV_BUILD_TYPES}"
  )
endif()

project(SV LANGUAGES CXX)

get_filename_component(SV_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)

set(SV_PLATFORM ${CMAKE_SYSTEM_NAME})
string(TOLOWER ${SV_PLATFORM} SV_PLATFORM)
set(SV_SRC_DIR ${SV_DIR}/src)
set(SV_SHADER_DIR ${SV_DIR}/Shaders)
set(SV_BIN_DIR ${SV_DIR}/bin/${SV_PLATFORM})

# Graphics are provided by the Dahlia engine, built from source alongside Space Visualiser
set(SV_DAHLIA_DIR "${SV_DIR}/../Dahlia" CACHE PATH "Path to the Dahlia graphics engine source")
get_filename_component(SV_DAHLIA_DIR "${SV_DAHLIA_DIR}" REALPATH)
if(NOT EXISTS "${SV_DAHLIA_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "Cannot find Dahlia at \"${SV_DAHLIA_DIR}\". Please specify it with -DSV_DAHLIA_DIR=...")
endif()
message(NOTICE "-- Using Dahlia from \"${SV_DAHLIA_DIR}\"")
add_subdirectory("${SV_DAHLIA_DIR}" "${CMAKE_BINARY_DIR}/Dahlia")

# Planet textures. Overridable at runtime with --data <dir>
set(SV_DATA_DIR "${SV_DIR}/Data Files/Textures" CACHE PATH "Directory containing the planet textures")

include(Projects/cmake/SV.cmake)
