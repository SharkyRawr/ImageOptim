# ImageOptim

[ImageOptim](https://github.com/SharkyRawr/ImageOptim) is a GUI for image optimization tools: PNGOUT, [OxiPNG](https://lib.rs/crates/oxipng), AdvPNG, PNGCrush, [JPEGOptim](https://github.com/tjko/jpegoptim), Jpegtran, [Gifsicle](https://github.com/kohler/gifsicle), [SVGO](https://github.com/svg/svgo), [svgcleaner](https://github.com/RazrFalcon/svgcleaner), [MozJPEG](https://github.com/mozilla/mozjpeg), [libavif](https://github.com/AOMediaCodec/libavif), [libjxl](https://github.com/libjxl/libjxl), and [libwebp](https://developers.google.com/speed/webp).

This is a fork of [Kornel Lesiński's ImageOptim](https://imageoptim.com).

## Building

Requires:

* Xcode (macOS 26.6 deployment target)
* [Rust](https://rust-lang.org/) installed via [rustup](https://www.rustup.rs/) (not Homebrew).
* Node.js and npm (for the bundled SVGO script).
* CMake (to build the bundled WebP, AVIF, and JPEG XL executables).
* NASM (for the x86_64 AVIF codec build).

```sh
git clone --recursive https://github.com/SharkyRawr/ImageOptim.git ImageOptim
cd ImageOptim
make -C pngcrush
make -C svgo
xcodebuild -project imageoptim/ImageOptim.xcodeproj -scheme ImageOptim -configuration Debug CODE_SIGNING_ALLOWED=NO build
```

The two `make` commands generate inputs that Xcode needs before it plans the build. Open `imageoptim/ImageOptim.xcodeproj` to build from Xcode after running them.

In case of build errors, these sometimes help:

```sh
git submodule update --init --recursive
```

```sh
make -C pngcrush
make -C svgo
```

Still AVIF images are optimized losslessly with bundled libavif/libaom, preserving decoded YUV/alpha samples, color information, and metadata. Files are replaced only when smaller. Animated, progressive, and gain-map AVIFs are currently left untouched. The codec sources are pinned submodules under `avif/`.

Run the standalone AVIF checks with `cmake -S avif -B /tmp/imageoptim-avif -DIMAGEOPTIM_AVIF_TESTS=ON`, `cmake --build /tmp/imageoptim-avif`, and `ctest --test-dir /tmp/imageoptim-avif --output-on-failure`.

Still JPEG XL images in their original color profile are optimized losslessly with bundled libjxl. The optimizer supports 8- and 16-bit grayscale/RGB with optional alpha, preserves orientation, color information and Exif/XMP boxes, and verifies decoded samples before keeping a smaller JPEG XL output. XYB/lossy-profile images, animation, previews, JPEG reconstruction data, unusual extra channels, and unknown container boxes are left untouched. Inputs are limited to 512 MiB and 64 million pixels.

libjxl v0.12.0 is pinned under `jxl/src`, with its Brotli, Highway, and skcms dependencies pinned as nested submodules. Initialize them with `git submodule update --init --recursive jxl/src`; normal builds use these sources without downloading dependencies or using Homebrew codec libraries.

Run the standalone JPEG XL checks with `cmake -S jxl -B /tmp/imageoptim-jxl -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DIMAGEOPTIM_JXL_TESTS=ON`, `cmake --build /tmp/imageoptim-jxl --target jxloptim jxloptim-test`, and `ctest --test-dir /tmp/imageoptim-jxl --output-on-failure`.
