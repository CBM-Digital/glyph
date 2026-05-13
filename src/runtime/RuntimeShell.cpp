#include "runtime/RuntimeShell.h"

#include "script/Error.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>
#include <utility>

namespace glyph::runtime {
namespace {

std::string readFile(const std::filesystem::path& file) {
  std::ifstream input(file);
  if (!input) {
    throw script::RuntimeError("unable to open " + file.string());
  }

  std::ostringstream source;
  source << input.rdbuf();
  return source.str();
}

std::filesystem::path canonicalSceneRoot(const std::filesystem::path& gameFile) {
  std::error_code ec;
  auto root = std::filesystem::weakly_canonical(gameFile.parent_path(), ec);
  return ec ? std::filesystem::absolute(gameFile.parent_path()) : root;
}

bool pathInsideRoot(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  const auto relative = candidate.lexically_relative(root);
  if (relative.empty()) {
    return true;
  }

  auto it = relative.begin();
  return it != relative.end() && *it != ".." && !relative.is_absolute();
}

std::optional<std::filesystem::path> resolveScenePath(const std::filesystem::path& root,
                                                      const std::filesystem::path& currentFile,
                                                      const std::string& target,
                                                      std::string& error) {
  if (target.empty()) {
    error = "navigation target is empty";
    return std::nullopt;
  }

  const std::filesystem::path raw(target);
  std::filesystem::path candidate = raw.is_absolute() ? raw : currentFile.parent_path() / raw;
  if (candidate.extension() != ".glyph") {
    error = "navigation target must be a .glyph file";
    return std::nullopt;
  }

  std::error_code ec;
  candidate = std::filesystem::weakly_canonical(candidate, ec);
  if (ec || !std::filesystem::exists(candidate)) {
    error = "navigation target does not exist: " + target;
    return std::nullopt;
  }
  if (!pathInsideRoot(root, candidate)) {
    error = "navigation target escapes the game bundle: " + target;
    return std::nullopt;
  }
  return candidate;
}

} // namespace

RuntimeShell::RuntimeShell(std::filesystem::path entryFile) { loadGameBundle(std::move(entryFile)); }

void RuntimeShell::loadGameBundle(std::filesystem::path entryFile) {
  bundleRoot_ = canonicalSceneRoot(entryFile);
  scenes_.clear();
  scenes_.push_back(loadSceneFile(entryFile));
  paused_ = false;
}

void RuntimeShell::beginFrame() { host().input().beginFrame(); }

void RuntimeShell::tick(double deltaSeconds) {
  if (!paused_) {
    host().tick(deltaSeconds);
  }
}

std::vector<render::DrawCommand> RuntimeShell::renderView() { return host().renderView(); }

void RuntimeShell::endFrame() { host().input().endFrame(); }

void RuntimeShell::pause() { setPaused(true); }

void RuntimeShell::resume() { setPaused(false); }

void RuntimeShell::setPaused(bool paused) {
  paused_ = paused;
  host().setPaused(paused);
  if (paused) {
    host().audio().pauseAll();
  } else {
    host().audio().resumeAll();
  }
}

bool RuntimeShell::paused() const { return paused_; }

void RuntimeShell::setActionDown(std::string_view action, bool down) {
  host().input().setActionDown(internAction(action), down);
}

void RuntimeShell::setAxis(std::string_view action, float value) {
  host().input().setAxis(internAction(action), value);
}

void RuntimeShell::setPointerDown(bool down, Vec2 position) { host().input().setPointerDown(down, position); }

void RuntimeShell::setPointerPosition(Vec2 position) { host().input().setPointerPosition(position); }

void RuntimeShell::setSwipe(input::SwipeDirection direction) { host().input().setSwipe(direction); }

bool RuntimeShell::processNavigation() {
  auto& current = currentScene();
  const auto commands = current.host.navigation().commands();
  current.host.navigation().clear();

  for (const auto& command : commands) {
    if (command.type == game::NavigationCommandType::Push) {
      std::string error;
      const auto target = resolveScenePath(bundleRoot_, current.file, command.target, error);
      if (!target) {
        current.statusError = error;
        return false;
      }

      scenes_.push_back(loadSceneFile(*target));
      if (paused_) {
        scenes_.back()->host.setPaused(true);
        scenes_.back()->host.audio().pauseAll();
      }
      return true;
    }

    if (command.type == game::NavigationCommandType::Pop) {
      if (scenes_.size() > 1) {
        scenes_.pop_back();
        if (paused_) {
          scenes_.back()->host.setPaused(true);
          scenes_.back()->host.audio().pauseAll();
        }
        return true;
      }
      return false;
    }
  }

  return false;
}

game::GameHost& RuntimeShell::host() { return currentScene().host; }

const game::GameHost& RuntimeShell::host() const { return currentScene().host; }

input::InputSystem& RuntimeShell::input() { return host().input(); }

const std::filesystem::path& RuntimeShell::bundleRoot() const { return bundleRoot_; }

const std::filesystem::path& RuntimeShell::currentSceneFile() const { return currentScene().file; }

std::filesystem::path RuntimeShell::assetPath(const std::string& assetPath) const {
  const std::filesystem::path raw(assetPath);
  if (raw.is_absolute()) {
    return raw;
  }
  return currentSceneFile().parent_path() / raw;
}

std::size_t RuntimeShell::sceneDepth() const { return scenes_.size(); }

const std::string& RuntimeShell::statusError() const { return currentScene().statusError; }

void RuntimeShell::setStatusError(std::string error) { currentScene().statusError = std::move(error); }

void RuntimeShell::clearStatusError() { currentScene().statusError.clear(); }

RuntimeShell::Scene& RuntimeShell::currentScene() {
  if (scenes_.empty()) {
    throw script::RuntimeError("runtime shell has no loaded game bundle");
  }
  return *scenes_.back();
}

const RuntimeShell::Scene& RuntimeShell::currentScene() const {
  if (scenes_.empty()) {
    throw script::RuntimeError("runtime shell has no loaded game bundle");
  }
  return *scenes_.back();
}

std::unique_ptr<RuntimeShell::Scene> RuntimeShell::loadSceneFile(const std::filesystem::path& sceneFile) const {
  auto scene = std::make_unique<Scene>();
  scene->file = sceneFile;
  scene->host.loadSource(readFile(sceneFile), sceneFile.string());
  return scene;
}

StringId RuntimeShell::internAction(std::string_view action) {
  return host().vm().interner().intern(std::string(action));
}

} // namespace glyph::runtime
