/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/renderSession/RenderSession.h>
#include <igl/IGL.h>

// Forward declarations for D3D12 headless context
namespace igl::d3d12 {
class HeadlessD3D12Context;
class Device;
}

namespace igl::shell {

class PassthroughSession : public RenderSession {
 public:
  PassthroughSession(std::shared_ptr<Platform> platform);
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  void renderWindowed(SurfaceTextures surfaceTextures);
  void renderHeadless();

  // Windowed rendering (original)
  IDevice* device_ = nullptr;
  std::shared_ptr<IBuffer> vertexBuffer_;
  std::shared_ptr<IBuffer> uvBuffer_;
  std::shared_ptr<IBuffer> indexBuffer_;
  std::shared_ptr<ITexture> inputTexture_;
  std::shared_ptr<ITexture> offscreenTexture_;  // Render target for validation
  std::shared_ptr<ISamplerState> sampler_;
  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  bool validationDone_ = false;

  // Headless rendering (for comparison)
  std::unique_ptr<igl::d3d12::HeadlessD3D12Context> headlessContext_;
  std::unique_ptr<igl::d3d12::Device> headlessDevice_;
  std::shared_ptr<ICommandQueue> headlessCommandQueue_;
  std::shared_ptr<IBuffer> headlessVertexBuffer_;
  std::shared_ptr<IBuffer> headlessUvBuffer_;
  std::shared_ptr<IBuffer> headlessIndexBuffer_;
  std::shared_ptr<ITexture> headlessInputTexture_;
  std::shared_ptr<ITexture> headlessOffscreenTexture_;
  std::shared_ptr<ISamplerState> headlessSampler_;
  std::shared_ptr<IVertexInputState> headlessVertexInputState_;
  std::shared_ptr<IRenderPipelineState> headlessPipelineState_;
  bool headlessValidationDone_ = false;
};

} // namespace igl::shell
