#!/usr/bin/env bash
set -euxo pipefail

cmake -S "${SRC_DIR}" -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_PREFIX_PATH="${PREFIX}" \
    -DROCKY_SUPPORTS_IMGUI=OFF \
    -DROCKY_SUPPORTS_QT=OFF \
    -DROCKY_SUPPORTS_SLUGHORN=OFF
cmake --build build --parallel "${CPU_COUNT}"
cmake --install build
