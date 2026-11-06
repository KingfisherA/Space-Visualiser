# Space Visualiser, built into build/SV
#
#   SV      plain executable (default build)
#   SVApp   macOS only: "Space Visualiser.app" with the Space Visualiser icon and libDahlia embedded
#           (cmake --build build --target SVApp)

set(SV_SHADER_DIR ${PROJECT_SOURCE_DIR}/Shaders)
set(SV_ICON_DIR ${PROJECT_SOURCE_DIR}/Icons)

#   sv_add_executable(<target> [add_executable options and extra sources...])
function(sv_add_executable TARGET)
  add_executable(${TARGET} ${ARGN}
    main.cpp
    Application/Application.cpp
    Application/Application.hpp
    Application/SplashScreen.cpp
    Application/SplashScreen.hpp
    Atmosphere/Atmosphere.cpp
    Atmosphere/Atmosphere.hpp
    Atmosphere/AtmospherePrecomputer.cpp
    Atmosphere/AtmospherePrecomputer.hpp
    Graphics/AsyncTexture.cpp
    Graphics/AsyncTexture.hpp
    Graphics/ModalPipeline.cpp
    Graphics/ModalPipeline.hpp
    Graphics/SceneContext.hpp
    Graphics/ShaderSources.cpp
    Graphics/ShaderSources.hpp
    Platform/Trackpad.hpp
    Scene/Transformation.hpp
    Universe/CelestrialBody.hpp
    Universe/Coordinates.hpp
    Universe/Galaxy.cpp
    Universe/Galaxy.hpp
    Universe/Moon.cpp
    Universe/Moon.hpp
    Universe/MoonData.cpp
    Universe/MoonData.hpp
    Universe/Planet.cpp
    Universe/Planet.hpp
    Universe/PlanetarySystem.cpp
    Universe/PlanetarySystem.hpp
    Universe/Rings.cpp
    Universe/Rings.hpp
    Universe/SolarSystem.cpp
    Universe/SolarSystem.hpp
    Universe/Star.cpp
    Universe/Star.hpp
  )

  # Trackpad scrolling and pinch gestures (AppKit)
  if(APPLE)
    target_sources(${TARGET} PRIVATE Platform/Trackpad.mm)
    set_source_files_properties(Platform/Trackpad.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    target_link_libraries(${TARGET} PRIVATE $<LINK_LIBRARY:FRAMEWORK,AppKit>)
  endif()

  # Shaders are compiled to SPIR-V at build time and embedded (see Graphics/ShaderSources.cpp)
  dahlia_compile_shaders(${TARGET}
    INCLUDE_DIRS ${SV_SHADER_DIR}/Include
    SOURCES
      ${SV_SHADER_DIR}/Grid.vert
      ${SV_SHADER_DIR}/Grid.frag
      ${SV_SHADER_DIR}/Star.vert
      ${SV_SHADER_DIR}/Star.frag
      ${SV_SHADER_DIR}/Corona.vert
      ${SV_SHADER_DIR}/Corona.frag
      ${SV_SHADER_DIR}/Flare.vert
      ${SV_SHADER_DIR}/Flare.frag
      ${SV_SHADER_DIR}/Rings.vert
      ${SV_SHADER_DIR}/Rings.frag
      ${SV_SHADER_DIR}/Planet.vert
      ${SV_SHADER_DIR}/Planet.frag
      ${SV_SHADER_DIR}/GalaxyGlow.vert
      ${SV_SHADER_DIR}/GalaxyGlow.frag
      ${SV_SHADER_DIR}/GalaxyStars.vert
      ${SV_SHADER_DIR}/GalaxyStars.frag
      ${SV_SHADER_DIR}/GalaxyComposite.frag
      ${SV_SHADER_DIR}/Precompute/Fullscreen.vert
      ${SV_SHADER_DIR}/Precompute/Transmittance.frag
      ${SV_SHADER_DIR}/Precompute/DirectIrradiance.frag
      ${SV_SHADER_DIR}/Precompute/SingleScattering.frag
      ${SV_SHADER_DIR}/Precompute/ScatteringDensity.frag
      ${SV_SHADER_DIR}/Precompute/IndirectIrradiance.frag
      ${SV_SHADER_DIR}/Precompute/MultipleScattering.frag
  )

  # The splash screen's icon (see Application/SplashScreen.cpp)
  sv_embed_files(${TARGET} "${SV_ICON_DIR}/SpaceVisualiser Icon.png")

  target_include_directories(${TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_compile_definitions(${TARGET} PRIVATE
    SV_VERSION="${PROJECT_VERSION}"
    SV_DEFAULT_DATA_DIR="${SV_DATA_DIR}"
  )
  target_compile_options(${TARGET} PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
  )

  # Dahlia's public headers bring in glm and ImGui. ImGui is linked here too as the app calls it
  # directly (the context is shared with Dahlia, see GRAPHICS_APP_INIT_IMGUI_CTX).
  target_link_libraries(${TARGET} PRIVATE
    Dahlia
    ImGui
    DahliaTools
  )
  dahlia_target_defaults(${TARGET})
endfunction()

# ---- SV -------------------------------------------------------------------------------------------
sv_add_executable(SV)

if(WIN32)
  # Windows has no rpath: put Dahlia.dll (and any other runtime DLLs) next to the executable
  add_custom_command(TARGET SV POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:SV> $<TARGET_FILE_DIR:SV>
    COMMAND_EXPAND_LISTS
  )
endif()

# ---- Space Visualiser.app -------------------------------------------------------------------------
if(APPLE)
  set(SV_APP_ICON ${SV_ICON_DIR}/SpaceVisualiser.icns)
  set_source_files_properties(${SV_APP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

  sv_add_executable(SVApp MACOSX_BUNDLE EXCLUDE_FROM_ALL ${SV_APP_ICON})
  set_target_properties(SVApp PROPERTIES
    OUTPUT_NAME "Space Visualiser"
    MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/cmake/SVInfo.plist.in
    MACOSX_BUNDLE_BUNDLE_NAME "Space Visualiser"
    MACOSX_BUNDLE_GUI_IDENTIFIER com.kingfishera.SpaceVisualiser
    MACOSX_BUNDLE_ICON_FILE SpaceVisualiser.icns
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    # Load libDahlia from the bundle's Frameworks folder rather than the build tree
    BUILD_WITH_INSTALL_RPATH ON
    INSTALL_RPATH @executable_path/../Frameworks
  )

  set(SV_BUNDLE_FRAMEWORKS $<TARGET_BUNDLE_CONTENT_DIR:SVApp>/Frameworks)
  add_custom_command(TARGET SVApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SV_BUNDLE_FRAMEWORKS}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Dahlia> ${SV_BUNDLE_FRAMEWORKS}
    # Ad-hoc sign the bundle so its contents (Info.plist, icon, embedded dylib) are sealed
    COMMAND codesign --force --sign - $<TARGET_BUNDLE_DIR:SVApp>
    VERBATIM
  )
endif()
