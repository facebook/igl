/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <shell/shared/renderSession/RenderSession.h>

#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <igl/IGL.h>
#include <shell/shared/platform/Platform.h>

namespace igl::shell {

constexpr int kMaxJoints = 26;

struct UniformBlock {
  glm::mat4 jointMatrices[kMaxJoints]{};
  glm::mat4 viewProjectionMatrix[2]{};
  int viewId = 0;
};

class HandsOpenXRSession : public RenderSession {
 public:
  HandsOpenXRSession(std::shared_ptr<Platform> platform) : RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<ICommandQueue> commandQueue_;
  RenderPassDesc renderPass_;
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IDepthStencilState> depthStencilState_;
  std::shared_ptr<IVertexInputState> vertexInput0_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IBuffer> vb0_, ib0_;
  std::shared_ptr<IFramebuffer> framebuffer_[2];

  std::array<std::array<glm::mat4, kMaxJoints>, 2> jointInvBindMatrix_;

  struct DrawParams {
    size_t indexCount = 0;
    size_t indexBufferOffset = 0;
  } handsDrawParams_[2];

  UniformBlock ub_;
};

} // namespace igl::shell
