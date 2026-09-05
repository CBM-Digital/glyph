# Glyph Usage

The desktop target uses C++20, SDL2, SDL_image, SDL_ttf, and SDL_mixer. The arcade slice has been validated locally on macOS. Windows, Linux, mobile, and Steam Deck still need native build and device validation; CMake configuration support alone is not release certification.

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
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGLYPH_REQUIRE_MEDIA=ON
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

For the two-game playtest, run `build/glyph --demo`. The full index also exposes three selected prototypes and the prototype archive. Close the window to quit, or open the pause menu and choose Quit.

Useful controls:

```text
confirm: Space, Enter, controller A
tap: confirm, mouse click, touch
move-x: A/D or Left/Right
move-y: W/S or Up/Down
controller axes: left stick / D-pad
secondary: X / controller X
cancel / back: Backspace / controller B
pause: Escape / controller Start / Android Back
pointer: mouse position and button
```

Pause menu: Enter/A resumes, Backspace/B returns to the arcade, Q/Y quits. Losing focus pauses the game; resume deliberately after returning. Alpine supports two touch contacts: drag on the left to steer/tuck/brake and tap on the right to jump. Touch behavior has automated adapter coverage, but still requires device testing.

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

The expedition launcher contains two revised games and three development prototypes:

```bash
build/glyph --desktop examples/arcade/index.glyph
```

The two-game demo includes:

```text
Comet Courier: six contracts in two three-delivery routes, bonus rings, fragile cargo, one brake burst
Alpine Rush: one 60–90 second course, twelve authored sections, clean gate chains and risky medal lines
```

Each game has a top-left Back button. Circuit Keep, Volt Grid, and Fishing Cove remain prototypes. See [scope and remaining work](docs/ARCADE_IMPLEMENTATION.md). Earlier Comet and Alpine scripts are preserved as `prototype.glyph` beside their replacements.

The arcade bundles Noto Sans and its OFL license, selected original Kenney sprites and licenses, and synthesized WAV cues. It requires no system font installation. Regenerate assets with Python and Pillow, using the original Downloads packs:

```bash
python3 tools/import_arcade_assets.py --source "$HOME/Downloads"
python3 tools/generate_demo.py
```

Records, mute settings, and Comet checkpoints use SDL's portable user-data directory (`SDL_GetPrefPath("Glyph", "Arcade")`), with atomic replacement and a recoverable `.bak` copy. To isolate a playtest profile:

```bash
GLYPH_PROFILE_PATH=/tmp/glyph-playtest.glyphdata build/glyph --demo
```

`cmake --install build --prefix build/demo-package` stages the two-game scripts, assets, and executable. This is a development staging directory: it does **not** yet bundle SDL's shared dependencies or provide a signed distributable.

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

The summary reports asset, draw-command, and queued-audio counts for the current scripts.

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
