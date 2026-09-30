#!/bin/sh
set -eu
source_dir="$(cd "$(dirname "$0")" && pwd)"
if command -v cmake >/dev/null 2>&1; then
    cmake=$(command -v cmake)
elif [ -x /opt/homebrew/bin/cmake ]; then
    cmake=/opt/homebrew/bin/cmake
else
    cmake=/usr/local/bin/cmake
fi
# Build each architecture separately: libaom's assembly configuration is CPU-specific.
for arch in ${ARCHS}; do
    build_dir="${PROJECT_TEMP_DIR}/avif-${arch}"
    "$cmake" -S "$source_dir" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$arch" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET}" \
        -DAOM_TARGET_CPU="$arch" -DCMAKE_C_COMPILER=/usr/bin/clang \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++
    "$cmake" --build "$build_dir" --target avifoptim --parallel 4
    set -- "$@" "$build_dir/avifoptim"
done
/usr/bin/lipo -create "$@" -output "${BUILT_PRODUCTS_DIR}/avifoptim"
install -m 644 "$build_dir/avif-LICENSE" "${BUILT_PRODUCTS_DIR}/avif-LICENSE"
