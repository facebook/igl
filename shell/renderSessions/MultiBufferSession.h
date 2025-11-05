/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/renderSession/RenderSession.h>
#include <igl/IGL.h>

namespace igl::shell {

// Test session for multiple vertex attribute buffers - uses separate buffers for position, color, and UVs
class MultiBufferSession : public RenderSession {
 public:
  explicit MultiBufferSession(std::shared_ptr<Platform> platform);
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  IDevice* device_{};
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IBuffer> positionBuffer_;  // Buffer 0: positions (vec3)
  std::shared_ptr<IBuffer> colorBuffer_;     // Buffer 1: colors (vec4)
  std::shared_ptr<IBuffer> uvBuffer_;        // Buffer 2: UVs (vec2)
  std::shared_ptr<ITexture> texture_;
  std::shared_ptr<ISamplerState> sampler_;
  std::shared_ptr<IShaderModule> vertexShader_;
  std::shared_ptr<IShaderModule> fragmentShader_;
};

} // namespace igl::shell
