#pragma once

#include "assets/AssetManager.h"
#include "audio/AudioSystem.h"
#include "game/GameInstance.h"
#include "input/InputSystem.h"
#include "render/DrawCommand.h"
#include "render/RenderCompiler.h"
#include "script/VM.h"

#include <string>
#include <string_view>
#include <vector>

namespace glyph::game {

class GameHost {
public:
  static constexpr double fixedDt = 1.0 / 60.0;

  GameHost();

  void loadSource(std::string_view source, std::string file = "<game>");
  void reset();
  void setPaused(bool paused);

  void tick(double deltaSeconds);
  std::vector<render::DrawCommand> renderView();

  const GameDefinition& definition() const;
  const script::Value& state() const;
  void setState(script::Value state);

  script::VM& vm();
  input::InputSystem& input();
  assets::AssetManager& assets();
  audio::AudioSystem& audio();

private:
  script::Value metadataField(const script::Value& metadata, std::string_view key);
  script::Value resolveMetadataValue(const script::Value& value, std::string_view fieldName);
  GameDefinition extractDefinition(const script::Value& metadata);
  [[noreturn]] void fail(std::string_view message) const;

  script::VM vm_;
  input::InputSystem input_;
  assets::AssetManager assets_;
  audio::AudioSystem audio_;
  GameInstance instance_;
};

} // namespace glyph::game
