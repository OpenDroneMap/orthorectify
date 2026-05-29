#!/bin/bash
set -e

if [ ! -f vcpkg/vcpkg ]; then
    ./vcpkg/bootstrap-vcpkg.sh -disableMetrics
fi

cmake --preset linux-release
cmake --build --preset linux-release

