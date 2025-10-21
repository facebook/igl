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

class HelloTriangleSession : public RenderSession {
 public:
  explicit HelloTriangleSession(std::shared_ptr<Platform> platform);
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  IDevice* device_{};
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IBuffer> vertexBuffer_;
  std::shared_ptr<IShaderModule> vertexShader_;
  std::shared_ptr<IShaderModule> fragmentShader_;
};

} // namespace igl::shell
