/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <array>
#include <string>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <igl/IGL.h>
#include <igl/RenderPass.h>

namespace igl::shell {

/// Tile-based deferred shading of a static scene: a G-buffer pass writes albedo, normal and
/// material, and a lighting pass reads them at the same pixel and writes the image.
///
/// With DeviceFeatures::Subpasses both passes are subpasses of one render pass, and the G-buffer
/// stays in tile memory. Other devices render the G-buffer to textures and sample them in a second
/// render pass. Both paths produce the same pixels. A device that cannot run the passes, because
/// it has no multiple render targets or its OpenGL shader version is too old, only clears the
/// image.
class SubpassDeferredSession final : public RenderSession {
 public:
  explicit SubpassDeferredSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;
  void releaseFramebuffer() override;

  /// Whether the last frame ran the G-buffer and lighting passes as subpasses of one render pass
  [[nodiscard]] bool usesSubpasses() const noexcept {
    return usesSubpasses_;
  }

 private:
  bool renderWithSubpasses(ICommandBuffer& buffer, const SurfaceTextures& surfaceTextures);
  void renderWithPasses(ICommandBuffer& buffer, const SurfaceTextures& surfaceTextures);
  bool disableSubpasses(const std::string& reason);
  void disableDeferred();

  // One render pass with two subpasses
  RenderPassDesc subpassRenderPass_;
  std::shared_ptr<IRenderPipelineState> gBufferSubpassPipeline_;
  std::shared_ptr<IRenderPipelineState> lightingSubpassPipeline_;

  // Two render passes: the G-buffer pass, and the lighting pass that samples it
  RenderPassDesc gBufferRenderPass_;
  RenderPassDesc lightingRenderPass_;
  std::shared_ptr<IFramebuffer> gBufferFramebuffer_;
  std::shared_ptr<IRenderPipelineState> gBufferPipeline_;
  std::shared_ptr<IRenderPipelineState> lightingPipeline_;
  std::shared_ptr<ISamplerState> sampler_;

  // Albedo, normal and material
  std::array<std::shared_ptr<ITexture>, 3> gBuffer_;

  // False if the device cannot run the G-buffer and lighting passes, or failed to create their
  // resources, and the session only clears the image
  bool deferred_ = false;
  bool usesSubpasses_ = false;
  bool subpassesUnsupported_ = false;
  bool pathLogged_ = false;
};

} // namespace igl::shell
