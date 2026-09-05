#pragma once

#include "core/Types.h"

#include <unordered_map>
#include <deque>

namespace glyph::input {

enum class SwipeDirection {
  None,
  Left,
  Right,
  Up,
  Down
};

struct ButtonState {
  bool down = false;
  bool pressed = false;
  bool released = false;
};

struct PointerState {
  Vec2 position {};
  Vec2 startPosition {};
  bool down = false;
  bool pressed = false;
  bool released = false;
};

class InputSystem {
public:
  void beginFrame();
  void endFrame();
  // Render frames collect events. Simulation ticks consume each edge once.
  void beginTick();
  void clear();

  void setActionDown(StringId action, bool down);
  void setAxis(StringId action, float value);
  void setPointerDown(bool down, Vec2 position);
  void setPointerPosition(Vec2 position);
  void setSwipe(SwipeDirection direction);

  bool pressed(StringId action) const;
  bool held(StringId action) const;
  bool released(StringId action) const;
  float axis(StringId action) const;

  Vec2 pointerPosition() const;
  Vec2 pointerStartPosition() const;
  bool pointerPressed() const;
  bool pointerHeld() const;
  bool pointerReleased() const;

  bool swipe(SwipeDirection direction) const;

private:
  ButtonState button(StringId action) const;

  std::unordered_map<StringId, ButtonState> buttons_;
  std::unordered_map<StringId, bool> physicalButtons_;
  std::unordered_map<StringId, std::deque<bool>> pendingButtons_;
  struct PointerTransition { bool down; Vec2 position; };
  std::deque<PointerTransition> pendingPointer_;
  bool physicalPointerDown_ = false;
  Vec2 physicalPointerPosition_{};
  std::deque<SwipeDirection> pendingSwipes_;
  std::unordered_map<StringId, float> axes_;
  PointerState pointer_;
  SwipeDirection swipe_ = SwipeDirection::None;
};

} // namespace glyph::input
