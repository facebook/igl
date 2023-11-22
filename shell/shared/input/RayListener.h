/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glm/glm.hpp>

namespace igl::shell {

struct RayEvent {
  glm::vec3 start;
  glm::vec3 end;
  bool buttonPressed = false;
};

class IRayListener {
 public:
  virtual bool process(const RayEvent& event) = 0;
  virtual ~IRayListener() = default;
};

} // namespace igl::shell
