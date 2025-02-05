/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <shell/shared/renderSession/RenderSession.h>

#include <IGLU/imgui/Session.h>
#include <atomic>
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
  explicit GPUStressSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)), fps_(false) {}
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;
  void setNumLayers(size_t numLayers);

  void setNumThreads(int numThreads);
  void setThrashMemory(bool thrashMemory);
  void setMemorySize(size_t memorySize);
  void setMemoryReads(size_t memoryReads);
  void setMemoryWrites(size_t memoryWrites);
  void setGoSlowOnCpu(int goSlowOnCpu);
  void setCubeCount(int cubeCount);
  void setDrawCount(int drawCount);
  void setTestOverdraw(bool testOverdraw);
  void setEnableBlending(bool enableBlending);
  void setUseMSAA(bool useMSAA);
  void setLightCount(int lightCount);
  void setThreadCore(int thread, int core);
  void setDropFrameInterval(int numberOfFramesBetweenDrops);
  void setDropFrameCount(int numberOfFramesToDrop);
  void setRotateCubes(bool rotate);

  [[nodiscard]] int getNumThreads() const;
  [[nodiscard]] bool getThrashMemory() const;
  [[nodiscard]] size_t getMemorySize() const;
  [[nodiscard]] size_t getMemoryReads() const;
  [[nodiscard]] size_t getMemoryWrites() const;
  [[nodiscard]] bool getGoSlowOnCpu() const;
  [[nodiscard]] int getCubeCount() const;
  [[nodiscard]] int getDrawCount() const;
  [[nodiscard]] bool getTestOverdraw() const;
  [[nodiscard]] bool getEnableBlending() const;
  [[nodiscard]] bool getUseMSAA() const;
  [[nodiscard]] int getLightCount() const;
  [[nodiscard]] std::vector<int> getThreadsCores() const;
  [[nodiscard]] std::string getCurrentUsageString() const;
  [[nodiscard]] int getDropFrameInterval() const;
  [[nodiscard]] int getDropFrameCount() const;
  [[nodiscard]] bool getRotateCubes() const;

 private:
  std::shared_ptr<ICommandQueue> commandQueue_;
  RenderPassDesc renderPass_;
  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IVertexInputState> vertexInput0_;
  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IBuffer> vb0_,
      ib0_; // Buffers for vertices and indices (or constants)
  std::shared_ptr<ITexture> tex0_;
  std::shared_ptr<ITexture> tex1_;
  std::shared_ptr<ISamplerState> samp0_;
  std::shared_ptr<ISamplerState> samp1_;
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
  void initSystemSettings();

  igl::FPSCounter fps_;
  std::atomic<bool> forceReset_{false};
};

} // namespace igl::shell
