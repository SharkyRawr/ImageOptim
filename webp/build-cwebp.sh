#!/bin/sh
set -eu

source_dir="$(cd "$(dirname "$0")/src" && pwd)"
build_dir="${PROJECT_TEMP_DIR}/libwebp"
architectures=$(printf '%s' "${ARCHS}" | tr ' ' ';')
if command -v cmake >/dev/null 2>&1; then
    cmake=$(command -v cmake)
elif [ -x /opt/homebrew/bin/cmake ]; then
    cmake=/opt/homebrew/bin/cmake
elif [ -x /usr/local/bin/cmake ]; then
    cmake=/usr/local/bin/cmake
else
    echo 'CMake is required to build cwebp' >&2
    exit 1
fi

"$cmake" -S "$source_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="${CONFIGURATION}" \
    -DCMAKE_OSX_ARCHITECTURES="$architectures" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET}" \
    -DBUILD_SHARED_LIBS=OFF \
    -DWEBP_LINK_STATIC=ON \
    -DWEBP_BUILD_ANIM_UTILS=OFF \
    -DWEBP_BUILD_CWEBP=ON \
    -DWEBP_BUILD_DWEBP=OFF \
    -DWEBP_BUILD_GIF2WEBP=OFF \
    -DWEBP_BUILD_IMG2WEBP=OFF \
    -DWEBP_BUILD_VWEBP=OFF \
    -DWEBP_BUILD_WEBPINFO=OFF \
    -DWEBP_BUILD_LIBWEBPMUX=OFF \
    -DWEBP_BUILD_WEBPMUX=OFF \
    -DWEBP_BUILD_EXTRAS=OFF \
    -DCMAKE_DISABLE_FIND_PACKAGE_PNG=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_JPEG=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_GIF=ON \
    -DCMAKE_DISABLE_FIND_PACKAGE_OpenGL=ON
"$cmake" --build "$build_dir" --target cwebp --parallel 4
install -m 755 "$build_dir/cwebp" "${BUILT_PRODUCTS_DIR}/cwebp"
install -m 644 "$source_dir/COPYING" "${BUILT_PRODUCTS_DIR}/libwebp-COPYING"
