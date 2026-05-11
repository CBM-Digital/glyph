# Glyph

**Glyph** is a small, production-oriented 2D game runtime written in C++ with games authored in a tiny Lisp-like scripting language.

Glyph is designed for making **small, polished, arcade-style 2D games quickly**. It prioritizes live iteration, portable deployment, simple game code, strong defaults, and a deliberately constrained feature set.

The core idea:

```text
state -> update(dt, state) -> new-state -> view(state) -> render-tree
````

C++ owns the engine, platforms, rendering, audio, assets, input, storage, and runtime lifecycle.

Glyph script owns game state, rules, and declarative scene composition.

---

## Table of contents

1. Project vision
2. Goals
3. Non-goals
4. Product positioning
5. MVP definition
6. Runtime model
7. Engine/language ownership boundary
8. Glyph language specification
9. Standard library specification
10. Render tree specification
11. Game file format
12. Multi-game bundle format
13. Input model
14. State, persistence, and serialization
15. Hot reload
16. C++ architecture
17. Core data structures
18. AST and interpreter design
19. Memory management and garbage collection
20. Rendering architecture
21. Audio architecture
22. Asset system
23. Mobile support
24. Web support
25. Testing strategy
26. Performance targets
27. Security and sandboxing
28. Build system and repo layout
29. Implementation roadmap
30. First sample games
31. Long-term direction

---

# 1. Project vision

Glyph exists to make small 2D games extremely fast to create, modify, bundle, and ship.

It is not intended to replace Unity, Godot, Unreal, GameMaker, or custom C++ engines.

Glyph is optimized for a narrower product category:

```text
small arcade games
mobile-first games
one-screen games
score-chasing games
tiny puzzle games
daily challenge games
arcade bundles
AI-assisted game generation
rapid prototyping
```

The ideal Glyph game can be understood within seconds, played in short sessions, and represented as a small script file.

The ideal Glyph developer workflow is:

```bash
glyph new stack-tower
glyph run
# edit game.glyph
# see changes immediately
glyph build ios
glyph build android
glyph build web
```

The project should feel like:

```text
PICO-8 immediacy
+ React-style declarative composition
+ C++ runtime performance
+ SDL-style portability
+ Lisp-like data/code simplicity
```

---

# 2. Goals

## Primary goal

Build a small, robust, production-ready runtime for shipping simple 2D games written in Glyph script.

## Product goals

Glyph should enable:

```text
fast game creation
fast iteration
small codebases
many games in one app
portable builds
highly replayable arcade games
simple onboarding
AI-assisted generation and modification
```

## Technical goals

Glyph should provide:

```text
tiny Lisp-like language
plain-data state model
fixed timestep update loop
declarative render trees
hot reload
mobile-friendly input abstraction
sprite/shape/text rendering
audio playback
asset manifest
state serialization
safe scripting sandbox
cross-platform runtime core
```

## Developer experience goals

A developer should be able to:

```text
write a game in one file
understand the full program at once
live reload without losing state
ship the same game on multiple platforms
avoid engine boilerplate
avoid complex object lifecycles
avoid manually wiring native platform input
```

---

# 3. Non-goals

Glyph intentionally avoids broad engine scope.

## Not in MVP

```text
3D rendering
full physics engine
visual editor
network multiplayer
complex skeletal animation
ECS framework
shader graph
node editor
plugin marketplace
native scripting escape hatches
arbitrary filesystem access from scripts
dynamic user code execution from network
large open-world games
large RPG systems
complex module/dependency graph
```

## Not the product promise

Glyph is not:

```text
a Unity alternative
a Godot alternative
a full Lisp implementation
a general application scripting language
a modding VM for arbitrary native access
a AAA-capable engine
```

## Console support

Console support is a long-term target, not an MVP target.

The architecture should avoid decisions that make consoles impossible, but no console-specific work is required for the first production release.

---

# 4. Product positioning

Glyph should not be marketed primarily as “a Lisp game engine.”

Better positioning:

> **A tiny live-coded 2D runtime for making polished arcade games fast.**

Alternative tagline:

> **Make tiny games. Tweak them live. Ship everywhere.**

For commercial use, the strongest near-term product angle is:

> **A runtime for producing and bundling many small arcade games efficiently.**

This supports products such as:

```text
mobile arcade bundle
daily challenge arcade
premium mini-game collection
web arcade portal
Steam microgame collection
AI-assisted rapid game production tool
```

---

# 5. MVP definition

The MVP must be small, but it must be shippable.

“Production-ready MVP” means:

```text
stable runtime loop
deterministic update model
safe script execution
reasonable error messages
asset loading
state persistence
mobile-ready input model
hot reload during development
packaged release builds
test coverage for parser/interpreter core
no known memory leaks in normal use
clear limits and documented unsupported behavior
```

## MVP platforms

Required:

```text
desktop via SDL
mobile architecture ready
iOS first-class path
Android first-class path
```

Nice-to-have:

```text
web via Emscripten
```

## MVP rendering

Required:

```text
clear
group
rect
circle
line
sprite
text
camera
transform
```

## MVP audio

Required:

```text
sound effects
music playback
volume control
audio command queue
```

## MVP input

Required:

```text
tap
hold
pointer position
swipe left/right/up/down
semantic actions
keyboard fallback
controller fallback
```

## MVP scripting

Required:

```text
parser
AST interpreter
numbers
strings
booleans
nil
keywords
vectors
maps
functions
closures
special forms
standard library
engine native functions
state serialization
```

## MVP samples

Required sample games:

```text
Perfect Shot
Stack Tower
Lane Dodger
```

These three games are the acceptance test for the engine.

If these cannot be implemented cleanly in Glyph, the language or engine is wrong.

---

# 6. Runtime model

Glyph games are plain data transformations.

Each game defines:

```lisp
(def initial {...})

(defn update [dt state]
  ...)

(defn view [state]
  ...)
