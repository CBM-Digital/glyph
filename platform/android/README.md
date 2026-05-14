# Glyph Android

This Gradle project builds the SDL host as `libglyph.so` and packages
`examples/arcade` into the APK assets as `examples/arcade`.

## Prerequisites

- Android Studio with SDK, NDK, CMake, and a device or emulator.
- SDL2 Android source checkout or Android distribution.
- Optional SDL2_image Android source checkout for PNG/JPG texture loading.

Create `platform/android/local.properties` from the example:

```properties
sdk.dir=/Users/you/Library/Android/sdk
glyph.sdl2Root=/absolute/path/to/SDL2
glyph.sdl2ImageRoot=/absolute/path/to/SDL2_image
```

`glyph.sdl2Root` must contain SDL's Android Java sources at
`android-project/app/src/main/java`, because `GlyphActivity` extends SDL's
`org.libsdl.app.SDLActivity`.

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
