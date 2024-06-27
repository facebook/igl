/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/ITrackedResource.h>
#include <igl/Texture.h>
#include <vector>

namespace igl {

class ICommandQueue;

enum class FramebufferMode {
  Mono, // Default mode
  Stereo, // Single pass stereo rendering. In this mode, IGL assumes there are two layers for each
          // attachment. The first layer represents left view and the second layer
          // represents the right view.
  Multiview, // Reserved for future use
};

/**
 * @brief Represents textures associated with the frame buffer including color, depth, and stencil
 * surfaces.
 */
struct FramebufferDesc {
  struct AttachmentDesc {
    std::shared_ptr<ITexture> texture;
    std::shared_ptr<ITexture> resolveTexture;
  };

  /** @brief All color attachments */
  AttachmentDesc colorAttachments[IGL_COLOR_ATTACHMENTS_MAX] = {};
  /** @brief The depth attachment */
  AttachmentDesc depthAttachment;
  /** @brief The stencil attachment */
  AttachmentDesc stencilAttachment;

  std::string debugName;

  FramebufferMode mode = FramebufferMode::Mono;
};

/**
 * @brief Interface common to all frame buffers across all implementations
 */
class IFramebuffer : public ITrackedResource<IFramebuffer> {
 public:
  ~IFramebuffer() override = default;

  // Accessors
  /** @brief Retrieve array of all valid color and resolve color attachment indices */
  [[nodiscard]] virtual std::vector<size_t> getColorAttachmentIndices() const = 0;
  /** @brief Retrieve a specific color attachment by index */
  [[nodiscard]] virtual std::shared_ptr<ITexture> getColorAttachment(size_t index) const = 0;
  /** @brief Retrieve a specific resolve color attachment by index */
  [[nodiscard]] virtual std::shared_ptr<ITexture> getResolveColorAttachment(size_t index) const = 0;
  /** @brief Retrieve depth attachment */
  [[nodiscard]] virtual std::shared_ptr<ITexture> getDepthAttachment() const = 0;
  /** @brief Retrieve resolve depth attachment */
  [[nodiscard]] virtual std::shared_ptr<ITexture> getResolveDepthAttachment() const = 0;
  /** @brief Retrieve the stencil attachment */
  [[nodiscard]] virtual std::shared_ptr<ITexture> getStencilAttachment() const = 0;
  /** @brief Retrieve the mode that this framebuffer was created in. */
  [[nodiscard]] virtual FramebufferMode getMode() const = 0;
  /** @brief Retrieve the flag checking if framebuffer is bound to swapchain. */
  [[nodiscard]] virtual bool isSwapchainBound() const = 0;

  // Methods
  /** @brief Copy color data from the color attachment at the specified index into 'pixelBytes'.
   * Some implementations may only support index 0. If bytesPerRow is 0, it will be
   * autocalculated assuming now padding. */
  virtual void copyBytesColorAttachment(ICommandQueue& cmdQueue,
                                        size_t index,
                                        void* pixelBytes,
                                        const TextureRangeDesc& range,
                                        size_t bytesPerRow = 0) const = 0;

  /** @brief Copy depth data from the depth attachment into 'pixelBytes'. If bytesPerRow is 0, it
   * will be autocalculated assuming now padding. */
  virtual void copyBytesDepthAttachment(ICommandQueue& cmdQueue,
                                        void* pixelBytes,
                                        const TextureRangeDesc& range,
                                        size_t bytesPerRow = 0) const = 0;

  /** @brief Copy stencil data from stencil attachment into 'pixelBytes'. If bytesPerRow is 0, it
   * will be autocalculated assuming now padding. */
  virtual void copyBytesStencilAttachment(ICommandQueue& cmdQueue,
                                          void* pixelBytes,
                                          const TextureRangeDesc& range,
                                          size_t bytesPerRow = 0) const = 0;
  /** @brief Copy color data from the color attachment at the specified index into 'destTexture'.
   * Some implementations may only support index 0. */
  virtual void copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                          size_t index,
                                          std::shared_ptr<ITexture> destTexture,
                                          const TextureRangeDesc& range) const = 0;

  /** @brief Replaces color attachment at index 0 with the 'texture' specified. A null texture is
   * valid and unbinds the current color attachment at index 0. */
  virtual void updateDrawable(std::shared_ptr<ITexture> texture) = 0;

  /** @brief Replaces color attachment at index 0 and the depth texture with the textures specified.
   * A null texture is valid and unbinds the attachment. */
  virtual void updateDrawable(SurfaceTextures surfaceTextures) = 0;

  /** @brief Replaces color attachment at index 0 and the depth texture with the textures specified.
   * A null texture is valid and unbinds the attachment. */

  virtual void updateResolveAttachment(std::shared_ptr<ITexture> texture) = 0;
};

} // namespace igl
