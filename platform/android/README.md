# Glyph Android

This Gradle project builds the SDL host as `libglyph.so` and packages
`examples/arcade` into the APK assets as `examples/arcade`.

## Prerequisites

- Android Studio with SDK, NDK, CMake, and a device or emulator.
- SDL2 Android source checkout or Android distribution.
- SDL2_image, SDL2_ttf and SDL2_mixer Android source/prebuilt roots for production PNG, bundled-font and audio support.

Create `platform/android/local.properties` from the example:

```properties
sdk.dir=/Users/you/Library/Android/sdk
glyph.sdl2Root=/absolute/path/to/SDL2
glyph.sdl2ImageRoot=/absolute/path/to/SDL2_image
glyph.sdl2TtfRoot=/absolute/path/to/SDL2_ttf
glyph.sdl2MixerRoot=/absolute/path/to/SDL2_mixer
```

`glyph.sdl2Root` must contain SDL's Android Java sources at
`android-project/app/src/main/java`, because `GlyphActivity` extends SDL's
`org.libsdl.app.SDLActivity`.

`glyph.requireMedia` defaults to `ON`: missing release media dependencies fail configuration. For an explicitly limited engine-development build, use `-Pglyph.requireMedia=OFF`. The revised touch adapter has synthetic multi-contact tests; actual Android packaging, audio, safe-area controls, purchase restoration and device performance remain unvalidated. See [arcade status](../../docs/ARCADE_IMPLEMENTATION.md).

## Build and launch

```bash
cd platform/android
./gradlew :app:assembleDebug
./gradlew :app:installDebug
adb shell monkey -p com.glyph.runtime 1
```

Useful overrides:

```bash
./gradlew :app:assembleDebug -Pglyph.abiFilters=arm64-v8a
./gradlew :app:assembleDebug -Pglyph.compileSdk=35 -Pglyph.targetSdk=35
```