```

The engine repeatedly does:

```cpp
state = update(fixedDt, state);
tree = view(state);
render(tree);
```

## Frame lifecycle

```text
begin frame
poll platform events
update input state
run fixed timestep updates
call Glyph update
call Glyph view
compile render tree
submit draw commands
flush audio commands
present frame
end frame
```

## Fixed timestep

MVP uses a fixed simulation timestep:

```cpp
fixedDt = 1.0 / 60.0;
```

Rendering may occur at display refresh rate, but game simulation updates at fixed intervals.

## Pause behavior

When a game is paused:

```text
update is not called
view may still be called
audio is paused or reduced according to host policy
state remains in memory or is serialized
```

---

# 7. Engine/language ownership boundary

## C++ engine owns

```text
windowing
platform lifecycle
rendering backend
audio backend
input devices
touch/keyboard/gamepad mapping
asset loading
texture management
font management
sound/music resources
fixed timestep
hot reload
file watching
state serialization implementation
save storage
crash-safe product builds
platform builds
native app shell
```

## Glyph script owns

```text
game state
game rules
score
player movement
enemy behavior
spawn logic
collision response
which sprites exist
which sounds should play
which scene is active
UI composition
animation choice
local progression
```

## Important design rule

The engine should not expose mutable native game objects to Glyph.

Avoid this style:

```lisp
(set-position! player 100 200)
(player.move 10 0)
(enemy.destroy!)
```

Prefer this style:

```lisp
(assoc player :x 100)
(update-in state [:player :x] + 10)
(remove enemies dead?)
```

The language returns new data. The engine consumes that data.

This makes hot reload, serialization, testing, and AI-assisted code generation much easier.

---

# 8. Glyph language specification

Glyph is a tiny Lisp-like expression language.

It is not Common Lisp, Scheme, or Clojure.

It borrows syntax ideas from Lisp and Clojure because they are easy to parse and naturally represent trees.

## 8.1 Values

```lisp
nil
true
false

123
3.14

"hello"

:player
:game-over

[1 2 3]

{:x 10
 :y 20
 :hp 3}

(+ 1 2)
```

## 8.2 Value types

```text
nil       empty value
bool      true / false
number    double-precision number
string    UTF-8 string
keyword   interned symbolic identifier
vector    ordered list of values
map       keyword-keyed dictionary
function  Glyph function
native    C++ native function
```

## 8.3 Comments

```lisp
; this is a comment
```

Comments run from `;` to the end of the line.

## 8.4 Function calls

```lisp
(+ 1 2)
(sprite :image :hero :x 10 :y 20)
```

The first item is evaluated as the callee. Remaining items are evaluated as arguments, unless the list is a special form.

## 8.5 Special forms

Special forms are syntax constructs evaluated specially by the interpreter.

MVP special forms:

```text
game
def
defn
fn
let
if
cond
case
do
for
```

No macros in v0.1.

No mutation syntax in v0.1.

No module/import system in v0.1.

---

## 8.6 `game`

Declares a game.

```lisp
(game perfect-shot
  :title "Perfect Shot"
  :size [360 640]
  :initial initial
  :update update
  :view view)
```

Required fields:

```text
:title
:size
:initial
:update
:view
```

Optional fields:

```text
:author
:version
:assets
:tags
:description
```

---

## 8.7 `def`

Defines a top-level value.

```lisp
(def gravity 900)

(def initial
  {:score 0
   :dead false})
```

---

## 8.8 `defn`

Defines a named function.

```lisp
(defn add [a b]
  (+ a b))
```

Multiple body expressions are allowed. The last value is returned.

```lisp
(defn hit [state]
  (sound/play :hit)
  (assoc state :dead true))
```

---

## 8.9 `fn`

Creates an anonymous function.

```lisp
(fn [x] (* x x))
```

Useful with higher-order functions:

```lisp
(map enemies
  (fn [enemy]
    (update enemy :y + 10)))
```

---

## 8.10 `let`

Creates local bindings.

```lisp
(let [x 10
      y 20]
  (+ x y))
```

Bindings are evaluated in order.

Later bindings can reference earlier bindings.

```lisp
(let [x 10
      y (+ x 5)]
  y)
```

---

## 8.11 `if`

Conditional expression.

```lisp
(if (:dead state)
  state
  (update-game dt state))
```

Only the selected branch is evaluated.

---

## 8.12 `cond`

Multi-branch conditional.

```lisp
(cond
  (< hp 1) :dead
  (< hp 3) :hurt
  else :ok)
```

`else` is a reserved symbol inside `cond`.

---

## 8.13 `case`

Keyword/value dispatch.

```lisp
(case (:scene state)
  :menu (update-menu dt state)
  :play (update-play dt state)
  :dead (update-dead dt state))
```

An optional default branch may be provided with `else`.

```lisp
(case (:scene state)
  :menu (view-menu state)
  :play (view-play state)
  else empty)
```

---

## 8.14 `do`

Evaluates expressions in order and returns the last one.

```lisp
(do
  (sound/play :hit)
  (assoc state :dead true))
```

Useful for controlled engine side effects.

---

## 8.15 `for`

Maps over a vector and returns a vector.

```lisp
(for [enemy (:enemies state)]
  (enemy-view enemy))
