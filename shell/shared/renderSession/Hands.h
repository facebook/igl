/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <glm/ext/quaternion_float.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

namespace igl::shell {
struct Pose {
  glm::quat orientation;
  glm::vec3 position;
};

struct Velocity {
  glm::vec3 linear;
  glm::vec3 angular;
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

struct HandTracking {
  std::vector<Pose> jointPose;
  std::vector<Velocity> jointVelocity;
  std::vector<bool> isJointTracked;
};
} // namespace igl::shell
