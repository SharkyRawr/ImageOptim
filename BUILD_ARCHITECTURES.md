# Build architecture reference

Verified on 2026-10-24 with Xcode 27.1 (27A9275) on an Apple silicon Mac.
The intended default is a local native build, including Release/Archive, without
requiring an Intel Rust toolchain on an Apple silicon development machine.

## Where the settings live

| Location | Setting or behavior |
| --- | --- |
| `imageoptim/release.xcconfig` | `ARCHS = $(NATIVE_ARCH_ACTUAL);` selects the host Mac's architecture. `ONLY_ACTIVE_ARCH = NO;` remains unchanged. This shared config supplies the app/backend and the codec projects that reference it. |
| `imageoptim/debug.xcconfig` | Includes `release.xcconfig`, so it inherits the native `ARCHS` default, then sets `ONLY_ACTIVE_ARCH = YES;`. |
| `oxipng/oxipng.xcodeproj/project.pbxproj`, `pngquant/pngquant.xcodeproj/project.pbxproj`, `svgcleaner/svgcleaner.xcodeproj/project.pbxproj` | Include the shared configs and translate Xcode's per-slice architecture into Rust's target: `arm64`/`arm64e` becomes `aarch64-apple-darwin`; Intel becomes `x86_64-apple-darwin`. Generated build rules invoke Cargo, then a separate phase runs `lipo`. |
| `avif/build-avif.sh`, `jxl/build-jxl.sh`, `qoi/build-qoi.sh` | Loop over Xcode's `ARCHS`, pass each slice to `CMAKE_OSX_ARCHITECTURES`, then run `lipo -create`, even for one slice. |
| `webp/build-cwebp.sh` | Converts Xcode's space-separated `ARCHS` into a semicolon-separated CMake architecture list. |
| `Sparkle/Configurations/ConfigCommon.xcconfig` | Independently sets `ARCHS = $(ARCHS_STANDARD)`. Sparkle and its helper targets still build arm64 and x86_64 in Release. Its Debug config sets `ONLY_ACTIVE_ARCH = YES`. |

## Overrides and traps

- **Archive:** in the tested Xcode setup, changing only `ONLY_ACTIVE_ARCH` to
  `YES` still produced both architectures. The explicit native `ARCHS` default
  fixed the Rust `can't find crate for core` / missing Intel target failure.
- **Schemes:** the local ImageOptim scheme archives with `Release` and has
  `buildArchitectures="Automatic"`. Removing that attribute alone did not fix
  Archive. Schemes (`*.xcscheme`) are currently Git-ignored, so editing one is
  also not a durable repository fix. No scheme change was retained.
- **Xcode's custom-architectures dialog:** choose **Build** to retain the config;
  **Update and Build** can reset the architecture settings. **Don't ask again**
  suppresses the prompt for that Xcode user; no repository-wide suppression or
  global preference change was implemented. The dialog reported `arm64e`, while
  the verified app actually contained `arm64`; inspect the binary, not the label.
- **Other overrides:** check target/project Build Settings in each `.pbxproj`,
  the selected configuration's base `.xcconfig`, scheme options, and command-line
  build settings. A command-line `ARCHS='arm64 x86_64'` overrides the native
  default; both architectures resolved correctly in a settings-only check.
  A full universal archive was not tested and requires the matching Rust targets
  and native codec toolchains.
- **`lipo` is not proof of a universal build.** Inspect its inputs/output and
  identify which target is compiling Intel code; Sparkle legitimately does so.

## Reproduce and inspect

Run from the repository root after completing the README's build prerequisites:

```sh
xcodebuild -project imageoptim/ImageOptim.xcodeproj -scheme ImageOptim \
  -configuration Release -destination 'platform=macOS,name=My Mac' \
  -showBuildSettings | rg ' (ARCHS|NATIVE_ARCH_ACTUAL|ONLY_ACTIVE_ARCH) ='

xcodebuild -project imageoptim/ImageOptim.xcodeproj -scheme ImageOptim \
  -configuration Release -destination 'platform=macOS,name=My Mac' \
  -derivedDataPath /private/tmp/ImageOptim-Release-DerivedData \
  -archivePath /private/tmp/ImageOptim-Release.xcarchive archive

lipo -archs ~/ImageOptim.app/Contents/MacOS/ImageOptim
lipo -archs ~/ImageOptim.app/Contents/Frameworks/ImageOptimGPL.framework/Versions/A/Resources/oxipng
codesign --verify --deep --strict ~/ImageOptim.app
```

Use `My Mac` by name: adding `arch=arm64` with the native macro caused Xcode's
destination preflight to offer `arm64e` and reject the explicit destination,
although the name-only archive resolved and built `arm64` correctly.

The verified signed archive was `/private/tmp/ImageOptim-Home-Release.xcarchive`;
its complete app was copied with `ditto` to `~/ImageOptim.app`. The app, Finder
extension, and checked codec helpers were arm64; Sparkle was arm64+x86_64. Cargo
used only `aarch64-apple-darwin`. The copied bundle passed signature verification.
An earlier archive also launched and optimized a disposable JPEG XL fixture.
These are local Apple silicon results; Intel/universal archive execution remains
unverified. Temporary archive/log paths may disappear and are not source files.