```

This is equivalent to a simple `map`, but easier to read in render trees.

---

## 8.16 Truthiness

Falsey values:

```text
nil
false
```

Everything else is truthy.

---

## 8.17 Map access shorthand

Keywords are callable as map accessors.

```lisp
(:x player)
```

Equivalent to:

```lisp
(get player :x)
```

Nested access:

```lisp
(get-in state [:player :x])
```

---

## 8.18 No mutation in v0.1

Glyph values are treated as immutable from script.

These functions return new values:

```lisp
(assoc map :x 10)
(update map :score + 1)
(update-in state [:player :x] + 5)
```

The original value is not modified from the script author’s perspective.

The runtime may optimize internally later.

---

# 9. Standard library specification

The standard library is intentionally small.

Every standard function should be:

```text
easy to document
safe
deterministic where possible
useful for arcade games
simple to implement in C++
```

---

## 9.1 Arithmetic

```lisp
(+ a b ...)
(- a b)
(* a b ...)
(/ a b)
(mod a b)
(abs x)
(floor x)
(ceil x)
(round x)
(min a b ...)
(max a b ...)
(clamp x lo hi)
(lerp a b t)
```

Examples:

```lisp
(+ 1 2 3)
(clamp y 0 640)
(lerp x target 0.1)
```

---

## 9.2 Comparison

```lisp
(= a b)
(not= a b)
(< a b)
(<= a b)
(> a b)
(>= a b)
```

---

## 9.3 Boolean logic

```lisp
(not x)
(and a b ...)
(or a b ...)
```

`and` and `or` should short-circuit.

They may be implemented as special forms or native functions with lazy evaluation support.

---

## 9.4 Math and geometry

```lisp
(sin x)
(cos x)
(tan x)
(sqrt x)

(deg->rad deg)
(rad->deg rad)

(dist x1 y1 x2 y2)
(angle-to x1 y1 x2 y2)
```

All trigonometric functions operate on radians unless otherwise specified.

---

## 9.5 Strings

```lisp
(str a b ...)
(substr s start len)
(string-length s)
```

Example:

```lisp
(str "Score " (:score state))
```

---

## 9.6 Maps

Maps are keyword-keyed dictionaries in v0.1.

```lisp
(get map key)
(get map key fallback)

(get-in map path)

(assoc map key value)
(assoc-in map path value)

(update map key fn arg...)
(update-in map path fn arg...)

(keys map)
(values map)
(has? map key)
(merge a b)
```

Examples:

```lisp
(get player :x)
(get player :hp 3)

(assoc player :x 100)

(update state :score + 1)

(update-in state [:player :x] + (* 100 dt))
```

---

## 9.7 Vectors

```lisp
(count xs)
(empty? xs)

(first xs)
(last xs)
(nth xs i)
(nth xs i fallback)

(conj xs value)
(push xs value)
(pop xs)

(slice xs start end)

(map xs fn)
(filter xs fn)
(remove xs fn)
(any xs fn)
(all xs fn)
(find xs fn)

(range end)
(range start end)
```

Examples:

```lisp
(count enemies)

(for [enemy enemies]
  (enemy-view enemy))

(filter bullets on-screen?)
```

---

## 9.8 Random

```lisp
(rand)
(rand-int max)
(rand-range min max)
(choose xs)
(chance p)
```

Examples:

```lisp
(rand-int 3)
(rand-range 0 360)
(choose [:red :blue :green])
(chance 0.25)
```

For v0.1, random may use engine-level RNG.

Long-term, add seeded RNG for deterministic daily challenges and replays.

---

## 9.9 Input

```lisp
(pressed? action)
(held? action)
(released? action)

(axis action)

(swipe? dir)

(pointer-x)
(pointer-y)
(pointer-pos)

(pointer-held?)
(pointer-pressed?)
(pointer-released?)
```

Examples:

```lisp
(pressed? :tap)
(axis :move-x)
(swipe? :left)
(pointer-pos)
```

Default actions:

```text
:tap
:confirm
:cancel
:move-x
:move-y
:left
:right
:up
:down
```

---

## 9.10 Time

Game simulation receives time through `dt`.

Optional helpers:

```lisp
(now)
(tick value dt)
(after timer duration)
(loop-timer timer duration)
```

Example:

```lisp
(update state :spawn-timer + dt)
```

---

## 9.11 Collision

Rect helper:

```lisp
(rect-box x y w h)
```

Rect format:

```lisp
{:x 10 :y 20 :w 16 :h 16}
```

Functions:

```lisp
(overlap? a b)
(point-in-rect? x y rect)
(rect-center rect)

(circle-box x y r)
(circle-overlap? a b)
(point-in-circle? x y circle)
```

Collision in Glyph is explicit. The engine provides helpers, but scripts decide what happens.

---

## 9.12 Audio

Audio functions enqueue engine commands.

```lisp
(sound/play :hit)
(sound/play :hit :volume 0.8 :pitch 1.1)

(music/play :theme)
(music/stop)
(music/set-volume 0.5)
```

Recommended rule:

```text
Use audio side effects in update, not view.
```

---

## 9.13 Save and scores

```lisp
(save! :slot value)
(load :slot fallback)

(score/submit :main score)
(best-score :main)
```

State must be serializable.

Functions, native handles, and render nodes cannot be saved.

---

## 9.14 Debug

```lisp
(log value)
(debug/text value)
(debug/rect rect)
(debug/point x y)
```

Debug functions may be disabled or stripped in release builds.

---

# 10. Render tree specification

Render functions return render nodes. They do not draw immediately.

The engine compiles the returned tree into draw commands.

## 10.1 Empty

```lisp
empty
```

`empty` renders nothing.

## 10.2 Clear

```lisp
(clear "#101018")
```

Clears the screen to a color.

## 10.3 Group

```lisp
(group
  child
  child
  child)
```

Groups child nodes.

## 10.4 Layer

```lisp
(layer :name :ui
  child...)
```

Layers are mostly organizational in MVP.

## 10.5 Rect

```lisp
(rect :x 10 :y 20 :w 50 :h 50 :color "#fff")
```

## 10.6 Circle

```lisp
(circle :x 180 :y 320 :r 32 :color "#f44")
```

## 10.7 Line

```lisp
(line :x1 0 :y1 0 :x2 100 :y2 100 :width 2 :color "#fff")
```

## 10.8 Sprite

```lisp
(sprite :image :hero
        :x 100
        :y 120
        :frame :idle
        :src [0 0 32 32]
        :origin :center
        :scale 2
        :rotation 0
        :flip-x false
        :flip-y false
        :color "#fff")
