# Arcade snapshot tests

`glyph_visual_snapshot_tests` loads the real `examples/arcade` games, drives timed input
steps, renders through SDL's software renderer, and compares 160x120 PPM screenshots
under `tests/integration/snapshots/arcade`.

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
