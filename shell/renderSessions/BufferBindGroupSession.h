/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/renderSession/RenderSession.h>
#include <igl/IGL.h>
#include <igl/FPSCounter.h>
#include <glm/glm.hpp>
#include <IGLU/imgui/Session.h>

namespace igl::shell {

// Test session for buffer bind groups - uses BindGroupBufferDesc to bind 4 uniform buffers
class BufferBindGroupSession : public RenderSession {
 public:
  explicit BufferBindGroupSession(std::shared_ptr<Platform> platform);
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  IDevice* device_{};
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IBuffer> vertexBuffer_;           // Interleaved vertex buffer
  std::shared_ptr<IBuffer> uniformBuffer0_;         // Uniform buffer 0 (transform)
  std::shared_ptr<IBuffer> uniformBuffer1_;         // Uniform buffer 1 (color tint)
  std::shared_ptr<IBuffer> uniformBuffer2_;         // Uniform buffer 2 (position offset)
  std::shared_ptr<IBuffer> uniformBuffer3_;         // Uniform buffer 3 (scale)
  Holder<BindGroupBufferHandle> bufferBindGroup_;   // Bind group for all 4 uniform buffers
  std::shared_ptr<IShaderModule> vertexShader_;
  std::shared_ptr<IShaderModule> fragmentShader_;
  std::unique_ptr<iglu::imgui::Session> imguiSession_;
  igl::FPSCounter fps_;
  float rotation_ = 0.0f;
};

} // namespace igl::shell
