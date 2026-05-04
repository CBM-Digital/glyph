#include "input/InputSystem.h"

namespace glyph::input {

void InputSystem::beginFrame() {
  for (auto& [_, state] : buttons_) {
    state.pressed = false;
    state.released = false;
  }
  pointer_.pressed = false;
  pointer_.released = false;
  swipe_ = SwipeDirection::None;
}

void InputSystem::endFrame() {}

void InputSystem::setActionDown(StringId action, bool down) {
  auto& state = buttons_[action];
  if (state.down == down) {
    return;
  }
  state.down = down;
  state.pressed = down;
  state.released = !down;
}

void InputSystem::setAxis(StringId action, float value) { axes_[action] = value; }

void InputSystem::setPointerDown(bool down, Vec2 position) {
  pointer_.position = position;
  if (pointer_.down == down) {
    return;
  }
  pointer_.down = down;
  pointer_.pressed = down;
  pointer_.released = !down;
  if (down) {
    pointer_.startPosition = position;
  }
}

void InputSystem::setPointerPosition(Vec2 position) { pointer_.position = position; }

void InputSystem::setSwipe(SwipeDirection direction) { swipe_ = direction; }

bool InputSystem::pressed(StringId action) const { return button(action).pressed; }

bool InputSystem::held(StringId action) const { return button(action).down; }

bool InputSystem::released(StringId action) const { return button(action).released; }

float InputSystem::axis(StringId action) const {
  auto found = axes_.find(action);
  return found == axes_.end() ? 0.0f : found->second;
}

Vec2 InputSystem::pointerPosition() const { return pointer_.position; }

Vec2 InputSystem::pointerStartPosition() const { return pointer_.startPosition; }

bool InputSystem::pointerPressed() const { return pointer_.pressed; }

bool InputSystem::pointerHeld() const { return pointer_.down; }

bool InputSystem::pointerReleased() const { return pointer_.released; }

bool InputSystem::swipe(SwipeDirection direction) const { return swipe_ == direction; }

ButtonState InputSystem::button(StringId action) const {
  auto found = buttons_.find(action);
  return found == buttons_.end() ? ButtonState{} : found->second;
}

} // namespace glyph::input
