# Glyph Arcade implementation status

Updated 2026-09-05. This implements the playable core of milestone one, plus shared engine repairs. It does **not** represent a completed five-game product or a validated commercial release. The approved plan gates content expansion on an external 20-player test. That test has not taken place.

## Play the slice

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGLYPH_REQUIRE_MEDIA=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
build/glyph --demo
```

Use `build/glyph --desktop examples/arcade/index.glyph` for the five-cabinet development launcher and prototype archive. All five selected games are accessible. Only Comet and Alpine have received the revised content described below. Existing local edits were the baseline; the previous launcher, Comet, and Alpine scripts are preserved in `prototype-index.glyph` and each game's `prototype.glyph`.

## Implemented content

| Area | Playable implementation | Remaining planned content |
| --- | --- | --- |
| Comet Courier | Six authored contracts in sector one; two three-delivery routes; three hull; drag aim/power and stick/keyboard aiming; cached prediction using the same fixed-step physics; one brake; optional rings; standard, express, fragile cargo; docking-speed failures; saved between-delivery checkpoint; medals and records | Two further sectors, twelve contracts, repulsive/pulsing wells, challenge variants |
| Alpine Rush | Pine Ridge: twelve authored sections, 24 gates, approximately 64 seconds at normal speed; acceleration/carving, trails, jump/landing response; tuck/stamina/brake; clean chains, perfect lines, optional medal routes, once-per-obstacle near-misses; explicit loss feedback, medals and records | Five additional courses, ice/village visual families, challenge variants |
| Arcade shell | Five animated cabinet cards, focus navigation, visible record/medal indicators, separate two-game demo, prototype archive, persistent mute, explicit pause/back/quit | Compact world-map treatment, restoration decorations, six tours, daily challenge archive, cosmetics/music unlocks |
| Circuit Keep | Existing prototype, portable font | Linked defenses, three maps, four tower types, six enemy roles, eight waves, twelve upgrades and boss |
| Volt Grid | Existing prototype, portable font | Entire persistent cross-rotation circuit model, undo, twenty solvable boards; original formula-board defect remains here |
| Fishing Cove | Existing locally edited prototype, portable font | Habitat/bait/conditions, three coves, twelve species, four fights, requests, guide/aquarium, alternate equipment |

The six Comet contracts have tested ring-and-docking trajectories, including gentle fragile docking. The Alpine course has an automated steering/jumping witness that clears every gate and reaches the gold threshold. These prove attainability; they do not prove human enjoyment or appropriate difficulty.

## Engine and defect corrections

- Input transitions survive rendered frames until a fixed simulation step consumes them. Rapid press/release pairs are retained and delivered once; held state persists between steps.
- Axes combine all held keys, aliases, controller buttons/sticks and touch input. Releasing one source no longer cancels another.
- Escape pauses. Controller navigation, dead zone, explicit quit, two-contact Alpine touch, focus/background cancellation and deliberate resume are implemented. The steering pad activates during play, leaving briefing/results buttons usable.
- Profile data is versioned, literal-only and portable across VM interners. Atomic replacement, backup recovery (including missing primary files), size limits and preservation of unsupported future versions protect progress. Disk failures produce a visible error. This is a local service, not Steam Cloud integration.
- RNG state belongs to each VM and survives hot reload; existing random functions remain available. New slice content uses authored layouts and deterministic visual decoration. A separate general-purpose cosmetic stream API and challenge seed enforcement are still outstanding.
- `profile/get`, `profile/set`, `arcade/context`, and `arcade/complete` provide the first shared run/profile interface. Completion persists best score and medal rank. Tours and challenge-specific record namespaces are not implemented; `:stats` currently describes the latest completion, while score/medals are independent maxima.
- Named `:frame` atlas lookup now selects the declared source rectangle. Previously a hash fallback could draw the wrong sprite. Missing named frames are rejected.
- SDL sound pitch now resamples playback and caches pitch variants. Volume is per channel. Mute is persistent; scene activation restores music. Physical listening checks are still required.
- Game unload/reload breaks the global function-closure ownership cycle. This fixes retained top-level VM environments; the interpreter is not a general cycle-collecting GC.
- Harbor no longer rewards switch tapping. Near-passes use encounter IDs; collision separates the boats and produces one strike. Starforge resolves the contacted shield segment from relative arrival angle, including rotated quadrants and boundaries. These remain prototypes.
- Arcade scripts use bundled Noto fonts. Android/iOS configuration can include SDL_ttf and SDL_mixer; strict media configuration fails when release dependencies are missing. Mobile packages have not been built or device-tested in this pass.

## Assets and reproducibility

`tools/import_arcade_assets.py` selects original sprites from the local `kenney_tiny-ski` and `kenney_space-shooter-extension` packs, builds 64-pixel named atlases, copies their licenses, records input SHA-256 hashes, copies Noto Sans Regular/Semibold and OFL, and generates eight WAV cues/loops. It requires Python/Pillow and the original source folders. Generated outputs are checked into `examples/arcade/assets`; players need none of the import tools or source packs.

See `assets/original/*.frames.json` for frame rectangles, `sources.json` for source identity, and adjacent `*-LICENSE.txt` files. Scripts use Glyph's existing named grid-frame format. The JSON files document the import and are not a second runtime sprite format. `tools/generate_demo.py` derives the demo entry from the shared launcher.

## Validation and limits

Seven local test executables cover the existing engine/runtime suites plus the new playability and SDL adapter suites. Coverage includes 30/60/120/144 Hz input, rapid taps, pause/resume, simultaneous held sources, controller dead zone, normalized multi-touch, focus loss, literal save round trips/recovery, independent RNG, attainable delivery/ring routes, complete route/checkpoint/retry, Alpine completion and score, prototype collisions/shield angles, and atlas rectangles. Visual fixtures cover the launcher/demo, briefings, aiming, flight/braking, skiing, and result/failure cards.

The Release build passes all seven suites. The installed development layout passed both an SDL dummy-driver smoke test and a 90-frame native macOS window smoke test from outside the repository. The native test required display access outside the filesystem sandbox. These checks and software snapshots do not replace extended physical controller/audio, GPU, Steam Deck, Windows/Linux, or mobile testing. Staging currently copies executable/content; it does not bundle every shared runtime dependency, sign/notarize packages, or publish a Steam build. No external players, store engagement, purchases, or financial outcomes have been measured.

## Next delivery gate

1. Run the [20-player protocol](PLAYTEST.md), review explanations of failure and repeat attempts, then tune the slice. Complete physical controller/audio testing and frame-time/asset/allocation profiling before calling milestone one validated.
2. If the internal fun targets are met, implement the remaining three redesigns and content volumes, tours, daily archive, progression, externalized English strings, safe-area HUD/touch layout, shared asset reuse and profiling. Keep scored challenges free of permanent statistical advantages.
3. Package and graphically test native Windows/macOS/Linux builds, including shared dependencies, controller-complete navigation and Steam Deck. Configure Steam Cloud to separate portable progress from machine settings; polish the demo before storefront/Next Fest work.
4. Treat $9.99 Steam and $5.99 mobile unlock as pricing hypotheses from the approved plan. Add mobile purchase restoration, safe areas, lifecycle/performance validation and device controls only after desktop validation. No purchases, accounts, ads, online service or storefront publishing were added.

Do not use this local slice as evidence that the full collection is ready to sell. The next decision is whether strangers understand the actions and choose to try again.
