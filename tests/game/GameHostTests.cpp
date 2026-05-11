#include "game/GameHost.h"
#include "script/Error.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

std::string readFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  require(static_cast<bool>(input), "open " + path.string());
  std::ostringstream source;
  source << input.rdbuf();
  return source.str();
}

std::filesystem::path repoRoot() {
  const auto cwd = std::filesystem::current_path();
  if (std::filesystem::exists(cwd / "examples")) {
    return cwd;
  }
  if (std::filesystem::exists(cwd.parent_path() / "examples")) {
    return cwd.parent_path();
  }
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
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
               :tiles {:path "assets/sheets/tiles.png"
                       :frames {:coin [32 16 12 14]
                                :door [64 16 24 32]}}
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
        (sprite :image :tiles :src :coin :x 40 :y 20)
        (text :font :main :value "Ready" :x 2 :y 3)))
  )");

  require(host.assets().assets().size() == 5, "asset manifest loaded");
  const auto hero = host.vm().interner().intern(":hero");
  const auto tiles = host.vm().interner().intern(":tiles");
  const auto coin = host.vm().interner().intern(":coin");
  const auto mainFont = host.vm().interner().intern(":main");
  const auto hit = host.vm().interner().intern(":hit");
  const auto theme = host.vm().interner().intern(":theme");

  require(host.assets().texture(hero).id != 0, "texture asset resolved");
  require(host.assets().texture(tiles).id != 0, "atlas texture asset resolved");
  const auto* coinFrame = host.assets().frame(tiles, coin);
  require(coinFrame != nullptr, "atlas frame resolved");
  require(coinFrame->x == 32 && coinFrame->y == 16 && coinFrame->w == 12 && coinFrame->h == 14,
          "atlas frame rect");
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
  require(commands.size() == 3, "sprite, atlas sprite, and text commands");
  require(commands[0].type == glyph::render::DrawCommandType::Sprite, "sprite command");
  require(commands[0].asset.id == host.assets().texture(hero).id, "sprite texture handle");
  require(commands[1].type == glyph::render::DrawCommandType::Sprite, "atlas sprite command");
  require(commands[1].asset.id == host.assets().texture(tiles).id, "atlas sprite texture handle");
  require(commands[1].hasSourceRect, "atlas sprite source rect enabled");
  require(commands[1].sourceRect.x == 32 && commands[1].sourceRect.y == 16 &&
              commands[1].sourceRect.w == 12 && commands[1].sourceRect.h == 14,
          "atlas sprite source rect");
  require(commands[2].type == glyph::render::DrawCommandType::Text, "text command");
  require(commands[2].asset.id == host.assets().font(mainFont).id, "text font handle");

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

void testNavigationCommandsQueueFromScript() {
  glyph::game::GameHost host;
  host.loadSource(R"(
    (game nav-test
      :title "Nav Test"
      :size [160 120]
      :initial initial
      :update update
      :view view)

    (def initial {:phase 0})

    (defn update [dt state]
      (case (:phase state)
        0 (do
            (navigation/push "perfect-shot/game.glyph")
            (assoc state :phase 1))
        1 (do
            (navigation/pop)
            (assoc state :phase 2))
        else state))

    (defn view [state] empty)
  )");

  host.tick(1.0 / 60.0);
  require(host.navigation().commands().size() == 1, "navigation push queued");
  require(host.navigation().commands()[0].type == glyph::game::NavigationCommandType::Push,
          "push command type");
  require(host.navigation().commands()[0].target == "perfect-shot/game.glyph", "push target");

  host.navigation().clear();
  host.tick(1.0 / 60.0);
  require(host.navigation().commands().size() == 1, "navigation pop queued");
  require(host.navigation().commands()[0].type == glyph::game::NavigationCommandType::Pop,
          "pop command type");
}

void testArcadeExampleScenesCompileAndRender() {
  const auto root = repoRoot();
  const std::vector<std::filesystem::path> scenes{
      root / "examples/arcade/index.glyph",
      root / "examples/arcade/asteroid-belt/game.glyph",
      root / "examples/arcade/chef-chaos/game.glyph",
      root / "examples/arcade/circuit-keep/game.glyph",
      root / "examples/arcade/crown-cavern/game.glyph",
      root / "examples/arcade/fishing-cove/game.glyph",
      root / "examples/arcade/fussball-fever/game.glyph",
      root / "examples/arcade/lane-dodger/game.glyph",
      root / "examples/arcade/particle-swirl/game.glyph",
      root / "examples/arcade/perfect-shot/game.glyph",
      root / "examples/arcade/platform-hop/game.glyph",
      root / "examples/arcade/ski-slalom/game.glyph",
      root / "examples/arcade/stack-tower/game.glyph",
      root / "examples/arcade/tank-siege/game.glyph",
  };

  for (const auto& scene : scenes) {
    glyph::game::GameHost host;
    host.loadSource(readFile(scene), scene.string());
    host.tick(1.0 / 60.0);
    require(!host.renderView().empty(), "arcade scene renders " + scene.string());
    require(!host.assets().assets().empty(), "arcade scene has assets " + scene.string());
    for (const auto& asset : host.assets().assets()) {
      const std::filesystem::path assetPath(asset.path);
      if (!assetPath.is_absolute()) {
        require(std::filesystem::exists(scene.parent_path() / assetPath),
                "arcade asset exists " + (scene.parent_path() / assetPath).string());
      }
    }
  }
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
    testNavigationCommandsQueueFromScript();
    testArcadeExampleScenesCompileAndRender();
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << "ScriptError: " << error.what() << '\n';
    return 1;
  }

  std::cout << "glyph game host tests passed\n";
  return 0;
}
