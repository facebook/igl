/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glm/glm.hpp>
#include <shell/shared/renderSession/Fov.h>

namespace igl::shell {
struct ViewParams {
  glm::mat4 viewMatrix = glm::mat4(1);
  glm::vec3 cameraPosition = glm::vec3(0);
  Fov fov;
  uint8_t viewIndex = 0;
};
} // namespace igl::shell
