/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Framebuffer.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class Framebuffer final : public IFramebuffer {
 public:
  Framebuffer(const FramebufferDesc& desc) : desc_(desc) {}
  ~Framebuffer() override = default;

  std::vector<size_t> getColorAttachmentIndices() const override;
  std::shared_ptr<ITexture> getColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getResolveColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getDepthAttachment() const override;
  std::shared_ptr<ITexture> getResolveDepthAttachment() const override;
  std::shared_ptr<ITexture> getStencilAttachment() const override;
  FramebufferMode getMode() const override;
  bool isSwapchainBound() const override;

  void copyBytesColorAttachment(ICommandQueue& cmdQueue,
                                 size_t index,
                                 void* pixelBytes,
                                 const TextureRangeDesc& range,
                                 size_t bytesPerRow) const override;
  void copyBytesDepthAttachment(ICommandQueue& cmdQueue,
                                void* pixelBytes,
                                const TextureRangeDesc& range,
                                size_t bytesPerRow) const override;
  void copyBytesStencilAttachment(ICommandQueue& cmdQueue,
                                  void* pixelBytes,
                                  const TextureRangeDesc& range,
                                  size_t bytesPerRow) const override;
  void copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                  size_t index,
                                  std::shared_ptr<ITexture> destTexture,
                                  const TextureRangeDesc& range) const override;
  void updateDrawable(std::shared_ptr<ITexture> texture) override;
  void updateDrawable(SurfaceTextures surfaceTextures) override;
  void updateResolveAttachment(std::shared_ptr<ITexture> texture) override;

 private:
  FramebufferDesc desc_;
};

} // namespace igl::d3d12
