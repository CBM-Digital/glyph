#include "game/GameHost.h"
#include "script/Error.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void requireNumber(const glyph::script::Value& value, double expected, const std::string& message) {
  require(value.kind == glyph::script::ValueKind::Number, message + " kind");
  require(std::fabs(value.number - expected) < 0.000001, message);
}

std::string movingRectGame() {
  return R"(
    (game moving-rect
      :title "Moving Rect"
      :size [360 640]
      :initial initial
      :update update
      :view view)

    (def initial {:x 0})

    (defn update [dt state]
      (update state :x + (* 100 dt)))

    (defn view [state]
      (rect :x (:x state) :y 100 :w 32 :h 32 :color "#fff"))
  )";
}

std::string reloadableGame(const std::string& color) {
  return R"(
    (game reloadable
      :title "Reloadable"
      :size [160 120]
      :initial initial
      :update update
      :view view)

    (def initial {:x 0 :tag :same})

    (defn update [dt state]
      (update state :x + 1))

    (defn view [state]
      (rect :x (:x state) :y 10 :w 20 :h 20 :color ")" + color + R"("))
  )";
}

void testMilestoneAcceptanceGameLoop() {
  glyph::game::GameHost host;
  host.loadSource(movingRectGame());

  require(host.definition().title == "Moving Rect", "game title");
  require(host.definition().logicalSize.x == 360.0f, "logical width");
  require(host.definition().logicalSize.y == 640.0f, "logical height");
  requireNumber(host.state().map->at(host.vm().interner().intern(":x")), 0.0, "initial x");

  host.tick(1.0 / 60.0);
  requireNumber(host.state().map->at(host.vm().interner().intern(":x")), 100.0 / 60.0,
                "state advances one fixed step");

  const auto commands = host.renderView();
  require(commands.size() == 1, "view compiles one command");
  require(commands[0].type == glyph::render::DrawCommandType::Rect, "view compiles rect");
  require(std::fabs(commands[0].x - 100.0 / 60.0) < 0.000001, "rect follows state x");
  require(commands[0].y == 100.0, "rect y");
}

void testAccumulatorPauseAndReset() {
  glyph::game::GameHost host;
  host.loadSource(movingRectGame());

  host.tick((1.0 / 60.0) * 2.5);
  requireNumber(host.state().map->at(host.vm().interner().intern(":x")), 200.0 / 60.0,
                "accumulator runs whole fixed steps");

  host.setPaused(true);
  host.tick(1.0);
  requireNumber(host.state().map->at(host.vm().interner().intern(":x")), 200.0 / 60.0,
                "pause blocks updates");

  host.reset();
  requireNumber(host.state().map->at(host.vm().interner().intern(":x")), 0.0, "reset restores initial");
}

void testInputBridge() {
  glyph::game::GameHost host;
  host.loadSource(R"(
    (game input-test
      :title "Input Test"
      :size [100 100]
      :initial initial
      :update update
      :view view)

    (def initial {:taps 0 :px 0})

    (defn update [dt state]
      (if (pressed? :tap)
        (assoc state
          :taps (+ (:taps state) 1)
          :px (pointer-x))
        state))

    (defn view [state]
      empty)
  )");

  const auto tap = host.vm().interner().intern(":tap");
  host.input().beginFrame();
  host.input().setPointerDown(true, glyph::Vec2{42.0f, 9.0f});
  host.input().setActionDown(tap, true);
  host.tick(1.0 / 60.0);

  requireNumber(host.state().map->at(host.vm().interner().intern(":taps")), 1.0, "input press observed");
  requireNumber(host.state().map->at(host.vm().interner().intern(":px")), 42.0, "pointer x observed");

  host.input().beginFrame();
  host.tick(1.0 / 60.0);
  requireNumber(host.state().map->at(host.vm().interner().intern(":taps")), 1.0,
                "pressed is frame-local");
}

