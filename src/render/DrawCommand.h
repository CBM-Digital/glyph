#pragma once

#include "assets/AssetManager.h"
#include "core/Types.h"

#include <string>

namespace glyph::render {

enum class DrawCommandType {
  Clear,
  Rect,
  Circle,
  Line,
  Sprite,
  Text,
  PushTransform,
  PopTransform,
  PushCamera,
  PopCamera
};

struct DrawCommand {
  DrawCommandType type = DrawCommandType::Clear;

  double x = 0.0;
  double y = 0.0;
  double w = 0.0;
  double h = 0.0;
  double r = 0.0;
  double x1 = 0.0;
  double y1 = 0.0;
  double x2 = 0.0;
  double y2 = 0.0;
  double width = 1.0;
  double zoom = 1.0;
  double scale = 1.0;
  double rotation = 0.0;
  double originX = 0.0;
  double originY = 0.0;
  double pivotX = 0.5;
  double pivotY = 0.5;

  StringId image = 0;
  StringId frame = 0;
  StringId font = 0;
  assets::AssetHandle asset {};
  bool hasSourceRect = false;
  bool hasOrigin = false;
  bool hasPivot = false;
  bool flipX = false;
  bool flipY = false;
  assets::SourceRect sourceRect {};

  std::string color;
  std::string text;
};

} // namespace glyph::render
