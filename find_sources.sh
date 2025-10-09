find "./src/SV-Engine" -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.cpp" \)  -print0 | while IFS= read -r -d $'\0' file; do
    echo '"${SV_SRC_DIR}'${file:5}'" '
done