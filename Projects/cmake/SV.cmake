add_executable("SV"
  "${SV_SRC_DIR}/SV/main.cpp" 
  "${SV_SRC_DIR}/SV/Atmosphere/Atmosphere.hpp" 
  "${SV_SRC_DIR}/SV/Atmosphere/AtmospherePrecomputer.hpp" 
  "${SV_SRC_DIR}/SV/Atmosphere/AtmospherePrecomputer.cpp" 
  "${SV_SRC_DIR}/SV/Atmosphere/Atmosphere.cpp" 
  "${SV_SRC_DIR}/SV/Application/Application.hpp" 
  "${SV_SRC_DIR}/SV/Application/Application.cpp" 
  "${SV_SRC_DIR}/SV/Universe/Planetary System.h" 
  "${SV_SRC_DIR}/SV/Universe/Star.cpp" 
  "${SV_SRC_DIR}/SV/Universe/Planet.hpp" 
  "${SV_SRC_DIR}/SV/Universe/Planetary System.cpp" 
  "${SV_SRC_DIR}/SV/Universe/PlanetNew.cpp" 
  "${SV_SRC_DIR}/SV/Universe/PlanetNew.hpp" 
  "${SV_SRC_DIR}/SV/Universe/Star.hpp" 
  "${SV_SRC_DIR}/SV/Universe/Planet.cpp" 
  "${SV_SRC_DIR}/SV/Universe/CelestrialBody.hpp" 
)

add_dependencies("SV"
    "SV-Engine"
)

target_include_directories(SV PRIVATE
  ${SV_SRC_DIR}
  ${GLM_INCLUDE_DIRS}
  ${GLEW_INCLUDE_DIRS}
  ${SOIL2_INCLUDE_DIRS}
)

target_compile_definitions("SV" PRIVATE
  GLM_ENABLE_EXPERIMENTAL
)

target_link_libraries(SV PRIVATE
  SV-Engine
  glfw
  ${SOIL2_LIBRARIES}
  OpenGL::GL
  $<IF:$<TARGET_EXISTS:GLEW::glew_s>,GLEW::glew_s,GLEW::GLEW>
  #Add check for MacOS here
  $<LINK_LIBRARY:FRAMEWORK,AppKit,Quartz>
)

set_target_properties(SV PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE True
    INTERPROCEDURAL_OPTIMIZATION False
    MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>DLL
    OUTPUT_NAME "SV")

set(COMPILE_OPTIONS)

if((CMAKE_CXX_COMPILER_ID STREQUAL Clang) OR 
   (CMAKE_CXX_COMPILER_ID STREQUAL GNU))
    list(APPEND COMPILE_OPTIONS
      -fPIC
      -std=c++20
      -arch arm64
    )

    if(CMAKE_BUILD_TYPE STREQUAL Debug)
      list(APPEND COMPILE_OPTIONS
        -g
        -O0
      )
    elseif(CMAKE_BUILD_TYPE STREQUAL Release)
      list(APPEND COMPILE_OPTIONS
        -O3
      )
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL MSVC)
  list(APPEND COMPILE_OPTIONS
      /std:c++20
    )
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
      list(APPEND COMPILE_OPTIONS
        /Zi
        /Od
      )
    elseif(CMAKE_BUILD_TYPE STREQUAL Release)
      list(APPEND COMPILE_OPTIONS /O2)
    endif()
endif()

target_compile_options(SV PRIVATE ${COMPILE_OPTIONS})


if(CMAKE_CONFIGURATION_TYPES)
  set(DEBUG_BIN_DIR "${SV_BIN_DIR}")
  set(RELEASE_BIN_DIR "${SV_BIN_DIR}")
else()
  set(DEBUG_BIN_DIR "${SV_BIN_DIR}/Debug")
  set(RELEASE_BIN_DIR "${SV_BIN_DIR}/Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set_target_properties(SV PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${DEBUG_BIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${DEBUG_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${DEBUG_BIN_DIR}"
  )
elseif(CMAKE_BUILD_TYPE STREQUAL Release)
  set_target_properties(SV PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${RELEASE_BIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${RELEASE_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${RELEASE_BIN_DIR}"
  )
endif()
