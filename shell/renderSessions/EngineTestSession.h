/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/renderSession/RenderSession.h>
#include <shell/engine/resources/GLTFLoader.h>
#include <igl/IGL.h>
#include <glm/glm.hpp>

namespace igl::shell {

class EngineTestSession : public RenderSession {
 public:
  explicit EngineTestSession(std::shared_ptr<Platform> platform);

  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
  void renderNode(const std::shared_ptr<engine::GLTFNode>& node,
                  const glm::mat4& parentTransform,
                  IRenderCommandEncoder* commands);
  std::shared_ptr<engine::Mesh> createCubeMesh(IDevice& device, float size);
  std::shared_ptr<ICommandQueue> commandQueue_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<IBuffer> vb_;
  std::shared_ptr<IBuffer> ib_;
  std::shared_ptr<IVertexInputState> vertexInput_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::unique_ptr<engine::GLTFModel> model_;
  uint32_t indexCount_ = 0;
  uint32_t frameCount_ = 0;
  bool capturedFrame_ = false;
  std::shared_ptr<IBuffer> uniformBuffer_;
  std::shared_ptr<ISamplerState> sampler_;
  std::shared_ptr<IDepthStencilState> depthStencilState_;
  std::shared_ptr<ITexture> defaultTexture_;
  float rotationAngle_ = 0.0f;
  size_t currentUniformOffset_ = 0;
  static constexpr size_t kMaxDrawCalls = 64;
  static constexpr size_t kUniformAlignment = 256;  // Metal requires 256-byte alignment
};

} // namespace igl::shell
