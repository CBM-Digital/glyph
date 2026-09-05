#pragma once

#include "assets/AssetSource.h"
#include "core/Types.h"
#include "profile/ProfileStore.h"
#include "game/GameHost.h"
#include "input/InputSystem.h"
#include "render/DrawCommand.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace glyph::runtime {

class RuntimeShell {
public:
  RuntimeShell() = default;
  explicit RuntimeShell(std::filesystem::path entryFile);
  RuntimeShell(std::shared_ptr<assets::IAssetSource> assetSource, std::string entryPath);

  void loadGameBundle(std::filesystem::path entryFile);
  void loadGameBundle(std::shared_ptr<assets::IAssetSource> assetSource, std::string entryPath);

  void setProfile(std::shared_ptr<profile::ProfileStore> profile);
  const profile::ProfileStore& profile() const { return *profile_; }
  void beginFrame();
  void tick(double deltaSeconds);
  std::vector<render::DrawCommand> renderView();
  void endFrame();

  void pause();
  void resume();
  void setPaused(bool paused);
  bool paused() const;

  void setActionDown(std::string_view action, bool down);
  void setAxis(std::string_view action, float value);
  void setPointerDown(bool down, Vec2 position);
  void setPointerPosition(Vec2 position);
  void setSwipe(input::SwipeDirection direction);

  bool processNavigation();

  game::GameHost& host();
  const game::GameHost& host() const;
  input::InputSystem& input();

  const std::filesystem::path& bundleRoot() const;
  const std::filesystem::path& currentSceneFile() const;
  const std::string& currentScenePath() const;
  std::string assetLocation(const std::string& assetPath) const;
  std::filesystem::path assetPath(const std::string& assetPath) const;
  const assets::IAssetSource& assetSource() const;
  std::optional<std::string> readAssetText(const std::string& assetPath) const;
  std::optional<std::vector<std::uint8_t>> readAssetBytes(const std::string& assetPath) const;
  bool assetExists(const std::string& assetPath) const;
  std::size_t sceneDepth() const;

  const std::string& statusError() const;
  void setStatusError(std::string error);
  void clearStatusError();

private:
  struct Scene {
    game::GameHost host;
    std::string path;
    std::filesystem::path file;
    std::string statusError;
  };

  Scene& currentScene();
  const Scene& currentScene() const;
  std::unique_ptr<Scene> loadSceneFile(const std::string& scenePath) const;
  StringId internAction(std::string_view action);

  std::shared_ptr<assets::IAssetSource> assetSource_;
  std::filesystem::path bundleRoot_;
  std::vector<std::unique_ptr<Scene>> scenes_;
  bool paused_ = false;
  std::shared_ptr<profile::ProfileStore> profile_ = std::make_shared<profile::ProfileStore>();
};

} // namespace glyph::runtime
