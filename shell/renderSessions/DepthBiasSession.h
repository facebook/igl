/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <IGLU/simdtypes/SimdTypes.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <igl/IGL.h>

namespace igl::shell {

class DepthBiasSession : public RenderSession {
 public:
  explicit DepthBiasSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IRenderPipelineState> shadowPipelineState_;
  std::shared_ptr<IDepthStencilState> depthStencilState_;
  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IShaderStages> shadowShaderStages_;
  std::shared_ptr<IBuffer> vertexBuffer_;
  std::shared_ptr<IBuffer> indexBuffer_;
  std::shared_ptr<ITexture> shadowMap_;
  std::shared_ptr<ISamplerState> shadowSampler_;
  std::shared_ptr<IFramebuffer> shadowFramebuffer_;
  RenderPassDesc renderPass_;
  RenderPassDesc shadowRenderPass_;
};

} // namespace igl::shell
