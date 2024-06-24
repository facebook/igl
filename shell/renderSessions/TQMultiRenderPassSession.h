/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <IGLU/simdtypes/SimdTypes.h>
#include <shell/renderSessions/TQSession.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl::shell {

class TQMultiRenderPassSession : public RenderSession {
 public:
  TQMultiRenderPassSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<IDevice> device_;

  std::shared_ptr<ICommandQueue> commandQueue_;

  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<ISamplerState> samplerState_;

  FragmentFormat fragmentParameters_{};
  std::vector<igl::UniformDesc> fragmentUniformDescriptors_;

  std::shared_ptr<IBuffer> fragmentParamBuffer_;
  std::shared_ptr<IBuffer> vb0_;
  std::shared_ptr<IBuffer> vb1_;
  std::shared_ptr<IBuffer> ib0_;

  std::shared_ptr<ITexture> depthTexture_;
  std::shared_ptr<ITexture> tex0_;
  std::shared_ptr<ITexture> tex1_;

  std::shared_ptr<IRenderPipelineState> pipelineState0_;
  std::shared_ptr<IRenderPipelineState> pipelineState1_;
  RenderPassDesc renderPass0_;
  RenderPassDesc renderPass1_;
  std::shared_ptr<IFramebuffer> framebuffer0_;
  std::shared_ptr<IFramebuffer> framebuffer1_;
};

} // namespace igl::shell