void testAssetsRenderingAndAudioQueue() {
  glyph::game::GameHost host;
  host.loadSource(R"(
    (game asset-audio-test
      :title "Asset Audio Test"
      :size [160 120]
      :assets {:hero "assets/hero.png"
               :main "assets/main.ttf"
               :hit "assets/hit.wav"
               :theme "assets/theme.ogg"}
      :initial initial
      :update update
      :view view)

    (def initial {:played false})

    (defn update [dt state]
      (if (:played state)
        state
        (do
          (sound/play :hit :volume 0.5 :pitch 1.25)
          (music/play :theme :volume 0.25)
          (music/set-volume 0.75)
          (assoc state :played true))))

    (defn view [state]
      (group
        (sprite :image :hero :x 10 :y 20)
        (text :font :main :value "Ready" :x 2 :y 3)))
  )");

  require(host.assets().assets().size() == 4, "asset manifest loaded");
  const auto hero = host.vm().interner().intern(":hero");
  const auto mainFont = host.vm().interner().intern(":main");
  const auto hit = host.vm().interner().intern(":hit");
  const auto theme = host.vm().interner().intern(":theme");

  require(host.assets().texture(hero).id != 0, "texture asset resolved");
  require(host.assets().font(mainFont).id != 0, "font asset resolved");
  require(host.assets().sound(hit).id != 0, "sound asset resolved");
  require(host.assets().music(theme).id != 0, "music asset resolved");

  host.tick(1.0 / 60.0);
  require(host.audio().commands().size() == 3, "audio commands queued");
  require(host.audio().commands()[0].type == glyph::audio::AudioCommandType::PlaySound,
          "sound/play queues sound");
  require(host.audio().commands()[0].asset == hit, "sound asset id");
  require(host.audio().commands()[0].handle.id == host.assets().sound(hit).id, "sound handle");
  require(std::fabs(host.audio().commands()[0].volume - 0.5f) < 0.000001f, "sound volume");
  require(std::fabs(host.audio().commands()[0].pitch - 1.25f) < 0.000001f, "sound pitch");
  require(host.audio().commands()[1].type == glyph::audio::AudioCommandType::PlayMusic,
          "music/play queues music");
  require(host.audio().commands()[1].handle.id == host.assets().music(theme).id, "music handle");
  require(host.audio().commands()[2].type == glyph::audio::AudioCommandType::SetMusicVolume,
          "music/set-volume queues volume");

  const auto commands = host.renderView();
  require(commands.size() == 2, "sprite and text commands");
  require(commands[0].type == glyph::render::DrawCommandType::Sprite, "sprite command");
  require(commands[0].asset.id == host.assets().texture(hero).id, "sprite texture handle");
  require(commands[1].type == glyph::render::DrawCommandType::Text, "text command");
  require(commands[1].asset.id == host.assets().font(mainFont).id, "text font handle");

  host.audio().flush();
  require(host.audio().commands().empty(), "audio flush clears queue");
}

void testHotReloadPreservesStateAndSwapsCode() {
  glyph::game::GameHost host;
  host.loadSource(reloadableGame("#fff"));
  host.tick(1.0 / 60.0);
  requireNumber(host.state().map->at(host.vm().interner().intern(":x")), 1.0, "pre-reload state");

  const auto beforeCommands = host.renderView();
  require(beforeCommands[0].color == "#fff", "initial view color");

  const auto result = host.reloadSourcePreservingState(reloadableGame("#f55"));
  require(result.success, "hot reload succeeds");
  require(host.lastReloadError().empty(), "reload error clears on success");
  requireNumber(host.state().map->at(host.vm().interner().intern(":x")), 1.0, "reload preserves state");
  require(glyph::script::valueToString(host.state().map->at(host.vm().interner().intern(":tag")),
                                       host.vm().interner()) == ":same",
          "reload re-interns state keywords");

  const auto afterCommands = host.renderView();
  require(afterCommands[0].color == "#f55", "reload swaps view code");

  host.tick(1.0 / 60.0);
  requireNumber(host.state().map->at(host.vm().interner().intern(":x")), 2.0, "reloaded update still runs");
}

void testHotReloadFailureKeepsOldCodeRunning() {
  glyph::game::GameHost host;
  host.loadSource(reloadableGame("#fff"));
  host.tick(1.0 / 60.0);

  const auto result = host.reloadSourcePreservingState("(game broken");
  require(!result.success, "bad reload fails");
  require(!host.lastReloadError().empty(), "bad reload stores error");
  requireNumber(host.state().map->at(host.vm().interner().intern(":x")), 1.0,
                "bad reload preserves current state");
  require(host.renderView()[0].color == "#fff", "bad reload preserves old view");

  host.tick(1.0 / 60.0);
  requireNumber(host.state().map->at(host.vm().interner().intern(":x")), 2.0,
                "bad reload keeps old update running");

  const auto recovery = host.reloadSourcePreservingState(reloadableGame("#0f0"));
  require(recovery.success, "reload recovers after failure");
  require(host.lastReloadError().empty(), "recovery clears error");
  require(host.renderView()[0].color == "#0f0", "recovery swaps view");
}

} // namespace

int main() {
  try {
    testMilestoneAcceptanceGameLoop();
    testAccumulatorPauseAndReset();
    testInputBridge();
    testAssetsRenderingAndAudioQueue();
    testHotReloadPreservesStateAndSwapsCode();
    testHotReloadFailureKeepsOldCodeRunning();
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << "ScriptError: " << error.what() << '\n';
    return 1;
  }

  std::cout << "glyph game host tests passed\n";
  return 0;
}
