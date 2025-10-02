/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/input/KeyListener.h>
#include <shell/shared/input/MouseListener.h>
#include <glm/glm.hpp>
#include <unordered_map>

namespace engine {

class InputManager : public igl::shell::IKeyListener, public igl::shell::IMouseListener {
 public:
  InputManager() = default;

  void update();

  // Key input
  bool isKeyDown(int key) const;
  bool isKeyPressed(int key) const;  // True only on the frame the key was pressed
  bool isKeyReleased(int key) const; // True only on the frame the key was released

  // Mouse input
  glm::vec2 getMousePosition() const {
    return mousePosition_;
  }
  glm::vec2 getMouseDelta() const {
    return mouseDelta_;
  }
  bool isMouseButtonDown(int button) const;
  bool isMouseButtonPressed(int button) const;
  bool isMouseButtonReleased(int button) const;

  // IKeyListener
  bool process(const igl::shell::KeyEvent& event) override;

  // IMouseListener
  bool process(const igl::shell::MouseButtonEvent& event) override;
  bool process(const igl::shell::MouseMotionEvent& event) override;
  bool process(const igl::shell::MouseWheelEvent& event) override;

 private:
  struct KeyState {
    bool isDown = false;
    bool wasPressed = false;
    bool wasReleased = false;
  };

  struct MouseButtonState {
    bool isDown = false;
    bool wasPressed = false;
    bool wasReleased = false;
  };

  std::unordered_map<int, KeyState> keyStates_;
  std::unordered_map<int, MouseButtonState> mouseButtonStates_;
  glm::vec2 mousePosition_{0.0f};
  glm::vec2 mouseDelta_{0.0f};
  glm::vec2 lastMousePosition_{0.0f};
};

} // namespace engine
