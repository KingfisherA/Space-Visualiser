#!/bin/sh
export CMAKE_PREFIX_PATH=/opt/homebrew/opt/lapack

exec cmake "$@"
