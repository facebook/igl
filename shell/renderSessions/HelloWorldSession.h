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

class HelloWorldSession : public RenderSession {
 public:
  explicit HelloWorldSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  RenderPassDesc renderPass_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IVertexInputState> vertexInput0_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<ITexture> depthTexture_;
  std::shared_ptr<IRenderPipelineState> renderPipelineStateTriangle_;
  std::shared_ptr<IBuffer> vb0_;
  std::shared_ptr<IBuffer> ib0_;
  std::vector<UniformDesc> vertexUniformDescriptors_;
};

} // namespace igl::shell
