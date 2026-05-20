# Arcade snapshot tests

`glyph_visual_snapshot_tests` loads the real `examples/arcade` games, drives timed input
steps, renders through SDL's software renderer at each game's logical resolution, and
compares PPM screenshots under `tests/integration/snapshots/arcade`.

When SDL_ttf is available, the harness uses the same TrueType text path as the desktop
runtime so text stays representative of player-facing builds. Without SDL_ttf it falls
back to the built-in bitmap text renderer.

Run the comparison:

```bash
ctest --test-dir build -R glyph_visual_snapshot_tests --output-on-failure
```

Update snapshots after an intentional visual change:

```bash
GLYPH_UPDATE_SNAPSHOTS=1 build/glyph_visual_snapshot_tests
```

When a snapshot differs, the test writes the actual frame to `snapshot_failures/`.
Add or edit scenarios in `tests/integration/PixelSnapshotTests.cpp` by appending an
`InputStep` with a name, elapsed seconds, pressed/released buttons, axis values, or
pointer state.
