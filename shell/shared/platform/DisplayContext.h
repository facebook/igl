/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glm/glm.hpp>

namespace igl::shell {

class DisplayContext {
 public:
  float scale = 1.0f; // TODO: Transition call sites to pixelsPerPoint and remove this
  float pixelsPerPoint = 1.0f; // e.g. retina scale on apple platforms
  glm::mat4 preRotationMatrix = glm::mat4(1.0f);
};

} // namespace igl::shell
