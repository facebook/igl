/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#pragma once

#include <shell/shared/renderSession/RenderSession.h>

#include <IGLU/imgui/Session.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <shell/shared/platform/Platform.h>
#include <igl/FPSCounter.h>
#include <igl/RenderPass.h>

namespace igl::shell {

class TextureViewSession : public RenderSession {
 public:
  explicit TextureViewSession(std::shared_ptr<Platform> platform);
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<ICommandQueue> commandQueue_;
  FramebufferDesc framebufferDesc_;
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IVertexInputState> vertexInput0_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IBuffer> vb_, ib_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<ISamplerState> sampler_;
  std::shared_ptr<ITexture> texture_;
  std::vector<std::shared_ptr<ITexture>> textureViews_;
  std::unique_ptr<iglu::imgui::Session> imguiSession_;

  FPSCounter fps_;
  float angle_ = 0;
};

} // namespace igl::shell
