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
# Build both slices from the same pinned sources.
for arch in ${ARCHS}; do
    build_dir="${PROJECT_TEMP_DIR}/jxl-${arch}"
    "$cmake" -S "$source_dir" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$arch" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET}" \
        -DCMAKE_C_COMPILER=/usr/bin/clang \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++
    "$cmake" --build "$build_dir" --target jxloptim --parallel 4
    set -- "$@" "$build_dir/jxloptim"
done
/usr/bin/lipo -create "$@" -output "${BUILT_PRODUCTS_DIR}/jxloptim"
install -m 644 "$build_dir/jxl-LICENSE" "${BUILT_PRODUCTS_DIR}/jxl-LICENSE"
