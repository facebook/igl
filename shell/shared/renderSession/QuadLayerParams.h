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

enum class LayerBlendMode : uint8_t {
  Opaque = 0,
  AlphaBlend = 1,
  AlphaAdditive = 2,
};

struct QuadLayerParams {
  std::vector<glm::vec3> positions;
  std::vector<glm::vec2> sizes;
  std::vector<LayerBlendMode> blendModes;
  uint32_t imageWidth = 1024;
  uint32_t imageHeight = 1024;

  [[nodiscard]] size_t numQuads() const noexcept {
    return positions.size();
  }
};
} // namespace igl::shell