```

Required sprite fields:

```text
:image
:x
:y
```

Optional:

```text
:frame
:src
:origin
:scale
:rotation
:flip-x
:flip-y
:color
```

`:src` selects an exact source rectangle from the texture as `[x y w h]`. It can also name
an atlas frame declared on the texture asset:

```lisp
(sprite :image :tiles :src :grass :x 0 :y 0)
```

If `:src` is present it takes precedence over `:frame`. The legacy `:frame :f0`
through `:frame :f7` strip behavior remains supported for existing horizontal
sprite strips.

## 10.9 Text

```lisp
(text :value "Score 10"
      :font :main
      :x 8
      :y 8
      :size 16
      :align :left
      :color "#fff")
```

## 10.10 Camera

```lisp
(camera :x 0 :y 0 :zoom 1
  child...)
```

## 10.11 Transform

```lisp
(transform :x 100 :y 100 :rotation 30 :scale 2
  child...)
```

---

# 11. Game file format

A complete one-file game:

```lisp
(game perfect-shot
  :title "Perfect Shot"
  :size [360 640]

  :assets
  {:hit "hit.wav"
   :font "font.ttf"}

  :initial initial
  :update update
  :view view)

(def initial
  {:angle 0
   :target 90
   :score 0
   :dead false})

(defn update [dt state]
  (if (:dead state)
    (if (pressed? :tap)
      initial
      state)
    (let [s1 (update state :angle + (* 180 dt))]
      (if (pressed? :tap)
        (shoot s1)
        s1))))

(defn shoot [state]
  (let [diff (abs (- (:angle state) (:target state)))]
    (if (< diff 12)
      (do
        (sound/play :hit)
        (assoc state
          :score (+ (:score state) 1)
          :target (rand-range 0 360)))
      (assoc state :dead true))))

(defn view [state]
  (group
    (clear "#050510")

    (circle :x 180 :y 320 :r 120 :color "#222")

    (line :x1 180 :y1 320
          :x2 (+ 180 (* 120 (cos (deg->rad (:angle state)))))
          :y2 (+ 320 (* 120 (sin (deg->rad (:angle state)))))
          :width 4
          :color "#fff")

    (line :x1 180 :y1 320
          :x2 (+ 180 (* 120 (cos (deg->rad (:target state)))))
          :y2 (+ 320 (* 120 (sin (deg->rad (:target state)))))
          :width 4
          :color "#f55")

    (text :value (str (:score state))
          :x 16 :y 16
          :size 32)

    (if (:dead state)
      (text :value "Tap to restart"
            :x 90 :y 500
            :size 20)
      empty)))
```

---

# 12. Multi-game bundle format

A bundle is a native app containing multiple isolated Glyph games.

MVP bundle format:

```lisp
(bundle tiny-arcade
  :title "Tiny Arcade"
  :games [perfect-shot
          stack-tower
          lane-dodger])
```

Each game remains independent.

The native host owns:

```text
menu
game selection
save slots
leaderboards
payment state
settings
analytics
```

Glyph games do not directly own app navigation outside their own state unless explicitly allowed.

---

# 13. Input model

Glyph uses semantic input.

A game should not care whether input came from:

```text
touchscreen
keyboard
gamepad
mouse
web pointer event
```

## Touch-first mapping

For mobile arcade games:

```text
tap       primary action
hold      sustained primary action
swipe     directional gesture
pointer   drag/aim input
```

## Desktop fallback

```text
tap       Space / Enter / mouse click
move-x    A/D or Left/Right
move-y    W/S or Up/Down
cancel    Escape
```

## Controller fallback

```text
tap       A / Cross
cancel    B / Circle
move-x    left stick / d-pad
move-y    left stick / d-pad
```

---

# 14. State, persistence, and serialization

Glyph game state must be plain data.

Serializable values:

```text
nil
bool
number
string
keyword
vector
map
```

Not serializable:

```text
functions
native functions
render nodes
native handles
platform objects
```

## Save format

Initial implementation may use JSON-like serialization.

Example serialized state:

```json
{
  "score": 10,
  "dead": false,
  "player": {
    "x": 100,
    "y": 200
  }
}
```

Keywords may serialize as strings with `:` prefix.

---

# 15. Hot reload

Hot reload is a development-only feature for v0.1.

Pipeline:

```text
file changed
parse new script
evaluate in fresh environment
extract game definition
if success:
  swap update/view functions
  preserve current state
if failure:
  keep old code running
  display error overlay
```

State survives reload because it is plain data.

If state shape changes, no automatic migration is required in v0.1.

Future migration hook:

```lisp
(defn migrate [old-state]
  ...)
```

---

# 16. C++ architecture

The C++ engine is organized into module domains.

```text
engine/
  core/
  platform/
  input/
  assets/
  audio/
  render/
  script/
  game/
  app/
  tools/
```

---

## 16.1 `core`

Responsibilities:

```text
logging
assertions
math types
string interning
error types
time helpers
basic containers
hashing
```

Core types:

```cpp
using u8  = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

struct Vec2 {
    f32 x;
    f32 y;
};

struct Rect {
    f32 x;
    f32 y;
    f32 w;
    f32 h;
};

struct Color {
    f32 r;
    f32 g;
    f32 b;
    f32 a;
};

using StringId = u32;
```

---

## 16.2 `platform`

Responsibilities:

```text
window creation
native app lifecycle
event polling
time source
filesystem access
platform-specific storage path
mobile pause/resume
safe area info
```

Platform event:

```cpp
enum class PlatformEventType {
    TouchDown,
    TouchMove,
    TouchUp,
    MouseDown,
    MouseMove,
    MouseUp,
    KeyDown,
    KeyUp,
    GamepadButtonDown,
    GamepadButtonUp,
    GamepadAxis,
    Resize,
    Pause,
    Resume,
    Quit
};

