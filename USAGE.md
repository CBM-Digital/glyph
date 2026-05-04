# Glyph Usage

Glyph currently runs games in a headless C++ runtime. It loads a `.glyph` file, runs fixed timestep updates, compiles the render tree into draw commands, and records audio commands.

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

Use `--run` with a game file. `--frames` controls how many fixed `1/60` update steps to execute before printing the final state, compiled draw commands, loaded asset count, and queued audio commands.

```bash
build/glyph --run examples/moving-rect/game.glyph --frames 60
```

Expected output includes a state near `{:x 100}` and two draw commands: `clear` and `rect`.

## Run The Asset And Audio Example

```bash
build/glyph --run examples/sprite-audio/game.glyph --frames 1
```

Expected output includes:

```text
assets: 4
draw-commands: 3
audio-commands: 2
```

The `assets/` files are placeholder paths for this milestone. The runtime resolves asset handles from the manifest by extension, but real image/font/audio decoding belongs to later SDL or platform backends.

## Run The Perfect Shot Headless Example

```bash
build/glyph --run examples/perfect-shot-headless/game.glyph --frames 30
```

This compiles a simple render tree using `clear`, `circle`, `line`, and `text`.

## Current Runtime Limits

The current runner is backend-independent. It does not open a window, play sound, decode textures, or render pixels. It proves the engine-facing pipeline:

```text
game.glyph -> load game -> fixed update -> view -> render commands -> audio command queue
```
