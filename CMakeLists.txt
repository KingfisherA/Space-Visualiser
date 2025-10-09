# CMake Build Script for the Space Visualiser project
cmake_minimum_required(VERSION 3.16)

set(SCESS_BUILD_TYPES "Debug" "Release")
if(CMAKE_CONFIGURATION_TYPES)
#  set(CMAKE_CONFIGURATION_TYPES SCESS_BUILD_TYPES)
  message(NOTICE "---Multiconfiguration build generator selected---")
elseif(NOT CMAKE_BUILD_TYPE IN_LIST SCESS_BUILD_TYPES)
  message(FATAL_ERROR
    "Invalid build type '${CMAKE_BUILD_TYPE}'.
    CMAKE_BUILD_TYPE must be any one of the possible values:
    ${SCESS_BUILD_TYPES}"
  )
endif()

project(SV LANGUAGES CXX)

get_filename_component(SV_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)

set(SV_PLATFORM ${CMAKE_SYSTEM_NAME})
string(TOLOWER ${SV_PLATFORM} SV_PLATFORM)
set(SV_SRC_DIR ${SV_DIR}/src)
set(SV_LIB_DIR ${SV_DIR}/Libraries)
set(SV_BIN_DIR ${SV_DIR}/bin/${SV_PLATFORM})

if(NOT SV_GENG_DIR)
    message(NOTICE "-- Graphics Engine directory not set. Setting to default: \"../GraphicsEngine\"")
    get_filename_component(SV_GENG_DIR "../GraphicsEngine" REALPATH)
    if(NOT (EXISTS "${SV_GENG_DIR}"))   
        message(FATAL_ERROR "!! Cannot find GraphicsEngine directory. Please specify with -DSV_GENG_DIR=...")
    else()
        message(NOTICE "-- GraphicsEngine directory successfully bound!")
    endif()
endif()

# find_library(GENG_LIBRARIES GraphicsEngine REQUIRED
#   PATHS "${SV_GENG_DIR}/bin"
#   PATH_SUFFIXES "${SV_PLATFORM}/Release")
# if(NOT (GENG_LIBRARIES STREQUAL "GENG_LIBRARIES-NOTFOUND"))
#     get_filename_component(IMGUI_INCLUDE_DIRS "${SV_GENG_DIR}/include/ImGui" REALPATH)
#     get_filename_component(GENG_INCLUDE_DIRS "${SV_GENG_DIR}/include" REALPATH)

#     if((NOT EXISTS "${IMGUI_INCLUDE_DIRS}") OR (NOT EXISTS "${GENG_INCLUDE_DIRS}"))   
#         message(FATAL_ERROR "!! Cannot find GraphicsEngine include directory and or the GraphicsEngine's ImGui include directory. Please ensure that the include files for ImGui and GraphicsEngine are placed in a directory named 'include' in the folder:  \"${SCESS_GENG_DIR}\" ")
#     else()
#         message(NOTICE "-- Found GraphicsEngine include directories: ${GENG_INCLUDE_DIRS}, ${IMGUI_INCLUDE_DIRS}")
#     endif()

#     message("-- Found GraphicsEngine: ${GENG_LIBRARIES}")
# endif()

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

set(GLEW_USE_STATIC_LIBS true)
find_package(GLEW CONFIG)
if(NOT GLEW_FOUND)
    find_package(GLEW REQUIRED)
    message(NOTICE "Found GLEW with Find.cmake")
endif()

find_library(SOIL2_LIBRARIES SOIL2 REQUIRED
  PATHS "${SV_LIB_DIR}/SOIL2/lib"
  PATH_SUFFIXES "${SV_PLATFORM}/arm64")
set(SOIL2_INCLUDE_DIRS "${SV_LIB_DIR}/SOIL2/src/SOIL2")

message(NOTICE ${GLEW_INCLUDE_DIRS})

include(Projects/cmake/SV-Engine.cmake)
include(Projects/cmake/SV.cmake)