struct PlatformEvent {
    PlatformEventType type;

    int id = 0;
    float x = 0;
    float y = 0;
    int code = 0;
    float value = 0;
};
```

Platform interface:

```cpp
class IPlatform {
public:
    virtual ~IPlatform() = default;

    virtual bool pollEvent(PlatformEvent& out) = 0;
    virtual double nowSeconds() const = 0;
    virtual void requestQuit() = 0;
    virtual bool shouldQuit() const = 0;
};
```

Initial implementation:

```text
SDLPlatform
```

Later:

```text
IOSPlatform
AndroidPlatform
WebPlatform
```

---

## 16.3 `input`

Responsibilities:

```text
convert platform events to semantic actions
track pressed/held/released
track pointer state
detect swipes
provide game-facing input API
```

Types:

```cpp
struct ButtonState {
    bool down = false;
    bool pressed = false;
    bool released = false;
};

struct PointerState {
    Vec2 position {};
    Vec2 startPosition {};
    bool down = false;
    bool pressed = false;
    bool released = false;
};

enum class SwipeDirection {
    None,
    Left,
    Right,
    Up,
    Down
};

class InputSystem {
public:
    void beginFrame();
    void consume(const PlatformEvent& event);
    void endFrame();

    bool pressed(StringId action) const;
    bool held(StringId action) const;
    bool released(StringId action) const;

    float axis(StringId action) const;

    Vec2 pointerPosition() const;
    bool pointerPressed() const;
    bool pointerHeld() const;
    bool pointerReleased() const;

    bool swipe(SwipeDirection dir) const;
};
```

---

## 16.4 `assets`

Responsibilities:

```text
load asset manifest
resolve keyword asset references
load textures
load fonts
load sounds/music
watch assets in dev mode
release assets when no longer needed
```

Types:

```cpp
enum class AssetType {
    Texture,
    Font,
    Sound,
    Music,
    Script
};

struct AssetHandle {
    u32 id = 0;
};

struct AssetInfo {
    StringId name;
    AssetType type;
    std::string path;
};

class AssetManager {
public:
    void loadManifest(const GlyphValue& manifest);

    AssetHandle texture(StringId name);
    AssetHandle font(StringId name);
    AssetHandle sound(StringId name);
    AssetHandle music(StringId name);
};
```

---

## 16.5 `audio`

Responsibilities:

```text
sound playback
music playback
volume control
audio lifecycle
command queue
```

Audio command:

```cpp
enum class AudioCommandType {
    PlaySound,
    PlayMusic,
    StopMusic,
    SetMusicVolume
};

struct AudioCommand {
    AudioCommandType type;
    StringId asset = 0;
    float volume = 1.0f;
    float pitch = 1.0f;
};
```

Audio system:

```cpp
class AudioSystem {
public:
    void enqueue(const AudioCommand& command);
    void flush();
    void pauseAll();
    void resumeAll();
};
```

Glyph audio functions enqueue commands rather than directly mutating audio backend state immediately.

---

## 16.6 `render`

Responsibilities:

```text
compile Glyph render tree
resolve assets
generate draw commands
submit to backend
manage logical resolution
handle scaling
handle camera/transform stack
```

Draw command:

```cpp
enum class DrawCommandType {
    Clear,
    Rect,
    Circle,
    Line,
    Sprite,
    Text,
    PushTransform,
    PopTransform,
    PushCamera,
    PopCamera
};

struct DrawCommand {
    DrawCommandType type;
    // Payload stored as variant or tagged union.
};
```

Renderer interface:

```cpp
class Renderer {
public:
    void beginFrame();
    void submit(const DrawCommand& command);
    void endFrame();
};
```

Initial backend:

```text
SDL renderer or OpenGL backend
```

The renderer backend should be replaceable.

---

## 16.7 `script`

Responsibilities:

```text
lexing
parsing
AST allocation
value representation
environment
interpreter
native function registration
error reporting
serialization
hot reload
optional garbage collection
```

Submodules:

```text
script/Lexer
script/Parser
script/Ast
script/Value
script/Env
script/VM
script/Native
script/Serializer
script/GC
```

---

## 16.8 `game`

Responsibilities:

```text
load Glyph game definitions
hold current game state
call update/view
manage pause/resume
own fixed timestep accumulator
serialize/restore state
```

Types:

```cpp
struct GameDefinition {
    StringId id;
    std::string title;
    Vec2 logicalSize;

    GlyphValue initialState;
    GlyphValue updateFn;
    GlyphValue viewFn;
    GlyphValue assetManifest;
};

struct GameInstance {
    GameDefinition* definition = nullptr;
    GlyphValue state;

    bool active = false;
    bool paused = false;

    double accumulator = 0.0;
};
```

---

# 17. Core data structures

## 17.1 String interner

Keywords and symbols are interned.

```cpp
class StringInterner {
public:
    StringId intern(std::string_view text);
    std::string_view resolve(StringId id) const;

private:
    std::unordered_map<std::string, StringId> toId_;
    std::vector<std::string> fromId_;
};
```

Benefits:

```text
fast symbol comparison
fast keyword map lookup
smaller values
simpler equality
```

---

## 17.2 Tokens

```cpp
enum class TokenType {
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    LeftBrace,
    RightBrace,
    Number,
    String,
    Symbol,
    Keyword,
    End
};

struct Token {
    TokenType type;
    std::string text;
    double number = 0.0;
    int line = 1;
    int column = 1;
};
```

---

## 17.3 AST

```cpp
enum class AstKind {
    Nil,
    Bool,
    Number,
    String,
    Symbol,
    Keyword,
    List,
    Vector,
    Map
};

struct SourceSpan {
    std::string file;
    int line = 1;
    int column = 1;
};

struct AstNode {
    AstKind kind;
    SourceSpan span;

    double number = 0.0;
    StringId id = 0;

