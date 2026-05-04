#pragma once

#include "core/Types.h"
#include "script/Value.h"

#include <string>

namespace glyph::game {

struct GameDefinition {
  StringId id = 0;
  std::string title;
  Vec2 logicalSize {};

  script::Value initialState;
  script::Value updateFn;
  script::Value viewFn;
  script::Value assetManifest;
};

} // namespace glyph::game
