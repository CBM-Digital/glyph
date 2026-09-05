# Glyph iOS simulator target

The CMake iOS target builds a bundled app named `glyph_ios` and packages
`examples/arcade` under `Resources/examples/arcade`. The app entrypoint launches
`examples/arcade/index.glyph`.

Install the SDL2 iOS framework bundles before configuring:

```sh
mkdir -p ios/Frameworks
cp -R /path/to/SDL2.xcframework ios/Frameworks/
cp -R /path/to/SDL2_image.xcframework ios/Frameworks/
cp -R /path/to/SDL2_ttf.xcframework ios/Frameworks/
cp -R /path/to/SDL2_mixer.xcframework ios/Frameworks/
```

Framework bundles can also live elsewhere if CMake is configured with
`-DGLYPH_IOS_SDL2_ROOT=/path/to/sdl2/root` and
`-DGLYPH_IOS_SDL2_IMAGE_ROOT=/path/to/sdl2-image/root`, with equivalent
`GLYPH_IOS_SDL2_TTF_ROOT` and `GLYPH_IOS_SDL2_MIXER_ROOT` settings.

Configure and build with full Xcode selected:

```sh
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
cmake -S . -B build/ios-sim -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator -DGLYPH_REQUIRE_MEDIA=ON
cmake --build build/ios-sim --config Debug --target glyph_ios
```

The arcade bundles Noto Sans rather than relying on macOS fonts. This configuration work has not been validated with an iOS build or device session. Safe-area layout, purchases/restoration and physical lifecycle/performance testing remain release work; see [arcade status](../docs/ARCADE_IMPLEMENTATION.md).