    std::vector<AstNode*> children;
    std::vector<std::pair<AstNode*, AstNode*>> entries;
};
```

AST nodes are allocated in an AST arena owned by the loaded script.

---

## 17.4 Runtime values

```cpp
enum class ValueKind {
    Nil,
    Bool,
    Number,
    String,
    Keyword,
    Vector,
    Map,
    Function,
    NativeFunction
};

struct Object;

struct GlyphValue {
    ValueKind kind;

    bool boolean = false;
    double number = 0.0;
    StringId id = 0;
    Object* object = nullptr;
};
```

Heap object base:

```cpp
enum class ObjectKind {
    String,
    Vector,
    Map,
    Function,
    NativeFunction
};

struct Object {
    ObjectKind kind;
    bool marked = false;
    Object* next = nullptr;
};
```

Vector object:

```cpp
struct VectorObject : Object {
    std::vector<GlyphValue> items;
};
```

Map object:

```cpp
struct MapObject : Object {
    std::unordered_map<StringId, GlyphValue> entries;
};
```

Function object:

```cpp
struct FunctionObject : Object {
    std::vector<StringId> params;
    AstNode* body = nullptr;
    Env* closure = nullptr;
};
```

Native function:

```cpp
using NativeFn = GlyphValue (*)(VM&, const std::vector<GlyphValue>&);

struct NativeFunctionObject : Object {
    StringId name;
    NativeFn fn;
    int minArgs = 0;
    int maxArgs = -1;
};
```

---

# 18. AST and interpreter design

## 18.1 Lexer algorithm

```text
while not end:
  skip whitespace
  skip comments
  read punctuation
  read strings
  read numbers
  read keywords
  read symbols
emit End token
```

## 18.2 Parser algorithm

```text
parse expression:
  number -> number AST
  string -> string AST
  symbol -> symbol AST
  keyword -> keyword AST
  (...) -> list AST
  [...] -> vector AST
  {...} -> map AST
```

Because Glyph is Lisp-like, parsing does not require precedence handling.

## 18.3 Evaluation

Main interpreter function:

```cpp
GlyphValue VM::eval(AstNode* node, Env* env);
```

Evaluation rules:

```text
number evaluates to number
string evaluates to string
keyword evaluates to keyword
symbol looks up environment binding
vector evaluates each item
map evaluates values
list evaluates as special form or function call
```

## 18.4 Environment

```cpp
struct Env {
    Env* parent = nullptr;
    std::unordered_map<StringId, GlyphValue> bindings;
};
```

Lookup:

```text
check current environment
if missing, check parent
repeat until root
if missing, runtime error
```

## 18.5 Function call

```text
evaluate callee
evaluate arguments
if native: call C++ function
if Glyph function:
  create local environment
  bind params
  evaluate body
```

## 18.6 Special forms

Special forms are handled before normal function calls:

```cpp
if head == "def"  -> evalDef
if head == "defn" -> evalDefn
if head == "let"  -> evalLet
if head == "if"   -> evalIf
if head == "cond" -> evalCond
if head == "case" -> evalCase
if head == "do"   -> evalDo
if head == "fn"   -> evalFn
if head == "for"  -> evalFor
```

## 18.7 Native functions

Native functions are C++ functions exposed to Glyph.

Examples:

```text
+
-
*
/
assoc
update-in
sprite
rect
pressed?
sound/play
```

They are registered into the global environment at VM startup.

---

# 19. Memory management and garbage collection

## 19.1 MVP recommendation

For the first implementation, use a simple memory model.

Acceptable MVP path:

```text
AST nodes allocated in script arena
runtime objects allocated as shared pointers OR custom GC heap
clear ownership boundaries
no raw native handles inside Glyph values
```

For a production-ready MVP, two options are acceptable:

## Option A: `std::shared_ptr`

Pros:

```text
simpler
safer initially
fewer custom memory bugs
fast enough for tiny games
```

Cons:

```text
slower
reference cycles can leak
less control
harder to tune
```

This is acceptable if:

```text
Glyph does not support mutation
cycles are not constructible in normal script
performance is measured and sufficient
```

## Option B: mark-and-sweep GC

Pros:

```text
explicit VM ownership
better long-term control
common VM architecture
avoids shared_ptr overhead
```

Cons:

```text
more implementation complexity
root tracking bugs are possible
```

Recommended production direction:

```text
v0 internal prototype: shared_ptr
v0.1 production MVP: mark-and-sweep GC if time permits
```

---

## 19.2 Mark-and-sweep GC

The GC maintains a linked list of allocated heap objects.

```cpp
class GC {
public:
    template <typename T, typename... Args>
    T* allocate(Args&&... args);

    void collect(VM& vm);

private:
    Object* objects_ = nullptr;
    size_t bytesAllocated_ = 0;
    size_t nextCollection_ = 1024 * 1024;

    void markRoots(VM& vm);
    void markValue(GlyphValue value);
    void markObject(Object* object);
    void markEnv(Env* env);
    void sweep();
};
```

## 19.3 Roots

Roots are values that must remain alive.

Roots include:

```text
global environment
current game state
paused game states
current call stack values
loaded game definitions
temporary VM stack values
```

## 19.4 Mark phase

Starting from roots, recursively mark reachable objects:

```text
mark vector items
mark map values
mark function closures
mark environment bindings
```

## 19.5 Sweep phase

Walk all allocated objects.

```text
if marked:
  unmark and keep
else:
  delete
```

## 19.6 GC timing

To reduce temporary-root complexity, MVP may only run GC:

```text
after update/view evaluation completes
between frames
after hot reload
after game switch
after explicit memory threshold
```

Avoid running GC halfway through native function evaluation until the VM has robust temporary root tracking.

---

# 20. Rendering architecture

Glyph render nodes are plain maps.

Example:

```lisp
(sprite :image :hero :x 100 :y 100)
```

Internally returns something equivalent to:

```lisp
{:node :sprite
 :image :hero
 :x 100
 :y 100}
