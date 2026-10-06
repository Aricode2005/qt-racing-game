#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"

export PATH="/Users/mukutchatterjee/Qt/Tools/Ninja:/Users/mukutchatterjee/Qt/Tools/CMake/CMake.app/Contents/bin:/usr/bin:/bin:$PATH"

if [ ! -f "build/ApexVelocity" ]; then
    echo "Building Apex Velocity..."
    cmake -B build -G Ninja -S . -DCMAKE_PREFIX_PATH="/Users/mukutchatterjee/Qt/6.11.1/macos" -DCMAKE_CXX_COMPILER=/usr/bin/clang++
    ninja -C build
fi

echo "Launching Apex Velocity..."
./build/ApexVelocity "$@"
