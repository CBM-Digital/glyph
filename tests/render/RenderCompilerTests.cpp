#include "render/RenderCompiler.h"
#include "render/Renderer.h"
#include "script/Error.h"
#include "script/VM.h"

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

void requireNumber(double actual, double expected, const std::string& message) {
  require(std::fabs(actual - expected) < 0.000001, message);
}

void testMilestoneAcceptance() {
  glyph::script::VM vm;
  const auto tree = vm.evalSource(R"(
    (defn view [state]
      (rect :x 10 :y 10 :w 20 :h 20 :color "#fff"))
    (view nil)
  )");

  glyph::render::RenderCompiler compiler(vm.interner());
  const auto commands = compiler.compile(tree);
  require(commands.size() == 1, "acceptance emits one draw command");
  require(commands[0].type == glyph::render::DrawCommandType::Rect, "acceptance emits rect");
  requireNumber(commands[0].x, 10.0, "rect x");
  requireNumber(commands[0].y, 10.0, "rect y");
  requireNumber(commands[0].w, 20.0, "rect w");
  requireNumber(commands[0].h, 20.0, "rect h");
  require(commands[0].color == "#fff", "rect color");

  glyph::render::CommandBufferRenderer renderer;
  renderer.beginFrame();
  for (const auto& command : commands) {
    renderer.submit(command);
  }
  renderer.endFrame();
  require(renderer.commands().size() == 1, "acceptance renderer receives one command");
  require(renderer.commands()[0].type == glyph::render::DrawCommandType::Rect,
          "acceptance renderer receives rect");
}

void testGroupAndPrimitiveCommands() {
  glyph::script::VM vm;
  const auto tree = vm.evalSource(R"(
    (group
      (clear "#101018")
      (circle :x 20 :y 30 :r 8 :color "#f44")
      (line :x1 0 :y1 1 :x2 2 :y2 3 :width 4 :color "#0f0")
      (text :value (str "Score " 7) :x 5 :y 6 :size 12 :color "#fff")
      empty)
  )");

  glyph::render::RenderCompiler compiler(vm.interner());
  const auto commands = compiler.compile(tree);
  require(commands.size() == 4, "group emits child draw commands");
  require(commands[0].type == glyph::render::DrawCommandType::Clear, "clear command");
  require(commands[0].color == "#101018", "clear color");
  require(commands[1].type == glyph::render::DrawCommandType::Circle, "circle command");
  requireNumber(commands[1].r, 8.0, "circle radius");
  require(commands[2].type == glyph::render::DrawCommandType::Line, "line command");
  requireNumber(commands[2].width, 4.0, "line width");
  require(commands[3].type == glyph::render::DrawCommandType::Text, "text command");
  require(commands[3].text == "Score 7", "text value");
  requireNumber(commands[3].scale, 12.0, "text size");
}

void testCameraTransformAndSpriteCommands() {
  glyph::script::VM vm;
  const auto tree = vm.evalSource(R"(
    (camera :x 1 :y 2 :zoom 3
      (transform :x 4 :y 5 :rotation 6 :scale 7
        (sprite :image :hero :x 8 :y 9 :frame :idle :src [16 32 24 40]
                :scale 2 :origin :center :pivot [0.25 0.75] :flip-x true :flip-y false)))
  )");

  glyph::render::RenderCompiler compiler(vm.interner());
  const auto commands = compiler.compile(tree);
  require(commands.size() == 5, "camera/transform command count");
  require(commands[0].type == glyph::render::DrawCommandType::PushCamera, "push camera");
  requireNumber(commands[0].zoom, 3.0, "camera zoom");
  require(commands[1].type == glyph::render::DrawCommandType::PushTransform, "push transform");
  requireNumber(commands[1].rotation, 6.0, "transform rotation");
  require(commands[2].type == glyph::render::DrawCommandType::Sprite, "sprite command");
  require(commands[2].image == vm.interner().intern(":hero"), "sprite image");
  require(commands[2].frame == vm.interner().intern(":idle"), "sprite frame");
  require(commands[2].hasSourceRect, "sprite source rect enabled");
  require(commands[2].sourceRect.x == 16, "sprite source rect x");
  require(commands[2].sourceRect.y == 32, "sprite source rect y");
  require(commands[2].sourceRect.w == 24, "sprite source rect w");
  require(commands[2].sourceRect.h == 40, "sprite source rect h");
  requireNumber(commands[2].x, 8.0, "sprite x");
  requireNumber(commands[2].scale, 2.0, "sprite scale");
  require(commands[2].hasOrigin, "sprite origin enabled");
  requireNumber(commands[2].originX, 0.5, "sprite origin x");
  requireNumber(commands[2].originY, 0.5, "sprite origin y");
  require(commands[2].hasPivot, "sprite pivot enabled");
  requireNumber(commands[2].pivotX, 0.25, "sprite pivot x");
  requireNumber(commands[2].pivotY, 0.75, "sprite pivot y");
  require(commands[2].flipX, "sprite flip x");
  require(!commands[2].flipY, "sprite flip y");
  require(commands[3].type == glyph::render::DrawCommandType::PopTransform, "pop transform");
  require(commands[4].type == glyph::render::DrawCommandType::PopCamera, "pop camera");
}

} // namespace

int main() {
  try {
    testMilestoneAcceptance();
    testGroupAndPrimitiveCommands();
    testCameraTransformAndSpriteCommands();
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << "ScriptError: " << error.what() << '\n';
    return 1;
  }

  std::cout << "glyph render compiler tests passed\n";
  return 0;
}
