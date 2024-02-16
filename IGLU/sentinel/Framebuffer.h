/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Framebuffer.h>

namespace iglu::sentinel {

/**
 * Sentinel Frambuffer intended for safe use where access to a real framebuffer is not available.
 * Use cases include returning a reference to a framebuffer from a raw pointer when a
 * valid framebuffer is not available.
 * All methods return nullptr, the default value or an error.
 */
class Framebuffer : public igl::IFramebuffer {
 public:
  explicit Framebuffer(bool shouldAssert = true);

  [[nodiscard]] std::vector<size_t> getColorAttachmentIndices() const final;
  [[nodiscard]] std::shared_ptr<igl::ITexture> getColorAttachment(size_t index) const final;
  [[nodiscard]] std::shared_ptr<igl::ITexture> getResolveColorAttachment(size_t index) const final;
  [[nodiscard]] std::shared_ptr<igl::ITexture> getDepthAttachment() const final;
  [[nodiscard]] std::shared_ptr<igl::ITexture> getResolveDepthAttachment() const final;
  [[nodiscard]] std::shared_ptr<igl::ITexture> getStencilAttachment() const final;
  [[nodiscard]] igl::FramebufferMode getMode() const final;
  [[nodiscard]] bool isSwapchainBound() const final;
  void copyBytesColorAttachment(igl::ICommandQueue& cmdQueue,
                                size_t index,
                                void* pixelBytes,
                                const igl::TextureRangeDesc& range,
                                size_t bytesPerRow = 0) const final;
  void copyBytesDepthAttachment(igl::ICommandQueue& cmdQueue,
                                void* pixelBytes,
                                const igl::TextureRangeDesc& range,
                                size_t bytesPerRow = 0) const final;
  void copyBytesStencilAttachment(igl::ICommandQueue& cmdQueue,
                                  void* pixelBytes,
                                  const igl::TextureRangeDesc& range,
                                  size_t bytesPerRow = 0) const final;
  void copyTextureColorAttachment(igl::ICommandQueue& cmdQueue,
                                  size_t index,
                                  std::shared_ptr<igl::ITexture> destTexture,
                                  const igl::TextureRangeDesc& range) const final;
  void updateDrawable(std::shared_ptr<igl::ITexture> texture) final;
  void updateDrawable(igl::SurfaceTextures surfaceTextures) final;
  void updateResolveAttachment(std::shared_ptr<igl::ITexture> texture) final;

 private:
  [[maybe_unused]] bool shouldAssert_;
};

} // namespace iglu::sentinel
