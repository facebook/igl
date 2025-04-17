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
#include <shell/shared/platform/Platform.h>
#include <igl/FPSCounter.h>
#include <igl/IGL.h>

namespace igl::shell {

struct VertexFormat {
  glm::mat4 projectionMatrix;
  glm::mat4 modelViewMatrix;
  float scaleZ{};
};

class GPUStressSession : public RenderSession {
 public:
  explicit GPUStressSession(std::shared_ptr<Platform> platform);
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;
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
  struct VertexPosUvw {
    glm::vec3 position;
    glm::vec4 uvw;
    glm::vec4 base_color;
  };

  [[nodiscard]] std::string getLightingCalc() const;
  [[nodiscard]] std::string getVulkanFragmentShaderSource() const;
  std::unique_ptr<IShaderStages> getShaderStagesForBackend(IDevice& device) const noexcept;
  void addNormalsToCube();

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
                 std::shared_ptr<IRenderCommandEncoder> commands);
  void initState(const igl::SurfaceTextures& surfaceTextures);
  void createCubes();
  void initSystemSettings();

  void thrashCPU() noexcept;
  float doReadWrite(std::vector<std::vector<std::vector<float>>>& memBlock,
                    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                    int numBlocks,
                    int numRows,
                    int numCols,
                    int threadId);
  void allocateMemory();
  void thrashMemory() noexcept;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  void getOffset(int counter, float& x, float& y, float& z);
  glm::vec3 animateCube(int counter, float x, float y, float scale, int frameCount);

  FPSCounter fps_;

  std::vector<VertexPosUvw> vertexData0_;
  std::vector<uint16_t> indexData0_;
  std::vector<VertexPosUvw> vertexData_;
  std::vector<uint16_t> indexData_;

  std::atomic<bool> forceReset_{false};
  std::atomic<int> cubeCount_ = 1; // number of cubes in the vertex buffer
  // number of times to draw the vertex buffer (triangles = 12 * kDrawCount *
  // kCubeCount)
  std::atomic<int> drawCount_ = 50;
  // turn this on and set kDrawCount to 1.  Cube count will be the number of
  // layers you'll see
  std::atomic<bool> testOverdraw_ = false;

  std::atomic<bool> enableBlending_ = false; // turn this on to see the effects of alpha blending
  // make this number little to make all the cubes tiny on screen so fill isn't a
  // problem
  std::atomic<bool> useMSAA_ = true;

  // each light will add about 45 ish instructions to your pixel shader (tested
  // using powerVR compiler so grain of salt)arc lint --engine LintCPP
  std::atomic<int> lightCount_ = 5;
  // number of times to do a lof of math that does not calculate pi
  std::atomic<int> goSlowOnCpu_ = 10000; // max10000000;
  // cpu threads - these are really necessary to get our CPU usage up  to 100%
  // (otherwise the framerate just throttles)
  std::atomic<int> threadCount_ = 1;
  std::atomic<bool> thrashMemory_ = true;
  std::atomic<size_t> memorySize_ = 64; // in MB
  std::atomic<unsigned long> memoryReads_ = 10000; // max 1000000;
  std::atomic<unsigned long> memoryWrites_ = 10000; // 100000;
  std::vector<int> threadIds_ = {-1, -1, -1, -1, -1, -1, -1, -1};
  std::atomic<int> dropFrameX_ = 0;
  std::atomic<int> dropFrameCount_ = 2;
  std::atomic<bool> rotateCubes_ = true;

  double pi_ = 0.f;
  std::vector<std::vector<std::vector<float>>> memBlock_;
};

} // namespace igl::shell
