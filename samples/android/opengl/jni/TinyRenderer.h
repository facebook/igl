/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <memory>
#include <igl/IGL.h>

namespace igl_samples::android {

class TinyRenderer final {
 public:
  void init();
  void render();
  void onSurfacesChanged();

 private:
  std::unique_ptr<igl::IDevice> device_;
  std::shared_ptr<igl::ICommandQueue> commandQueue_;
  std::shared_ptr<igl::IRenderPipelineState> pipelineState_;
  std::shared_ptr<igl::IVertexInputState> vertexInputState_;
  std::shared_ptr<igl::IShaderStages> shaderStages_;
  std::shared_ptr<igl::IBuffer> vertexBuffer_;
  std::shared_ptr<igl::IBuffer> indexBuffer_;
  std::shared_ptr<igl::IFramebuffer> framebuffer_;
  igl::RenderPassDesc renderPassDesc_;
};

} // namespace igl_samples::android
