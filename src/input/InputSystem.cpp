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

void InputSystem::beginTick() {
  beginFrame();
  for (auto& [action, events] : pendingButtons_) {
    if (events.empty()) continue;
    const bool down = events.front();
    events.pop_front();
    buttons_[action] = ButtonState{down, down, !down};
  }
  if (!pendingPointer_.empty()) {
    const auto event = pendingPointer_.front();
    pendingPointer_.pop_front();
    pointer_.down = event.down;
    pointer_.pressed = event.down;
    pointer_.released = !event.down;
    pointer_.position = pendingPointer_.empty() ? physicalPointerPosition_ : event.position;
    if (event.down) pointer_.startPosition = event.position;
  }
  if (!pendingSwipes_.empty() && pointer_.released) {
    swipe_ = pendingSwipes_.front();
    pendingSwipes_.pop_front();
  }
}

void InputSystem::clear() { *this = InputSystem{}; }

void InputSystem::setActionDown(StringId action, bool down) {
  auto& state = buttons_[action];
  if (physicalButtons_[action] == down) {
    return;
  }
  physicalButtons_[action] = down;
  pendingButtons_[action].push_back(down);
  state.down = down;
  state.pressed = down;
  state.released = !down;
}

void InputSystem::setAxis(StringId action, float value) { axes_[action] = value; }

void InputSystem::setPointerDown(bool down, Vec2 position) {
  physicalPointerPosition_ = position;
  pointer_.position = position;
  if (physicalPointerDown_ == down) {
    return;
  }
  physicalPointerDown_ = down;
  pendingPointer_.push_back({down, position});
  pointer_.down = down;
  pointer_.pressed = down;
  pointer_.released = !down;
  if (down) {
    pointer_.startPosition = position;
  }
}

void InputSystem::setPointerPosition(Vec2 position) { physicalPointerPosition_ = position; pointer_.position = position; }

void InputSystem::setSwipe(SwipeDirection direction) {
  swipe_ = direction;
  if (direction != SwipeDirection::None) pendingSwipes_.push_back(direction);
}

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
