#!/bin/sh
# Prints the Space Visualiser sources in the form used by Projects/cmake/SV.cmake
find "./src/SV" -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.cpp" \) -print0 | sort -z | while IFS= read -r -d $'\0' file; do
    echo '  "${SV_SRC_DIR}'${file:5}'"'
done
