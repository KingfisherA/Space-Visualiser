add_executable("SV"
  "${SV_SRC_DIR}/SV/main.cpp"
  "${SV_SRC_DIR}/SV/Application/Application.hpp"
  "${SV_SRC_DIR}/SV/Application/Application.cpp"
  "${SV_SRC_DIR}/SV/Atmosphere/Atmosphere.hpp"
  "${SV_SRC_DIR}/SV/Atmosphere/Atmosphere.cpp"
  "${SV_SRC_DIR}/SV/Atmosphere/AtmospherePrecomputer.hpp"
  "${SV_SRC_DIR}/SV/Atmosphere/AtmospherePrecomputer.cpp"
  "${SV_SRC_DIR}/SV/Graphics/AsyncTexture.hpp"
  "${SV_SRC_DIR}/SV/Graphics/AsyncTexture.cpp"
  "${SV_SRC_DIR}/SV/Graphics/ModalPipeline.hpp"
  "${SV_SRC_DIR}/SV/Graphics/ModalPipeline.cpp"
  "${SV_SRC_DIR}/SV/Graphics/SceneContext.hpp"
  "${SV_SRC_DIR}/SV/Graphics/ShaderSources.hpp"
  "${SV_SRC_DIR}/SV/Graphics/ShaderSources.cpp"
  "${SV_SRC_DIR}/SV/Scene/Transformation.hpp"
  "${SV_SRC_DIR}/SV/Universe/CelestrialBody.hpp"
  "${SV_SRC_DIR}/SV/Universe/Planetary System.h"
  "${SV_SRC_DIR}/SV/Universe/Planetary System.cpp"
  "${SV_SRC_DIR}/SV/Universe/Planet.hpp"
  "${SV_SRC_DIR}/SV/Universe/Planet.cpp"
  "${SV_SRC_DIR}/SV/Universe/Star.hpp"
  "${SV_SRC_DIR}/SV/Universe/Star.cpp"
  "${SV_SRC_DIR}/SV/Universe/Rings.hpp"
  "${SV_SRC_DIR}/SV/Universe/Rings.cpp"
  "${SV_SRC_DIR}/SV/Universe/SolarSystem.hpp"
  "${SV_SRC_DIR}/SV/Universe/SolarSystem.cpp"
  "${SV_SRC_DIR}/SV/Platform/Trackpad.hpp"
)

# Trackpad scrolling and pinch gestures (AppKit)
if(APPLE)
  target_sources("SV" PRIVATE "${SV_SRC_DIR}/SV/Platform/Trackpad.mm")
  set_source_files_properties("${SV_SRC_DIR}/SV/Platform/Trackpad.mm" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_link_libraries("SV" PRIVATE $<LINK_LIBRARY:FRAMEWORK,AppKit>)
endif()

# Shaders are compiled to SPIR-V at build time and embedded (see src/SV/Graphics/ShaderSources.cpp)
dahlia_compile_shaders("SV"
  INCLUDE_DIRS "${SV_SHADER_DIR}/Include"
  SOURCES
    "${SV_SHADER_DIR}/Grid.vert"
    "${SV_SHADER_DIR}/Grid.frag"
    "${SV_SHADER_DIR}/Star.vert"
    "${SV_SHADER_DIR}/Star.frag"
    "${SV_SHADER_DIR}/Corona.vert"
    "${SV_SHADER_DIR}/Corona.frag"
    "${SV_SHADER_DIR}/Flare.vert"
    "${SV_SHADER_DIR}/Flare.frag"
    "${SV_SHADER_DIR}/Rings.vert"
    "${SV_SHADER_DIR}/Rings.frag"
    "${SV_SHADER_DIR}/Planet.vert"
    "${SV_SHADER_DIR}/Planet.frag"
    "${SV_SHADER_DIR}/Precompute/Fullscreen.vert"
    "${SV_SHADER_DIR}/Precompute/Transmittance.frag"
    "${SV_SHADER_DIR}/Precompute/DirectIrradiance.frag"
    "${SV_SHADER_DIR}/Precompute/SingleScattering.frag"
    "${SV_SHADER_DIR}/Precompute/ScatteringDensity.frag"
    "${SV_SHADER_DIR}/Precompute/IndirectIrradiance.frag"
    "${SV_SHADER_DIR}/Precompute/MultipleScattering.frag"
)

target_include_directories("SV" PRIVATE
  ${SV_SRC_DIR}
)

target_compile_definitions("SV" PRIVATE
  SV_DEFAULT_DATA_DIR="${SV_DATA_DIR}"
  $<$<CONFIG:Debug>:CONFIG_DEBUG>
  $<$<CONFIG:Release>:CONFIG_RELEASE>
)

# Dahlia's public headers bring in glm and ImGui. ImGui is linked here too as the app calls it
# directly (the context is shared with Dahlia, see GRAPHICS_APP_INIT_IMGUI_CTX).
target_link_libraries("SV" PRIVATE
  Dahlia
  ImGui
  DahliaTools
)

set_target_properties("SV" PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE True
    INTERPROCEDURAL_OPTIMIZATION False
    MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>DLL
    OUTPUT_NAME "SV")

set(COMPILE_OPTIONS)

if((CMAKE_CXX_COMPILER_ID MATCHES "Clang") OR
   (CMAKE_CXX_COMPILER_ID STREQUAL GNU))
    list(APPEND COMPILE_OPTIONS
      -fPIC
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
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
      list(APPEND COMPILE_OPTIONS
        /Zi
        /Od
      )
    elseif(CMAKE_BUILD_TYPE STREQUAL Release)
      list(APPEND COMPILE_OPTIONS /O2)
    endif()
endif()

target_compile_options("SV" PRIVATE ${COMPILE_OPTIONS})

if(CMAKE_CONFIGURATION_TYPES)
  set(DEBUG_BIN_DIR "${SV_BIN_DIR}")
  set(RELEASE_BIN_DIR "${SV_BIN_DIR}")
else()
  set(DEBUG_BIN_DIR "${SV_BIN_DIR}/Debug")
  set(RELEASE_BIN_DIR "${SV_BIN_DIR}/Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set_target_properties("SV" PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${DEBUG_BIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${DEBUG_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${DEBUG_BIN_DIR}"
  )
elseif(CMAKE_BUILD_TYPE STREQUAL Release)
  set_target_properties("SV" PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${RELEASE_BIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${RELEASE_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${RELEASE_BIN_DIR}"
  )
endif()
