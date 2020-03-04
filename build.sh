#!/bin/sh
set -e

mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_SHARED_LIBS=OFF \
  ..
make -j$(nproc)
cd ..
