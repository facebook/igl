/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/renderSession/RenderSession.h>

#include <IGLU/imgui/Session.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <igl/FPSCounter.h>
#include <igl/IGL.h>
#include <shell/shared/platform/Platform.h>

namespace igl::shell {

struct VertexFormat {
  glm::mat4 projectionMatrix;
  glm::mat4 modelViewMatrix;
  float scaleZ{};
};

class GPUStressSession : public RenderSession {
 public:
  GPUStressSession(std::shared_ptr<Platform> platform) : RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<ICommandQueue> commandQueue_;
  RenderPassDesc renderPass_;
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IVertexInputState> vertexInput0_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IBuffer> vb0_, ib0_; // Buffers for vertices and indices (or constants)
  std::shared_ptr<ITexture> tex0_;
  std::shared_ptr<ISamplerState> samp0_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::unique_ptr<iglu::imgui::Session> imguiSession_;
  std::shared_ptr<IDepthStencilState> depthStencilState_;

  VertexFormat vertexParameters_;

  // utility fns
  void createSamplerAndTextures(const IDevice& /*device*/);
  void setModelViewMatrix(float angle, float scaleZ, float offsetX, float offsetY, float offsetZ);
  void setProjectionMatrix(float aspectRatio);

  void drawCubes(const igl::SurfaceTextures& surfaceTextures,
                 std::shared_ptr<igl::IRenderCommandEncoder> commands);
  void initState(const igl::SurfaceTextures& surfaceTextures);
  void createCubes();

  igl::FPSCounter fps_;
  unsigned long long lastTime_{0};
};

} // namespace igl::shell
