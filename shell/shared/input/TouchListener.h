/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace igl::shell {

struct TouchEvent {
  bool isDown;
  float x;
  float y;
  float dx;
  float dy;

  TouchEvent() = default;
  TouchEvent(bool isDown, float x, float y, float dx, float dy) :
    isDown(isDown), x(x), y(y), dx(dx), dy(dy) {}
};

class ITouchListener {
 public:
  virtual bool process(const TouchEvent& event) = 0;

  virtual ~ITouchListener() = default;
};

} // namespace igl::shell
