These files preserve local changes inside upstream submodules for the `luna` backup.

- `Sparkle-macos-26.6.patch` is the uncommitted change to `Sparkle/Sparkle.xcodeproj/project.pbxproj`. Apply it from `Sparkle/` with `git apply ../patches/Sparkle-macos-26.6.patch` after initializing submodules.
- `pngquant-Cargo.lock` is the untracked `pngquant/src/Cargo.lock`. Copy it back there if that local lockfile is needed.

Neither file is part of an upstream submodule commit. The parent repository's Git links continue to reference upstream commits.
