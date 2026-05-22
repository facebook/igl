/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <memory>
#include <shell/renderSessions/ShellRenderSession.h>
#include <igl/Buffer.h>
#include <igl/IGL.h>

namespace igl::shell {

class DrawIndirectSession : public ShellRenderSession {
 public:
  explicit DrawIndirectSession(std::shared_ptr<Platform> platform) :
    ShellRenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<IBuffer> vertexBuffer_; // vertices
  std::shared_ptr<IBuffer> indexBuffer_; // indices into vertices
  std::shared_ptr<IBuffer> indirectBuffer_; // draw commands
  std::shared_ptr<IVertexInputState> vertexState_;
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IRenderPipelineState> pipelineStateAlt_;
  std::shared_ptr<IShaderStages> shaderStagesAlt_;
  RenderPassDesc renderPass_;

  std::shared_ptr<IShaderStages> buildIndirectBufferStages_;
  std::shared_ptr<IComputePipelineState> computePipelineState_;
  std::shared_ptr<IBuffer> indirectBufferForCompute_; // draw commands to be filled by compute
};

} // namespace igl::shell
