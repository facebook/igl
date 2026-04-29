/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <shell/renderSessions/ShellRenderSession.h>
#include <shell/shared/platform/Platform.h>
#include <igl/IGL.h>

namespace igl::shell {

class ComputeSession : public ShellRenderSession {
 public:
  explicit ComputeSession(std::shared_ptr<Platform> platform) :
    ShellRenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  void createOrUpdateDefaultFramebuffer(const igl::SurfaceTextures& surfaceTextures);
  std::string shaderForStage(ShaderStage stage, int computePipelineIndex = 0);

 private:
  RenderPassDesc renderPass_;
  std::shared_ptr<IFramebuffer> framebuffer_;

  std::shared_ptr<IRenderPipelineState> pipelineState_; // Graphics configuration block

  std::shared_ptr<IComputePipelineState> computePipelineState0_,
      computePipelineState1_; // Compute configuration block

  std::shared_ptr<IVertexInputState> vertexInput_; // Vertex fetch unit (buffer -> shader
  // semantics)
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IShaderStages> computeStages0_, computeStages1_; // Compute shader function

  std::shared_ptr<IBuffer> vbIn_, vbOut_, ib_; // Buffers for vertices and indices (or constants)
  std::shared_ptr<ITexture> tex_; // Textures
  std::shared_ptr<ITexture> outTex_; // Textures
  std::shared_ptr<ISamplerState> samp_; // Samplers for texture (mipmap, clamp, linear etc.)
  bool isSupported_ = true;
};

} // namespace igl::shell
