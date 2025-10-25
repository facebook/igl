/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/renderSession/RenderSession.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <shell/shared/platform/Platform.h>
#include <igl/IGL.h>

namespace igl::shell {

struct VertexPosColor {
  glm::vec3 position;
  glm::vec3 color;
};

struct CubeTransform {
  glm::vec3 position;
  glm::vec3 rotationAxis;
  float rotationSpeed;
  float currentAngle;
  glm::vec3 color;
};

struct VertexUniforms {
  glm::mat4 mvpMatrix;
};

class ThreeCubesRenderSession : public RenderSession {
 public:
  explicit ThreeCubesRenderSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  RenderPassDesc renderPass_;
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IVertexInputState> vertexInput0_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IBuffer> vb0_, ib0_; // Buffers for vertices and indices

  // Three cube transforms
  CubeTransform cubes_[3];
  VertexUniforms vertexUniforms_;

  // TEMP: Cache uniform buffers to prevent GPU memory reuse bug in D3D12
  std::vector<std::shared_ptr<void>> cachedUniformBuffers_;

  // Utility functions
  void initializeCubeTransforms();
  void updateCubeTransform(size_t index, float deltaTime);
  void setVertexParams(size_t cubeIndex, float aspectRatio);
};

} // namespace igl::shell
