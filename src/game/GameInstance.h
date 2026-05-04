#pragma once

#include "game/GameDefinition.h"

namespace glyph::game {

struct GameInstance {
  GameDefinition definition;
  script::Value state;

  bool active = false;
  bool paused = false;

  double accumulator = 0.0;
};

} // namespace glyph::game
