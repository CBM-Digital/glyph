#include "runtime/RuntimeShell.h"
#include "script/Error.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void writeFile(const std::filesystem::path& path, const std::string& source) {
  std::ofstream output(path);
  require(static_cast<bool>(output), "open " + path.string());
  output << source;
}

std::filesystem::path runtimeTestRoot() {
  auto root = std::filesystem::temp_directory_path() / "glyph_runtime_shell_tests";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "assets");
  return root;
}

void writeRuntimeFixture(const std::filesystem::path& root) {
  writeFile(root / "index.glyph", R"(
    (game runtime-index
      :title "Runtime Index"
      :size [320 180]
      :assets {:hero "assets/hero.ppm"}
      :initial initial
      :update update
      :view view)

    (def initial {:taps 0 :px 0})

    (defn update [dt state]
      (if (pressed? :tap)
        (do
          (navigation/push "next.glyph")
          (assoc state :taps (+ (:taps state) 1) :px (pointer-x)))
        state))

    (defn view [state]
      (rect :x (:px state) :y 10 :w 16 :h 16 :color "#fff"))
  )");

  writeFile(root / "next.glyph", R"(
    (game runtime-next
      :title "Runtime Next"
      :size [320 180]
      :initial initial
      :update update
      :view view)

    (def initial {:ticks 0})

    (defn update [dt state]
      (update state :ticks + 1))

    (defn view [state]
      empty)
  )");

  writeFile(root / "assets/hero.ppm", "P3 1 1 255 255 255 255\n");
}

void testRuntimeShellLoadsFramesInputAndNavigation() {
  const auto root = runtimeTestRoot();
  writeRuntimeFixture(root);

  glyph::runtime::RuntimeShell shell(root / "index.glyph");
  require(shell.bundleRoot() == std::filesystem::weakly_canonical(root), "bundle root is scene directory");
  require(shell.currentSceneFile().filename() == "index.glyph", "initial scene loaded");
  require(shell.host().definition().title == "Runtime Index", "initial title");
  require(shell.assetPath("assets/hero.ppm") == root / "assets/hero.ppm", "relative asset path");

  shell.beginFrame();
  shell.setPointerDown(true, glyph::Vec2{42.0f, 5.0f});
  shell.setActionDown(":tap", true);
  shell.tick(glyph::game::GameHost::fixedDt);
  require(shell.processNavigation(), "tap-driven navigation activates next scene");
  shell.endFrame();

  require(shell.sceneDepth() == 2, "scene stack pushes");
  require(shell.currentSceneFile().filename() == "next.glyph", "current scene switches");
  require(shell.host().definition().title == "Runtime Next", "next scene title");
}

void testRuntimeShellPauseResume() {
  const auto root = runtimeTestRoot();
  writeRuntimeFixture(root);

  glyph::runtime::RuntimeShell shell(root / "next.glyph");
  const auto ticks = shell.host().vm().interner().intern(":ticks");

  shell.beginFrame();
  shell.tick(glyph::game::GameHost::fixedDt);
  shell.endFrame();
  require(shell.host().state().map->at(ticks).number == 1.0, "runtime tick advances state");

  shell.pause();
  shell.beginFrame();
  shell.tick(glyph::game::GameHost::fixedDt);
  shell.endFrame();
  require(shell.paused(), "runtime reports paused");
  require(shell.host().state().map->at(ticks).number == 1.0, "pause blocks runtime tick");

  shell.resume();
  shell.beginFrame();
  shell.tick(glyph::game::GameHost::fixedDt);
  shell.endFrame();
  require(shell.host().state().map->at(ticks).number == 2.0, "resume restores ticking");
}

} // namespace

int main() {
  try {
    testRuntimeShellLoadsFramesInputAndNavigation();
    testRuntimeShellPauseResume();
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << "ScriptError: " << error.what() << '\n';
    return 1;
  }

  std::cout << "glyph runtime shell tests passed\n";
  return 0;
}
