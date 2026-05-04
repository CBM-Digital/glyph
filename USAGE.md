# Glyph Usage (Unix/macOS)

Glyph currently targets Unix-like development environments, with macOS as the first-class desktop path. The desktop target uses pure SDL2 plus SDL_image, SDL_ttf, and SDL_mixer to open a window, process keyboard/mouse input, render PNG sprites and TTF text, load assets, and play sound.

## Prerequisites

macOS with Homebrew:

```bash
brew install cmake pkg-config sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

Debian/Ubuntu:

```bash
sudo apt update
sudo apt install cmake build-essential pkg-config libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
```

Fedora:

```bash
sudo dnf install cmake gcc-c++ pkgconf-pkg-config SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel
```

## Build

From the repository root:

```bash
cmake -S . -B build
cmake --build build
```

The main executable is:

```bash
build/glyph
```

## Run Tests

```bash
ctest --test-dir build --output-on-failure
```

## Run A Game In A Window

```bash
build/glyph --desktop examples/arcade/index.glyph
```

Close the window or press `Escape` to quit.

Useful controls:

```text
tap / confirm: Space, Enter, mouse click
move-x: A/D or Left/Right
move-y: W/S or Up/Down
cancel: Escape
pointer: mouse position and button
```

For smoke tests, `--frames` closes the SDL window after a fixed number of rendered frames:

```bash
build/glyph --desktop examples/arcade/index.glyph --frames 180
```

## Live Reload

The SDL desktop target watches the running `.glyph` file for changes. Edit and save the file while the window is open; Glyph reloads the script automatically.

On successful reload:

```text
current game state is preserved
update/view functions are replaced
asset manifests are reloaded
the window keeps running
```

On reload failure:

```text
the old script keeps running
state is preserved
a reload error overlay appears in the window
the error is printed to stderr
```

Try it with:

```bash
build/glyph --desktop examples/arcade/index.glyph
```

Then change a color or text string in the current scene and save. The desktop runtime watches the top scene file on the navigation stack.

## Arcade Scene Bundle

The milestone 7 proof of concept is a small multi-scene arcade bundle:

```bash
build/glyph --desktop examples/arcade/index.glyph
```

The index scene uses `navigation/push` to open three games:

```text
Perfect Shot: tap when the rotating blue shot aligns with the green target
Stack Tower: tap to drop moving blocks onto the stack
Lane Dodger: use Left/Right or A/D to dodge traffic
```

Each game has a top-left Back button that calls `navigation/pop` to return to the index scene. The bundle uses PNG sprite sheets from `examples/arcade/assets`, WAV sounds, and a TTF font rendered by SDL_ttf.

macOS includes the TTF path used by the examples:

```text
/System/Library/Fonts/Supplemental/Arial.ttf
```

On Linux, replace the `:main` asset value in the example `.glyph` files with an installed TTF such as:

```text
/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
```

## Other Examples

Moving rectangle:

```bash
build/glyph --desktop examples/moving-rect/game.glyph
```

Sprite and audio:

```bash
build/glyph --desktop examples/sprite-audio/game.glyph
```

Perfect Shot headless sample in a window:

```bash
build/glyph --desktop examples/perfect-shot-headless/game.glyph
```

## Headless Mode

Use `--run` when you want a deterministic command-line summary instead of a window. `--frames` controls how many fixed `1/60` update steps execute.

```bash
build/glyph --run examples/moving-rect/game.glyph --frames 60
```

Expected output includes:

```text
state: {:x 100}
draw-commands: 2
```

Asset/audio example:

```bash
build/glyph --run examples/arcade/index.glyph --frames 2
```

Expected output includes:

```text
assets: 5
draw-commands: 13
audio-commands: 1
```

## Evaluate A Glyph Expression

```bash
printf '%s\n' '(+ 1 2 3)' | build/glyph
```

## Asset Support

The current SDL desktop runtime supports:

```text
sprites: PNG/JPEG via SDL_image, with PPM (P3) and BMP fallbacks
text: TTF via SDL_ttf, with a built-in bitmap fallback
audio: WAV/OGG/etc. via SDL_mixer, plus queued WAV and procedural .tone/.music fallback
```

The example `.tone` files are plain text:

```text
frequency seconds volume
```

Example:

```text
720 0.16 0.35
```

## Troubleshooting

If CMake cannot find SDL2 on macOS, confirm `pkg-config` sees Homebrew SDL2:

```bash
pkg-config --modversion sdl2 SDL2_image SDL2_ttf SDL2_mixer
```

If that fails:

```bash
brew reinstall pkg-config sdl2 sdl2_image sdl2_ttf sdl2_mixer
```

For CI or SSH sessions without a display, use SDL dummy drivers and a frame limit:

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/glyph --desktop examples/arcade/index.glyph --frames 3
```

## Platform Notes

The engine code stays on the SDL path for desktop and mobile. macOS, Windows, Linux, iOS, and Android should use the same game/runtime primitives; platform packaging is still a separate build-system task.

Windows developers can install SDL dependencies through vcpkg or prebuilt SDL development packages, then expose the matching `pkg-config` files or CMake package paths before configuring Glyph.

iOS and Android should build SDL2, SDL_image, SDL_ttf, and SDL_mixer for the target platform and link the same Glyph runtime sources into the app shell.

## Current Limits

The SDL target now supports windowed rendering, input, PNG, TTF, WAV/mixer-backed sound, live reload, and scene-stack navigation. Packaged release bundles, persistent storage, and mobile app templates are still future work.
