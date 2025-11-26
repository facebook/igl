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

class PassthroughSession : public RenderSession {
 public:
  PassthroughSession(std::shared_ptr<Platform> platform);
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
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
};

} // namespace igl::shell
