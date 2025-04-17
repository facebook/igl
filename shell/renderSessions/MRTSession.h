/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <igl/RenderPass.h>

namespace igl::shell {

class MRTSession : public RenderSession {
 public:
  explicit MRTSession(std::shared_ptr<Platform> platform) : RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<ITexture> createTexture2D(const std::shared_ptr<ITexture>& tex);
  void createOrUpdateFramebufferDisplayLast(const igl::SurfaceTextures& surfaceTextures);
  void createOrUpdateFramebufferMRT(const igl::SurfaceTextures& surfaceTextures);

 private:
  std::shared_ptr<ICommandQueue> commandQueue_;

  // for the MRT pass
  RenderPassDesc renderPassMRT_;
  std::shared_ptr<IFramebuffer> framebufferMRT_;
  std::shared_ptr<IRenderPipelineState> pipelineStateMRT_;
  std::shared_ptr<IShaderStages> shaderStagesMRT_;

  // for last display pass
  RenderPassDesc renderPassDisplayLast_;
  std::shared_ptr<IFramebuffer> framebufferDisplayLast_;
  std::shared_ptr<IRenderPipelineState> pipelineStateLastDisplay_;
  std::shared_ptr<IShaderStages> shaderStagesDisplayLast_;

  std::shared_ptr<IVertexInputState> vertexInput_;

  std::shared_ptr<IBuffer> vb0_, vb1_, ib0_;

  std::shared_ptr<ITexture> tex0_; // Textures
  std::shared_ptr<ITexture> tex1_; // for MRT attachment 0
  std::shared_ptr<ITexture> tex2_; // for MRT attachment 1
  std::shared_ptr<ISamplerState> samp0_; // Samplers for texture (mipmap, clamp, linear etc.)
};

} // namespace igl::shell
