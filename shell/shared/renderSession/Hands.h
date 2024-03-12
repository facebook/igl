/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace igl::shell {
struct Pose {
  glm::quat orientation;
  glm::vec3 position;
};

struct HandMesh {
  uint32_t vertexCountOutput = 0;

  std::vector<glm::vec3> vertexPositions;
  std::vector<glm::vec2> vertexUVs;
  std::vector<glm::vec3> vertexNormals;
  std::vector<glm::vec4> vertexBlendIndices;
  std::vector<glm::vec4> vertexBlendWeights;

  uint32_t indexCountOutput = 0;
  std::vector<int16_t> indices;

  uint32_t jointCountOutput = 0;
  std::vector<Pose> jointBindPoses;
};
} // namespace igl::shell
