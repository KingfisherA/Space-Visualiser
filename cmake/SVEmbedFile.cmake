# Embeds files in a target as byte arrays, generated at build time.
#
#   sv_embed_files(<target> <file>...)
#
# Each file is written to ${CMAKE_CURRENT_BINARY_DIR}/Embedded/<target>/<file name>.inc as a C
# initializer list, which <target> can include (the directory is a private include directory):
#
#     static const unsigned char kData[] =
#     #include "My File.png.inc"
#     ;

set(SV_EMBED_SCRIPT ${CMAKE_CURRENT_LIST_FILE})

function(sv_embed_files TARGET)
  set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/Embedded/${TARGET})
  set(OUTPUTS)
  foreach(FILE ${ARGN})
    get_filename_component(INPUT ${FILE} ABSOLUTE)
    get_filename_component(NAME ${FILE} NAME)
    set(OUTPUT ${OUTPUT_DIR}/${NAME}.inc)
    add_custom_command(
      OUTPUT ${OUTPUT}
      COMMAND ${CMAKE_COMMAND} -DSV_EMBED_INPUT=${INPUT} -DSV_EMBED_OUTPUT=${OUTPUT} -P ${SV_EMBED_SCRIPT}
      DEPENDS ${INPUT} ${SV_EMBED_SCRIPT}
      COMMENT "Embedding ${NAME}"
      VERBATIM
    )
    list(APPEND OUTPUTS ${OUTPUT})
  endforeach()

  add_custom_target(${TARGET}Embedded DEPENDS ${OUTPUTS})
  add_dependencies(${TARGET} ${TARGET}Embedded)
  target_include_directories(${TARGET} PRIVATE ${OUTPUT_DIR})
endfunction()

# Script mode: convert SV_EMBED_INPUT to SV_EMBED_OUTPUT
if(CMAKE_SCRIPT_MODE_FILE AND DEFINED SV_EMBED_INPUT)
  file(READ "${SV_EMBED_INPUT}" HEX HEX)
  # 32 bytes a line
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," BYTES "${HEX}")
  string(REGEX REPLACE "((0x..,){32})" "\\1\n" BYTES "${BYTES}")
  file(WRITE "${SV_EMBED_OUTPUT}" "{\n${BYTES}\n}\n")
endif()
