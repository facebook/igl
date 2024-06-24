/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <iglu/kit/Renderable.hpp>

namespace igl {
class IRenderPipelineState;
class IVertexInputState;
class IShaderStages;
class IBuffer;
class IBuffer;
class ITexture;
class ISamplerState;
} // namespace igl

namespace iglu::kit {

class TinyRenderable : public IRenderable {
 public:
  ~TinyRenderable() override = default;
  [[nodiscard]] const nlohmann::json& getProperties() const override;
  void initialize(igl::IDevice& device, const igl::IFramebuffer& framebuffer) override;
  void update(igl::IDevice& device) override;
  void submit(igl::IRenderCommandEncoder& cmds) override;

 private:
  // PipelineState
  std::shared_ptr<igl::IRenderPipelineState> pipelineState_;
  std::shared_ptr<igl::IVertexInputState> vertexInput_;
  std::shared_ptr<igl::IShaderStages> shaderStages_;

  // Draw data
  std::shared_ptr<igl::IBuffer> vertexBuffer_;
  std::shared_ptr<igl::IBuffer> indexBuffer_;
  std::shared_ptr<igl::ITexture> texture_;
  std::shared_ptr<igl::ISamplerState> sampler_;
  // std::shared_ptr<igl::IBuffer> uniformBuffer_;
};

} // namespace iglu::kit
