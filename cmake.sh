#!/bin/sh
export CMAKE_PREFIX_PATH=/opt/homebrew/opt/lapack

# Route compiles through ccache when it is installed; CMake reads these at configure time
if command -v ccache >/dev/null 2>&1; then
    export CMAKE_C_COMPILER_LAUNCHER=ccache
    export CMAKE_CXX_COMPILER_LAUNCHER=ccache
fi

exec cmake "$@"