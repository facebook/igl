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

class BufferMappingSession : public RenderSession {
 public:
  explicit BufferMappingSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IBuffer> vertexBuffer_;
  std::shared_ptr<IBuffer> indexBuffer_;
  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IShaderStages> shaderStages_;
  RenderPassDesc renderPass_;
  size_t frameCount_ = 0;
};

} // namespace igl::shell
