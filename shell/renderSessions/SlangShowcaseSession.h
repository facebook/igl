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
#include <string>
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
// 3. Post effects composed at compile time: a variadic generic chains effects, and each chain of
//    effects compiles to its own pipeline.
// 4. GPU-driven geometry: a procedural terrain of meshlets, culled by a task shader and expanded by
//    a mesh shader with no vertex or index buffers; without mesh shaders, an instanced vertex
//    shader draws every meshlet.
// 5. Deferred lighting in tile memory: the spheres write a G-buffer that never leaves tile memory,
//    because one render pass writes it in a subpass and reads it as input attachments in the next,
//    where up to 64 point lights shade it. It compares with the classic path, which stores the
//    G-buffer and samples it in a second pass.
class SlangShowcaseSession : public RenderSession {
 public:
  enum class Chapter : uint8_t {
    Materials,
    InverseRendering,
    PostEffects,
    Terrain,
    TileMemory,
  };
  enum class ShadingMode : uint8_t {
    Specialized,
    Dynamic,
    Half,
  };
  enum class PostChain : uint8_t {
    Neutral,
    Film,
    Noir,
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
  // Fixes the post chain of chapter 3; otherwise it changes every few seconds.
  void setPostChain(PostChain chain) noexcept {
    fixedPostChain_ = chain;
    hasFixedPostChain_ = true;
  }
  // Chapter 4 options: look from above at what the flying camera culls, color by meshlet, and draw
  // with the vertex shader even where mesh shaders are available.
  void setTerrainOptions(bool overview, bool meshletColors, bool vertexFallback) noexcept {
    terrainOverview_ = overview;
    terrainMeshletColors_ = meshletColors;
    terrainVertexFallback_ = vertexFallback;
  }
  // Chapter 5 options: keep the G-buffer in tile memory with subpasses or store it and sample it,
  // and light the spheres with this many point lights, at most 64.
  void setTileMemoryOptions(bool subpasses, uint32_t lightCount) noexcept {
    tileSubpasses_ = subpasses;
    lightCount_ = lightCount;
  }

 private:
  bool createPipelines(TextureFormat format);
  void createInverseResources(IDevice& device);
  void createDeferredResources(IDevice& device);
  void restartFit();
  void encodeInverseStep(ICommandBuffer& commandBuffer);
  void bindSphereResources(IRenderCommandEncoder& encoder) const;
  void drawTerrain(IRenderCommandEncoder& encoder, float aspect, bool encodeGamma);
  bool renderInTileMemory(ICommandBuffer& commandBuffer, const std::shared_ptr<ITexture>& surface);
  bool renderGBuffer(ICommandBuffer& commandBuffer);
  void bindLightingResources(IRenderCommandEncoder& encoder) const;
  bool disableSubpasses(const std::string& reason);
  [[nodiscard]] bool canRenderDeferred() const;
  void drawOverlay(ShadingMode mode, PostChain chain);
  [[nodiscard]] ShadingMode currentMode() const;
  [[nodiscard]] PostChain currentPostChain() const;

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
  std::shared_ptr<IRenderPipelineState> radiancePipeline_;
  std::array<std::shared_ptr<IRenderPipelineState>, 3> postPipelines_;
  std::shared_ptr<ITexture> radianceTexture_;
  std::shared_ptr<IFramebuffer> radianceFramebuffer_;
  std::shared_ptr<ISamplerState> postSampler_;
  std::shared_ptr<IBuffer> postUniforms_;
  std::shared_ptr<IRenderPipelineState> terrainMeshPipeline_;
  std::shared_ptr<IRenderPipelineState> terrainVertexPipeline_;
  std::shared_ptr<IBuffer> terrainUniforms_;
  std::shared_ptr<IRenderPipelineState> gbufferSubpassPipeline_;
  std::shared_ptr<IRenderPipelineState> lightingSubpassPipeline_;
  std::shared_ptr<IRenderPipelineState> gbufferStoredPipeline_;
  std::shared_ptr<IRenderPipelineState> lightingStoredPipeline_;
  std::shared_ptr<IDepthStencilState> lightingDepthState_;
  std::shared_ptr<IFramebuffer> subpassFramebuffer_;
  std::array<std::shared_ptr<ITexture>, 3> gbuffer_;
  std::shared_ptr<IFramebuffer> gbufferFramebuffer_;
  std::shared_ptr<IBuffer> lights_;
  std::shared_ptr<IBuffer> deferredUniforms_;
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
  uint32_t lightCount_ = 32;
  double gpuMilliseconds_ = 0.0;
  // Why chapter 5 stores the G-buffer even when asked to keep it in tile memory; empty while
  // subpasses work.
  std::string noSubpassesReason_;
  Chapter chapter_ = Chapter::Materials;
  ShadingMode fixedMode_ = ShadingMode::Specialized;
  bool hasFixedMode_ = false;
  PostChain fixedPostChain_ = PostChain::Neutral;
  bool hasFixedPostChain_ = false;
  bool terrainOverview_ = false;
  bool terrainMeshletColors_ = false;
  bool terrainVertexFallback_ = false;
  bool showSensitivity_ = false;
  bool tileSubpasses_ = true;
  bool supported_ = false;
};

} // namespace igl::shell
