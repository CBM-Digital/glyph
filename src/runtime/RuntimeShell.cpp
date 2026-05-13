#include "runtime/RuntimeShell.h"

#include "script/Error.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <system_error>
#include <utility>

namespace glyph::runtime {
namespace {

std::filesystem::path canonicalSceneRoot(const std::filesystem::path& gameFile) {
  std::error_code ec;
  auto root = std::filesystem::weakly_canonical(gameFile.parent_path(), ec);
  return ec ? std::filesystem::absolute(gameFile.parent_path()) : root;
}

bool hasDrivePrefix(std::string_view path) {
  return path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':';
}

std::optional<std::string> normalizeVirtualPath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  if (path.empty() || path[0] == '/' || hasDrivePrefix(path)) {
    return std::nullopt;
  }

  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= path.size()) {
    const std::size_t end = path.find('/', start);
    const std::string part = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (!part.empty() && part != ".") {
      if (part == "..") {
        if (parts.empty()) {
          return std::nullopt;
        }
        parts.pop_back();
      } else {
        parts.push_back(part);
      }
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }

  std::string normalized;
  for (const auto& part : parts) {
    if (!normalized.empty()) {
      normalized += '/';
    }
    normalized += part;
  }
  return normalized.empty() ? std::nullopt : std::optional<std::string>(normalized);
}

std::string parentPath(std::string_view path) {
  const std::size_t slash = path.find_last_of('/');
  return slash == std::string_view::npos ? std::string() : std::string(path.substr(0, slash));
}

std::optional<std::string> resolveVirtualPath(std::string_view currentFile, const std::string& target) {
  std::string candidate;
  const auto currentParent = parentPath(currentFile);
  if (!currentParent.empty()) {
    candidate = currentParent + '/';
  }
  candidate += target;
  return normalizeVirtualPath(std::move(candidate));
}

bool hasGlyphExtension(std::string_view path) {
  constexpr std::string_view extension = ".glyph";
  return path.size() >= extension.size() && path.substr(path.size() - extension.size()) == extension;
}

std::optional<std::string> resolveScenePath(const assets::IAssetSource& assetSource,
                                            const std::string& currentFile,
                                            const std::string& target,
                                            std::string& error) {
  if (target.empty()) {
    error = "navigation target is empty";
    return std::nullopt;
  }

  const auto candidate = resolveVirtualPath(currentFile, target);
  if (!candidate) {
    error = "navigation target escapes the game bundle: " + target;
    return std::nullopt;
  }
  if (!hasGlyphExtension(*candidate)) {
    error = "navigation target must be a .glyph file";
    return std::nullopt;
  }
  if (!assetSource.exists(*candidate)) {
    error = "navigation target does not exist: " + target;
    return std::nullopt;
  }
  return candidate;
}

} // namespace

RuntimeShell::RuntimeShell(std::filesystem::path entryFile) { loadGameBundle(std::move(entryFile)); }

RuntimeShell::RuntimeShell(std::shared_ptr<assets::IAssetSource> assetSource, std::string entryPath) {
  loadGameBundle(std::move(assetSource), std::move(entryPath));
}

void RuntimeShell::loadGameBundle(std::filesystem::path entryFile) {
  bundleRoot_ = canonicalSceneRoot(entryFile);
  loadGameBundle(std::make_shared<assets::FilesystemAssetSource>(bundleRoot_),
                 entryFile.filename().generic_string());
}

void RuntimeShell::loadGameBundle(std::shared_ptr<assets::IAssetSource> assetSource, std::string entryPath) {
  if (!assetSource) {
    throw script::RuntimeError("runtime shell requires an asset source");
  }
  auto normalizedEntry = normalizeVirtualPath(std::move(entryPath));
  if (!normalizedEntry) {
    throw script::RuntimeError("game entry path escapes the game bundle");
  }

  if (const auto* filesystemSource = dynamic_cast<const assets::FilesystemAssetSource*>(assetSource.get())) {
    bundleRoot_ = filesystemSource->root();
  } else {
    bundleRoot_.clear();
  }

  assetSource_ = std::move(assetSource);
  scenes_.clear();
  scenes_.push_back(loadSceneFile(*normalizedEntry));
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
      const auto target = resolveScenePath(*assetSource_, current.path, command.target, error);
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

const std::string& RuntimeShell::currentScenePath() const { return currentScene().path; }

std::string RuntimeShell::assetLocation(const std::string& assetPath) const {
  auto resolved = resolveVirtualPath(currentScenePath(), assetPath);
  if (!resolved) {
    return {};
  }
  return *resolved;
}

std::filesystem::path RuntimeShell::assetPath(const std::string& assetPath) const {
  const auto location = assetLocation(assetPath);
  if (location.empty()) {
    return {};
  }
  if (auto physical = assetSource_->physicalPath(location)) {
    return *physical;
  }
  return std::filesystem::path(location);
}

const assets::IAssetSource& RuntimeShell::assetSource() const { return *assetSource_; }

std::optional<std::string> RuntimeShell::readAssetText(const std::string& assetPath) const {
  const auto location = assetLocation(assetPath);
  if (location.empty()) {
    return std::nullopt;
  }
  return assetSource_->readText(location);
}

std::optional<std::vector<std::uint8_t>> RuntimeShell::readAssetBytes(const std::string& assetPath) const {
  const auto location = assetLocation(assetPath);
  if (location.empty()) {
    return std::nullopt;
  }
  return assetSource_->readBytes(location);
}

bool RuntimeShell::assetExists(const std::string& assetPath) const {
  const auto location = assetLocation(assetPath);
  return !location.empty() && assetSource_->exists(location);
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

std::unique_ptr<RuntimeShell::Scene> RuntimeShell::loadSceneFile(const std::string& scenePath) const {
  auto scene = std::make_unique<Scene>();
  scene->path = scenePath;
  if (auto physical = assetSource_->physicalPath(scenePath)) {
    scene->file = *physical;
  } else {
    scene->file = std::filesystem::path(scenePath);
  }

  auto source = assetSource_->readText(scenePath);
  if (!source) {
    throw script::RuntimeError("unable to open " + scenePath);
  }
  scene->host.loadSource(*source, scenePath);
  return scene;
}

StringId RuntimeShell::internAction(std::string_view action) {
  return host().vm().interner().intern(std::string(action));
}

} // namespace glyph::runtime
