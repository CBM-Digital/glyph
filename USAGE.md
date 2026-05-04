# Glyph Usage

Glyph can run games either headlessly or in the SDL desktop target. The desktop target opens a window, maps keyboard/mouse input to Glyph actions, renders draw commands with SDL2, loads PPM/BMP sprite assets, renders text with a built-in bitmap font, and plays WAV or procedural `.tone` audio assets.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run Tests

```bash
ctest --test-dir build --output-on-failure
```

## Evaluate A Glyph Expression

```bash
printf '%s\n' '(+ 1 2 3)' | build/glyph
```

## Run A Game

### Desktop Window

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

For automated smoke checks, pass `--frames` to close after a fixed number of rendered frames:

```bash
build/glyph --desktop examples/sprite-audio/game.glyph --frames 180
```

### Headless Summary

Use `--run` with a game file. `--frames` controls how many fixed `1/60` update steps to execute before printing the final state, compiled draw commands, loaded asset count, and queued audio commands.

```bash
build/glyph --run examples/moving-rect/game.glyph --frames 60
```

Expected output includes a state near `{:x 100}` and two draw commands: `clear` and `rect`.

## Run The Asset And Audio Example In A Window

```bash
build/glyph --desktop examples/sprite-audio/game.glyph
```

This loads `assets/hero.ppm`, renders it as a sprite, draws bitmap text, and plays the procedural tone assets declared in the manifest.

## Run The Asset And Audio Example Headlessly

```bash
build/glyph --run examples/sprite-audio/game.glyph --frames 1
```

Expected output includes:

```text
assets: 4
draw-commands: 3
audio-commands: 2
```

## Run The Perfect Shot Headless Example

```bash
build/glyph --run examples/perfect-shot-headless/game.glyph --frames 30
```

This compiles a simple render tree using `clear`, `circle`, `line`, and `text`.

It also runs in the SDL desktop target:

```bash
build/glyph --desktop examples/perfect-shot-headless/game.glyph
```

## Current Runtime Limits

The SDL desktop target is intentionally small. It supports real windows, shape rendering, PPM/BMP sprites, built-in bitmap text, WAV and `.tone` audio, and keyboard/mouse input. It does not yet support SDL_image formats such as PNG/JPEG, SDL_ttf font rendering, streamed music, hot reload, or packed release bundles.
