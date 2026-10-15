/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <IGLU/imgui/Session.h>
#include <array>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <igl/IGL.h>

namespace igl::shell {

// What Slang adds on high-end GPUs (Metal, Vulkan), one chapter at a time:
// 1. Physically based materials written once as Slang interfaces and generics, shaded three ways (a
//    pipeline per material, dynamic dispatch on a type id, half precision), with the GPU time of
//    each.
// 2. Inverse rendering: the same materials, differentiated with bwd_diff, recover every sphere's
//    parameters from images rendered with hidden ones; fwd_diff shows how the fit responds to
//    roughness.
class SlangShowcaseSession : public RenderSession {
 public:
  enum class Chapter : uint8_t {
    Materials,
    InverseRendering,
  };
  enum class ShadingMode : uint8_t {
    Specialized,
    Dynamic,
    Half,
  };

  explicit SlangShowcaseSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

  void setChapter(Chapter chapter) noexcept {
    chapter_ = chapter;
  }
  // Fixes the shading mode of chapter 1; otherwise it changes every few seconds.
  void setShadingMode(ShadingMode mode) noexcept {
    fixedMode_ = mode;
    hasFixedMode_ = true;
  }
  // Chapter 2 shows the derivative of the fit by roughness instead of the fit.
  void setShowSensitivity(bool show) noexcept {
    showSensitivity_ = show;
  }

 private:
  bool createPipelines(TextureFormat format);
  void createInverseResources(IDevice& device);
  void restartFit();
  void encodeInverseStep(ICommandBuffer& commandBuffer);
  void drawOverlay(ShadingMode mode);
  [[nodiscard]] ShadingMode currentMode() const;

  std::shared_ptr<IBuffer> vertexBuffer_;
  std::shared_ptr<IBuffer> indexBuffer_;
  std::shared_ptr<IBuffer> spheres_;
  std::shared_ptr<IBuffer> sceneUniforms_;
  std::shared_ptr<IVertexInputState> vertexInput_;
  std::shared_ptr<IDepthStencilState> depthState_;
  std::shared_ptr<IRenderPipelineState> dynamicPipeline_;
  std::array<std::shared_ptr<IRenderPipelineState>, 4> specializedPipelines_;
  std::array<std::shared_ptr<IRenderPipelineState>, 4> halfPipelines_;
  std::shared_ptr<IRenderPipelineState> inversePipeline_;
  std::shared_ptr<IRenderPipelineState> sensitivityPipeline_;
  std::shared_ptr<IBuffer> fitted_;
  std::shared_ptr<IBuffer> adam_;
  std::shared_ptr<IBuffer> pixelGradients_;
  std::shared_ptr<IBuffer> inverseUniforms_;
  std::shared_ptr<IComputePipelineState> gradientsPipeline_;
  std::shared_ptr<IComputePipelineState> adamPipeline_;
  std::shared_ptr<ITimer> timer_;
  std::unique_ptr<iglu::imgui::Session> imgui_;
  size_t indexCount_ = 0;
  uint64_t frame_ = 0;
  uint32_t steps_ = 0;
  double gpuMilliseconds_ = 0.0;
  Chapter chapter_ = Chapter::Materials;
  ShadingMode fixedMode_ = ShadingMode::Specialized;
  bool hasFixedMode_ = false;
  bool showSensitivity_ = false;
  bool supported_ = false;
};

} // namespace igl::shell
