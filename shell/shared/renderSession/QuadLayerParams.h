/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace igl::shell {
struct QuadLayerParams {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec2> sizes;
  [[nodiscard]] size_t numQuads() const noexcept {
    return positions.size();
  }
};
} // namespace igl::shell
