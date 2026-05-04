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
      (update-in state [:x] + (* 100 dt)))

    (defn view [state]
      (rect :x (:x state) :y 100 :w 32 :h 32 :color "#fff"))
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

} // namespace

int main() {
  try {
    testMilestoneAcceptanceGameLoop();
    testAccumulatorPauseAndReset();
    testInputBridge();
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << "ScriptError: " << error.what() << '\n';
    return 1;
  }

  std::cout << "glyph game host tests passed\n";
  return 0;
}
