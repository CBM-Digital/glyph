#pragma once

#include "core/Types.h"
#include "game/GameHost.h"
#include "input/InputSystem.h"
#include "render/DrawCommand.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace glyph::runtime {

class RuntimeShell {
public:
  RuntimeShell() = default;
  explicit RuntimeShell(std::filesystem::path entryFile);

  void loadGameBundle(std::filesystem::path entryFile);

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
  std::filesystem::path assetPath(const std::string& assetPath) const;
  std::size_t sceneDepth() const;

  const std::string& statusError() const;
  void setStatusError(std::string error);
  void clearStatusError();

private:
  struct Scene {
    game::GameHost host;
    std::filesystem::path file;
    std::string statusError;
  };

  Scene& currentScene();
  const Scene& currentScene() const;
  std::unique_ptr<Scene> loadSceneFile(const std::filesystem::path& sceneFile) const;
  StringId internAction(std::string_view action);

  std::filesystem::path bundleRoot_;
  std::vector<std::unique_ptr<Scene>> scenes_;
  bool paused_ = false;
};

} // namespace glyph::runtime