```

The render compiler walks the tree and emits draw commands.

## Render compiler algorithm

```text
compile node:
  if empty: do nothing
  if group: compile children
  if clear: emit clear command
  if rect: emit rect command
  if circle: emit circle command
  if line: emit line command
  if sprite: resolve texture and emit sprite command
  if text: resolve font and emit text command
  if camera: push camera, compile children, pop camera
  if transform: push transform, compile children, pop transform
```

## Logical resolution

Each game declares a logical resolution:

```lisp
:size [360 640]
```

Renderer scales logical coordinates to device pixels.

This keeps game code independent from actual device dimensions.

---

# 21. Audio architecture

Audio is command-based.

Glyph:

```lisp
(sound/play :hit)
```

C++:

```text
enqueue PlaySound(:hit)
flush audio queue after update
```

This avoids direct backend mutation during arbitrary script evaluation.

Audio commands should be ignored or deduplicated if they are accidentally triggered from `view`.

Development mode may warn:

```text
Warning: sound/play called during view. Audio side effects belong in update.
```

---

# 22. Asset system

Assets are declared in game metadata.

```lisp
(game stack-tower
  :title "Stack Tower"
  :size [360 640]
  :assets
  {:block "block.png"
   :tiles {:path "sheets/tiles.png"
           :frames {:grass [0 0 16 16]
                    :coin [32 16 16 16]}}
   :hit "hit.wav"
   :font "font.ttf"}
  :initial initial
  :update update
  :view view)
```

Asset manager resolves keywords:

```lisp
(sprite :image :block ...)
(sprite :image :tiles :src :coin ...)
(sound/play :hit)
(text :font :font ...)
```

Production build should package assets into a deterministic bundle.

Development build may load from filesystem.

---

# 23. Mobile support

Mobile is a first-class product target.

## 23.1 Core mobile assumptions

```text
portrait-first
touch-first
short sessions
fast startup
small memory footprint
pause/resume safe
safe area aware
offline capable
battery-conscious
```

## 23.2 iOS

Implementation path:

```text
C++ core
Objective-C++ app bridge
SDL initially acceptable
Metal backend later if needed
AVAudioSession handling
iOS lifecycle pause/resume
safe area handling
App Store packaging
```

Must support:

```text
app background pause
audio interruptions
memory warnings
orientation lock
touch input
IAP integration outside Glyph
```

## 23.3 Android

Implementation path:

```text
C++ core
SDL Activity or NativeActivity
JNI bridge where needed
OpenGL ES/Vulkan later
Android lifecycle handling
Google Play packaging
```

Must support:

```text
back button
pause/resume
screen size variation
low memory events
touch input
billing outside Glyph
```

## 23.4 App shell ownership

For commercial apps, native shell owns:

```text
menus
store purchases
analytics
leaderboards
user identity
settings
content catalog
push notifications
remote config
```

Glyph games should not directly access those systems except through narrow safe APIs.

---

# 24. Web support

Web support is desirable but not required for the first production mobile build.

Target:

```text
Emscripten
HTML canvas
WebGL/OpenGL backend
browser input mapping
asset preloading
local storage saves
```

Web is valuable for:

```text
demos
marketing
shareable games
internal testing
daily challenge previews
```

---

# 25. Testing strategy

Production-ready MVP requires tests.

## 25.1 Lexer tests

Test:

```text
numbers
strings
keywords
symbols
comments
punctuation
line/column tracking
invalid tokens
```

## 25.2 Parser tests

Test:

```text
lists
vectors
maps
nested expressions
malformed forms
source spans
```

## 25.3 Interpreter tests

Test:

```text
literals
symbol lookup
def
defn
let
if
cond
case
do
fn
for
native calls
closures
errors
```

## 25.4 Standard library tests

Test:

```text
math
maps
vectors
strings
random range bounds
collision helpers
serialization
```

## 25.5 Engine integration tests

Test:

```text
load game
call update
call view
compile render tree
serialize state
restore state
hot reload success
hot reload failure preserves old code
```

## 25.6 Sample game tests

For each sample game:

```text
script parses
game definition extracts
initial state serializes
update runs for 1000 frames
view returns valid render tree
no memory leaks under test harness
```

---

# 26. Performance targets

Glyph is designed for small games, but the production runtime should be efficient.

## MVP targets

```text
60 FPS on modern mobile devices
fixed update at 60 Hz
view evaluation under 2 ms for simple games
render tree compile under 1 ms for simple games
cold game load under 500 ms from packaged assets
warm game switch under 100 ms
no unbounded memory growth during normal play
```

## Script complexity budget

Recommended per-game limits:

```text
script size: 16 KB to 128 KB
active entities: under 500
render nodes/frame: under 1000
texture memory/game: under 16 MB
audio memory/game: under 8 MB
```

These are not hard VM limits initially, but should guide product design.

---

# 27. Security and sandboxing

Glyph script is sandboxed.

Glyph cannot directly:

```text
access arbitrary filesystem
open network sockets
call native platform APIs
load dynamic libraries
inspect user identity
make purchases
access other games' state
access raw memory
```

Allowed APIs are explicitly registered native functions.

All native functions must validate arguments.

Script errors should fail safely:

```text
development: show overlay
production: fail game instance gracefully, return to host shell
```

---

# 28. Build system and repo layout

Recommended C++ standard:

```text
C++20
```

Recommended build tool:

```text
CMake
```

Initial dependencies:

```text
SDL3 or SDL2
stb_image or SDL_image
miniaudio or SDL_mixer
freetype or SDL_ttf
Catch2 or doctest for tests
```

Prefer minimal dependencies.

## Repo layout

```text
glyph/
  README.md
  CMakeLists.txt

  src/
    core/
      Types.h
      Log.h
      StringInterner.h
      StringInterner.cpp
      Error.h

    platform/
      IPlatform.h
      SDLPlatform.h
      SDLPlatform.cpp

    input/
      InputSystem.h
      InputSystem.cpp

    assets/
      AssetManager.h
      AssetManager.cpp

    audio/
      AudioSystem.h
      AudioSystem.cpp

    render/
      Renderer.h
      Renderer.cpp
      RenderCompiler.h
      RenderCompiler.cpp
      DrawCommand.h

    script/
      Token.h
      Lexer.h
      Lexer.cpp
      Ast.h
      Parser.h
      Parser.cpp
      Value.h
      Env.h
      VM.h
      VM.cpp
      Native.h
      Native.cpp
      Serializer.h
      Serializer.cpp
      GC.h
      GC.cpp

    game/
      GameDefinition.h
      GameInstance.h
      GameHost.h
      GameHost.cpp

    app/
      Main.cpp

  games/
    perfect-shot/
      game.glyph
      assets/

    stack-tower/
      game.glyph
      assets/

    lane-dodger/
      game.glyph
      assets/

  tests/
    script/
      LexerTests.cpp
      ParserTests.cpp
      InterpreterTests.cpp
      StdLibTests.cpp

    game/
      GameLoadTests.cpp
