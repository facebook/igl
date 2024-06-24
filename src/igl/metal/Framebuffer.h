/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Framebuffer.h>

@protocol MTLTexture;

namespace igl {

class ICommandQueue;
class ITexture;

namespace metal {

class Framebuffer : public IFramebuffer {
 public:
  explicit Framebuffer(FramebufferDesc value);
  ~Framebuffer() override = default;

  // Accessors
  [[nodiscard]] std::vector<size_t> getColorAttachmentIndices() const override;
  [[nodiscard]] std::shared_ptr<ITexture> getColorAttachment(size_t index) const override;
  [[nodiscard]] std::shared_ptr<ITexture> getResolveColorAttachment(size_t index) const override;
  [[nodiscard]] std::shared_ptr<ITexture> getDepthAttachment() const override;
  [[nodiscard]] std::shared_ptr<ITexture> getResolveDepthAttachment() const override;
  [[nodiscard]] std::shared_ptr<ITexture> getStencilAttachment() const override;
  [[nodiscard]] FramebufferMode getMode() const override;
  [[nodiscard]] bool isSwapchainBound() const override;

  // Methods
  void copyBytesColorAttachment(ICommandQueue& cmdQueue,
                                size_t index,
                                void* pixelBytes,
                                const TextureRangeDesc& range,
                                size_t bytesPerRow = 0) const override;

  void copyBytesDepthAttachment(ICommandQueue& cmdQueue,
                                void* pixelBytes,
                                const TextureRangeDesc& range,
                                size_t bytesPerRow = 0) const override;

  void copyBytesStencilAttachment(ICommandQueue& cmdQueue,
                                  void* pixelBytes,
                                  const TextureRangeDesc& range,
                                  size_t bytesPerRow = 0) const override;

  void copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                  size_t index,
                                  std::shared_ptr<ITexture> destTexture,
                                  const TextureRangeDesc& range) const override;

  void updateDrawable(std::shared_ptr<ITexture> texture) override;
  void updateDrawable(SurfaceTextures surfaceTextures) override;
  void updateResolveAttachment(std::shared_ptr<ITexture> texture) override;

  IGL_INLINE const FramebufferDesc& get() const {
    return value_;
  }

 private:
  void copyBytes(ICommandQueue& cmdQueue,
                 const std::shared_ptr<ITexture>& iglTexture,
                 void* pixelBytes,
                 const TextureRangeDesc& range,
                 size_t bytesPerRow) const;
  virtual bool canCopy(ICommandQueue& cmdQueue,
                       id<MTLTexture> texture,
                       const TextureRangeDesc& range) const = 0;

  FramebufferDesc value_;
};

} // namespace metal
} // namespace igl
