add_library("SV-Engine" STATIC
  "${SV_SRC_DIR}/SV-Engine/Precompiled Global.cpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/Errors.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/GlobalItems.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/Geometry.cpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/Maths.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/FunctionHandlers.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/Console.cpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/SVUtilities.cpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/Array.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/SVMemory.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/Tree.h" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/Tree.cpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/Console.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/SVUtilities.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/SVMemory.cpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/String.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/Chrono.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/Geometry.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Utilities/Maths.cpp" 
  "${SV_SRC_DIR}/SV-Engine/GUI/Display.cpp" 
  "${SV_SRC_DIR}/SV-Engine/GUI/Display.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Precompiled Global.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Scene/Shadows.cpp" 
  "${SV_SRC_DIR}/SV-Engine/Scene/Camera.cpp" 
  "${SV_SRC_DIR}/SV-Engine/Scene/Transformation.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Scene/Shadows.hpp" 
  "${SV_SRC_DIR}/SV-Engine/Scene/Camera.hpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/Post Processing.hpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/OpenGL Context.cpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/FBO.cpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/VAO.cpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/Texture.cpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/VBO.hpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/ShaderProgram.hpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/VAO.hpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/Texture.hpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/VBO.cpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/ShaderProgram.cpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/Post Processing.cpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/OpenGL Context.hpp" 
  "${SV_SRC_DIR}/SV-Engine/OpenGL/FBO.hpp" 
)

target_include_directories("SV-Engine" PRIVATE
  ${SV_SRC_DIR}
  ${GLM_INCLUDE_DIRS}
  ${GLEW_INCLUDE_DIRS}
  ${SOIL2_INCLUDE_DIRS}
)

target_compile_definitions("SV-Engine" PRIVATE
  GLM_ENABLE_EXPERIMENTAL
)

target_link_libraries("SV-Engine" PRIVATE
  ${SOIL2_LIBRARIES}
  glfw
  OpenGL::GL
  $<IF:$<TARGET_EXISTS:GLEW::glew_s>,GLEW::glew_s,GLEW::GLEW>
)

target_precompile_headers("SV-Engine" PRIVATE "${SV_SRC_DIR}/SV-Engine/Precompiled Global.hpp")

set_target_properties("SV-Engine" PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE True
    INTERPROCEDURAL_OPTIMIZATION False
    MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>DLL
    OUTPUT_NAME "SV-Engine")

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

target_compile_options("SV-Engine" PRIVATE ${COMPILE_OPTIONS})

if(CMAKE_CONFIGURATION_TYPES)
  set(DEBUG_BIN_DIR "${SV_BIN_DIR}")
  set(RELEASE_BIN_DIR "${SV_BIN_DIR}")
else()
  set(DEBUG_BIN_DIR "${SV_BIN_DIR}/Debug")
  set(RELEASE_BIN_DIR "${SV_BIN_DIR}/Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL Debug)
  set_target_properties("SV-Engine" PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${DEBUG_BIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${DEBUG_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${DEBUG_BIN_DIR}"
  )
elseif(CMAKE_BUILD_TYPE STREQUAL Release)
  set_target_properties("SV-Engine" PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${RELEASE_BIN_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${RELEASE_BIN_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${RELEASE_BIN_DIR}"
  )
endif()
