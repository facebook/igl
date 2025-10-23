/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/renderSession/RenderSession.h>
#include <igl/RenderPass.h>

namespace igl::shell {

class PassthroughSession : public RenderSession {
 public:
  PassthroughSession(std::shared_ptr<Platform> platform) : RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<ICommandQueue> commandQueue_;
  RenderPassDesc renderPass_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IVertexInputState> vertexInput0_;
  std::shared_ptr<IBuffer> vb0_;
  std::shared_ptr<IBuffer> uv0_;
  std::shared_ptr<IBuffer> ib0_;
  std::shared_ptr<ITexture> inputTexture_;
  std::shared_ptr<ISamplerState> sampler_;
};

} // namespace igl::shell
