/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "InputManager.h"

namespace engine {

void InputManager::update() {
  // Clear frame-specific states
  for (auto& [key, state] : keyStates_) {
    state.wasPressed = false;
    state.wasReleased = false;
  }

  for (auto& [button, state] : mouseButtonStates_) {
    state.wasPressed = false;
    state.wasReleased = false;
  }

  // Update mouse delta
  mouseDelta_ = mousePosition_ - lastMousePosition_;
  lastMousePosition_ = mousePosition_;
}

bool InputManager::isKeyDown(int key) const {
  auto it = keyStates_.find(key);
  return it != keyStates_.end() && it->second.isDown;
}

bool InputManager::isKeyPressed(int key) const {
  auto it = keyStates_.find(key);
  return it != keyStates_.end() && it->second.wasPressed;
}

bool InputManager::isKeyReleased(int key) const {
  auto it = keyStates_.find(key);
  return it != keyStates_.end() && it->second.wasReleased;
}

bool InputManager::isMouseButtonDown(int button) const {
  auto it = mouseButtonStates_.find(button);
  return it != mouseButtonStates_.end() && it->second.isDown;
}

bool InputManager::isMouseButtonPressed(int button) const {
  auto it = mouseButtonStates_.find(button);
  return it != mouseButtonStates_.end() && it->second.wasPressed;
}

bool InputManager::isMouseButtonReleased(int button) const {
  auto it = mouseButtonStates_.find(button);
  return it != mouseButtonStates_.end() && it->second.wasReleased;
}

bool InputManager::process(const igl::shell::KeyEvent& event) {
  auto& state = keyStates_[event.key];

  if (event.isDown && !state.isDown) {
    state.wasPressed = true;
  } else if (!event.isDown && state.isDown) {
    state.wasReleased = true;
  }

  state.isDown = event.isDown;
  return true;
}

bool InputManager::process(const igl::shell::MouseButtonEvent& event) {
  auto& state = mouseButtonStates_[event.button];

  if (event.isDown && !state.isDown) {
    state.wasPressed = true;
  } else if (!event.isDown && state.isDown) {
    state.wasReleased = true;
  }

  state.isDown = event.isDown;
  return true;
}

bool InputManager::process(const igl::shell::MouseMotionEvent& event) {
  mousePosition_ = glm::vec2(event.x, event.y);
  return true;
}

bool InputManager::process(const igl::shell::MouseWheelEvent& /*event*/) {
  return true;
}

} // namespace engine
