# Glyph Usage (Unix/macOS)

Glyph currently targets Unix-like development environments, with macOS as the first-class desktop path. The desktop target uses SDL2 to open a window, process keyboard/mouse input, render draw commands, load simple sprite/audio assets, and play sound.

## Prerequisites

macOS with Homebrew:

```bash
brew install cmake sdl2
```

Debian/Ubuntu:

```bash
sudo apt update
sudo apt install cmake build-essential pkg-config libsdl2-dev
```

Fedora:

```bash
sudo dnf install cmake gcc-c++ pkgconf-pkg-config SDL2-devel
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
build/glyph --desktop examples/moving-rect/game.glyph
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
build/glyph --desktop examples/sprite-audio/game.glyph --frames 180
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
build/glyph --desktop examples/moving-rect/game.glyph
```

Then change the rectangle color in `examples/moving-rect/game.glyph` and save.

## Example Games

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
build/glyph --run examples/sprite-audio/game.glyph --frames 1
```

Expected output includes:

```text
assets: 4
draw-commands: 3
audio-commands: 2
```

## Evaluate A Glyph Expression

```bash
printf '%s\n' '(+ 1 2 3)' | build/glyph
```

## Asset Support

The current SDL desktop runtime supports:

```text
sprites: PPM (P3) and BMP
text: built-in bitmap font
audio: WAV and procedural .tone/.music files
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
pkg-config --modversion sdl2
```

If that fails:

```bash
brew reinstall sdl2 pkg-config
```

For CI or SSH sessions without a display, use SDL dummy drivers and a frame limit:

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/glyph --desktop examples/sprite-audio/game.glyph --frames 3
```

## Current Limits

The SDL target is intentionally small. It does not yet support PNG/JPEG via SDL_image, TTF rendering via SDL_ttf, streamed music via SDL_mixer, hot reload, or packaged release bundles.
