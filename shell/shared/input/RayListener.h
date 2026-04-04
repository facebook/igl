/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glm/vec3.hpp>

namespace igl::shell {

enum class Button : int {
  Unknown = -1,
  LeftTrigger = 0,
  RightTrigger = 1,
  A = 2,
  B = 3,
  X = 4,
  Y = 5,
  Count = 6,
};

struct RayEvent {
  glm::vec3 start;
  glm::vec3 end;
  bool buttonPressed = false;
  Button button = Button::Unknown;
};

class IRayListener {
 public:
  virtual bool process(const RayEvent& event) = 0;
  virtual ~IRayListener() = default;
};

} // namespace igl::shell
