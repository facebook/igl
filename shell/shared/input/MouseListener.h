/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace igl::shell {

enum MouseButton {
  Left = 0,
  Right = 1,
  Middle = 2,
};

struct MouseButtonEvent {
  MouseButton button;
  bool isDown;
  float x;
  float y;

  MouseButtonEvent() = default;
  MouseButtonEvent(MouseButton button, bool isDown, float x, float y) :
    button(button), isDown(isDown), x(x), y(y) {}
};

struct MouseMotionEvent {
  float x;
  float y;
  float dx;
  float dy;

  MouseMotionEvent() = default;
  MouseMotionEvent(float x, float y, float dx, float dy) : x(x), y(y), dx(dx), dy(dy) {}
};

struct MouseWheelEvent {
  float dx;
  float dy;
  MouseWheelEvent() = default;
  MouseWheelEvent(float dx, float dy) : dx(dx), dy(dy) {}
};

class IMouseListener {
 public:
  virtual bool process(const MouseButtonEvent& event) = 0;
  virtual bool process(const MouseMotionEvent& event) = 0;
  virtual bool process(const MouseWheelEvent& event) = 0;

  virtual ~IMouseListener() = default;
};

} // namespace igl::shell