```

---

# 29. Implementation roadmap

## Milestone 1: Script core

Deliver:

```text
lexer
parser
AST
value system
environment
interpreter
basic errors
native function registration
```

Language support:

```text
numbers
strings
booleans
nil
keywords
vectors
maps
def
defn
fn
let
if
cond
case
do
for
```

Acceptance test:

```lisp
(defn add [a b] (+ a b))
(add 1 2)
```

returns `3`.

---

## Milestone 2: Standard library core

Deliver:

```text
math functions
comparison
boolean
string functions
map functions
vector functions
random
collision helpers
```

Acceptance test:

```lisp
(update-in {:player {:x 10}} [:player :x] + 5)
```

returns:

```lisp
{:player {:x 15}}
```

---

## Milestone 3: Render tree

Deliver:

```text
render node native functions
render compiler
basic renderer
clear
rect
circle
line
text placeholder
group
camera/transform stack
```

Acceptance test:

```lisp
(defn view [state]
  (rect :x 10 :y 10 :w 20 :h 20 :color "#fff"))
```

renders a rectangle.

---

## Milestone 4: Game loop

Deliver:

```text
GameDefinition
GameInstance
fixed timestep
update call
view call
state persistence in memory
input bridge
```

Acceptance test:

```lisp
(def initial {:x 0})

(defn update [dt state]
  (update state :x + (* 100 dt)))

(defn view [state]
  (rect :x (:x state) :y 100 :w 32 :h 32 :color "#fff"))
```

renders a moving rectangle.

---

## Milestone 5: Audio and assets

Deliver:

```text
asset manifest
texture loading
sprite rendering
font rendering
sound effects
music playback
audio command queue
```

Acceptance test:

```lisp
(sprite :image :hero :x 100 :y 100)
(sound/play :hit)
```

works.

---

## Milestone 6: Hot reload

Deliver:

```text
file watcher
reload script
preserve state on success
preserve old code on failure
error overlay
```

Acceptance test:

```text
editing view changes render immediately
syntax error does not crash runtime
fixing syntax error reloads successfully
```

---

## Milestone 7: Sample games

Implement:

```text
desktop SDL target with SDL_image, SDL_ttf, SDL_mixer
scene stack navigation with navigation/push and navigation/pop
index scene that links to playable game scenes
Perfect Shot
Stack Tower
Lane Dodger
```

Acceptance criteria:

```text
each game fits in one file
each game can be opened from index.glyph and popped back to the index
each game runs at 60 FPS
each game has score
each game uses PNG sprites, TTF text, sound, and input-driven animation
each game uses only approved MVP APIs
```

---

## Milestone 8: Mobile production path

Deliver:

```text
portrait logical resolution
touch input
pause/resume
safe area handling
save storage
mobile build target
release packaging docs
```

Acceptance test:

```text
sample games run on iOS simulator/device or Android emulator/device
pause/resume does not lose state
touch input works
```

---

# 30. First sample games

## 30.1 Perfect Shot

Mechanic:

```text
rotating aim line
static target line
tap when aligned
miss ends run
```

Tests:

```text
tap near target increments score
tap far from target ends game
target changes after hit
```

## 30.2 Stack Tower

Mechanic:

```text
moving block
tap to place
overlap trims next block
miss ends run
```

Tests:

```text
perfect placement keeps width
partial placement reduces width
no overlap ends game
score increments on valid placement
```

## 30.3 Lane Dodger

Mechanic:

```text
three lanes
swipe left/right
obstacles fall
collision ends run
survival increases score
```

Tests:

```text
swipe changes lane
obstacles spawn
obstacles move
same lane collision ends game
```

---

# 31. Long-term direction

After MVP, evaluate based on real games built with Glyph.

Possible future additions:

```text
bytecode VM
seeded deterministic RNG
daily challenge framework
replay recording
leaderboard integration
sprite atlas packing
particle primitives
tween helpers
tilemap support
simple animation declarations
state migration hooks
web export
game bundle carousel
AI generation tooling
limited module system
console ports
```

Avoid adding these until the MVP games prove that the language and runtime feel good.

---

# Final scope statement

Glyph v0.1 is a production-ready, intentionally small runtime for shipping tiny 2D games.

It includes:

```text
a tiny Lisp-like scripting language
plain-data state
fixed timestep update
declarative render trees
mobile-friendly input
basic rendering
basic audio
asset loading
serialization
hot reload for development
clear platform boundaries
sample games
tests
```

It does not include:

```text
3D
editor
physics engine
networking
ECS
macros
modules
console support
complex engine object model
```

The success of Glyph is not measured by how many engine features it has.

It is measured by whether a developer can make a fun, polished arcade game in a tiny script file, iterate live, and ship it inside a real product.
