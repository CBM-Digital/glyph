#pragma once

#include <cstdint>

namespace glyph {

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using f32 = float;
using f64 = double;

using StringId = u32;

struct Vec2 {
  f32 x = 0.0f;
  f32 y = 0.0f;
};

struct Rect {
  f32 x = 0.0f;
  f32 y = 0.0f;
  f32 w = 0.0f;
  f32 h = 0.0f;
};

struct Color {
  f32 r = 0.0f;
  f32 g = 0.0f;
  f32 b = 0.0f;
  f32 a = 1.0f;
};

} // namespace glyph
