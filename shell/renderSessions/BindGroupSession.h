/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <shell/shared/renderSession/RenderSession.h>

#include <IGLU/imgui/Session.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <igl/FPSCounter.h>
#include <igl/IGL.h>
#include <shell/shared/platform/Platform.h>

namespace igl::shell {

struct VertexFormat {
  glm::mat4 mvpMatrix;
};

class BindGroupSession : public RenderSession {
 public:
  BindGroupSession(std::shared_ptr<Platform> platform);
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<ICommandQueue> commandQueue_;
  RenderPassDesc renderPass_;
  FramebufferDesc framebufferDesc_;
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IVertexInputState> vertexInput0_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IBuffer> vb0_, ib0_; // Buffers for vertices and indices (or constants)
  std::shared_ptr<IFramebuffer> framebuffer_;
  igl::Holder<BindGroupTextureHandle> bindGroupTextures_;
  std::unique_ptr<iglu::imgui::Session> imguiSession_;

  VertexFormat vertexParameters_;
  igl::FPSCounter fps_;
  float angle_ = 0;

  void createSamplerAndTextures(const IDevice& /*device*/);
};

} // namespace igl::shell
