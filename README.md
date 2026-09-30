# ImageOptim

[ImageOptim](https://github.com/SharkyRawr/ImageOptim) is a GUI for image optimization tools: PNGOUT, [OxiPNG](https://lib.rs/crates/oxipng), AdvPNG, PNGCrush, [JPEGOptim](https://github.com/tjko/jpegoptim), Jpegtran, [Gifsicle](https://github.com/kohler/gifsicle), [SVGO](https://github.com/svg/svgo), [svgcleaner](https://github.com/RazrFalcon/svgcleaner), [MozJPEG](https://github.com/mozilla/mozjpeg), [libavif](https://github.com/AOMediaCodec/libavif), and [libwebp](https://developers.google.com/speed/webp).

This is a fork of [Kornel Lesiński's ImageOptim](https://imageoptim.com).

## Building

Requires:

* Xcode (macOS 26.6 deployment target)
* [Rust](https://rust-lang.org/) installed via [rustup](https://www.rustup.rs/) (not Homebrew).
* Node.js and npm (for the bundled SVGO script).
* CMake (to build the bundled WebP and AVIF executables).
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
