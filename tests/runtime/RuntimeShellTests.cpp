#include "runtime/RuntimeShell.h"
#include "script/Error.h"

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

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

class MemoryAssetSource final : public glyph::assets::IAssetSource {
public:
  explicit MemoryAssetSource(std::unordered_map<std::string, std::string> files) : files_(std::move(files)) {}

  std::optional<std::vector<std::uint8_t>> readBytes(std::string_view path) const override {
    auto found = files_.find(std::string(path));
    if (found == files_.end()) {
      return std::nullopt;
    }
    return std::vector<std::uint8_t>(found->second.begin(), found->second.end());
  }

  bool exists(std::string_view path) const override {
    return files_.find(std::string(path)) != files_.end();
  }

private:
  std::unordered_map<std::string, std::string> files_;
};

void testRuntimeShellLoadsFramesInputAndNavigation() {
  const auto root = runtimeTestRoot();
  writeRuntimeFixture(root);

  glyph::runtime::RuntimeShell shell(root / "index.glyph");
  require(shell.bundleRoot() == std::filesystem::weakly_canonical(root), "bundle root is scene directory");
  require(shell.currentSceneFile().filename() == "index.glyph", "initial scene loaded");
  require(shell.currentScenePath() == "index.glyph", "initial scene path is bundle relative");
  require(shell.host().definition().title == "Runtime Index", "initial title");
  require(shell.assetPath("assets/hero.ppm") == std::filesystem::weakly_canonical(root / "assets/hero.ppm"),
          "relative asset path");
  require(shell.assetLocation("assets/hero.ppm") == "assets/hero.ppm", "relative asset location");
  require(shell.assetExists("assets/hero.ppm"), "relative asset exists");
  require(shell.readAssetText("assets/hero.ppm").has_value(), "relative asset reads text");
  require(shell.assetSource().lastWriteTime(shell.currentScenePath()).has_value(),
          "filesystem source exposes last write time");

  shell.beginFrame();
  shell.setPointerDown(true, glyph::Vec2{42.0f, 5.0f});
  shell.setActionDown(":tap", true);
  shell.tick(glyph::game::GameHost::fixedDt);
  require(shell.processNavigation(), "tap-driven navigation activates next scene");
  shell.endFrame();

  require(shell.sceneDepth() == 2, "scene stack pushes");
  require(shell.currentSceneFile().filename() == "next.glyph", "current scene switches");
  require(shell.currentScenePath() == "next.glyph", "current scene path switches");
  require(shell.host().definition().title == "Runtime Next", "next scene title");
}

void testRuntimeShellLoadsFromAbstractAssetSource() {
  auto source = std::make_shared<MemoryAssetSource>(std::unordered_map<std::string, std::string>{
      {"index.glyph", R"(
        (game memory-index
          :title "Memory Index"
          :size [320 180]
          :assets {:hero "assets/hero.ppm"}
          :initial initial
          :update update
          :view view)

        (def initial {:taps 0})

        (defn update [dt state]
          (if (pressed? :tap)
            (do
              (navigation/push "next.glyph")
              (update state :taps + 1))
            state))

        (defn view [state]
          empty)
      )"},
      {"next.glyph", R"(
        (game memory-next
          :title "Memory Next"
          :size [320 180]
          :initial initial
          :update update
          :view view)

        (def initial {:ticks 0})

        (defn update [dt state]
          (update state :ticks + 1))

        (defn view [state]
          empty)
      )"},
      {"assets/hero.ppm", "P3 1 1 255 255 255 255\n"},
  });

  glyph::runtime::RuntimeShell shell(source, "index.glyph");
  require(shell.bundleRoot().empty(), "abstract source has no filesystem bundle root");
  require(shell.currentScenePath() == "index.glyph", "abstract source scene path");
  require(shell.currentSceneFile() == std::filesystem::path("index.glyph"), "abstract source scene file fallback");
  require(shell.host().definition().title == "Memory Index", "abstract source title");
  require(shell.assetLocation("assets/hero.ppm") == "assets/hero.ppm", "abstract source asset location");
  require(shell.assetExists("assets/hero.ppm"), "abstract source asset exists");
  require(shell.readAssetBytes("assets/hero.ppm").has_value(), "abstract source reads bytes");
  require(!shell.assetExists("../escape.ppm"), "abstract source rejects escaping asset path");

  shell.beginFrame();
  shell.setActionDown(":tap", true);
  shell.tick(glyph::game::GameHost::fixedDt);
  require(shell.processNavigation(), "abstract source navigation works");
  shell.endFrame();

  require(shell.currentScenePath() == "next.glyph", "abstract source switches scene path");
  require(shell.host().definition().title == "Memory Next", "abstract source next scene title");
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
  require(shell.host().audio().paused(), "pause marks audio paused");
  shell.beginFrame();
  shell.tick(glyph::game::GameHost::fixedDt);
  shell.endFrame();
  require(shell.paused(), "runtime reports paused");
  require(shell.host().state().map->at(ticks).number == 1.0, "pause blocks runtime tick");

  shell.resume();
  require(!shell.host().audio().paused(), "resume marks audio resumed");
  shell.beginFrame();
  shell.tick(glyph::game::GameHost::fixedDt);
  shell.endFrame();
  require(shell.host().state().map->at(ticks).number == 2.0, "resume restores ticking");
}

void testRuntimeShellKeepsScenePausedAcrossNavigation() {
  const auto root = runtimeTestRoot();
  writeRuntimeFixture(root);

  glyph::runtime::RuntimeShell shell(root / "index.glyph");

  shell.beginFrame();
  shell.setActionDown(":tap", true);
  shell.tick(glyph::game::GameHost::fixedDt);
  shell.pause();
  require(shell.processNavigation(), "queued navigation can activate while paused");
  shell.endFrame();

  require(shell.paused(), "runtime stays paused after navigation");
  require(shell.currentScenePath() == "next.glyph", "paused navigation switches scene");
  require(shell.host().audio().paused(), "new scene audio starts paused");

  const auto ticks = shell.host().vm().interner().intern(":ticks");
  shell.beginFrame();
  shell.tick(glyph::game::GameHost::fixedDt);
  shell.endFrame();
  require(shell.host().state().map->at(ticks).number == 0.0, "new scene does not tick while paused");

  shell.resume();
  shell.beginFrame();
  shell.tick(glyph::game::GameHost::fixedDt);
  shell.endFrame();
  require(shell.host().state().map->at(ticks).number == 1.0, "new scene resumes from preserved state");
}

} // namespace

int main() {
  try {
    testRuntimeShellLoadsFramesInputAndNavigation();
    testRuntimeShellLoadsFromAbstractAssetSource();
    testRuntimeShellPauseResume();
    testRuntimeShellKeepsScenePausedAcrossNavigation();
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << "ScriptError: " << error.what() << '\n';
    return 1;
  }

  std::cout << "glyph runtime shell tests passed\n";
  return 0;
}
