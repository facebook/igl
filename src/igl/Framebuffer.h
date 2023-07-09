/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

namespace igl {

class ITexture;
struct TextureRangeDesc;

/**
 * @brief Represents textures associated with the frame buffer including color, depth, and stencil
 * surfaces.
 */
struct FramebufferDesc {
  struct AttachmentDesc {
    std::shared_ptr<ITexture> texture;
    std::shared_ptr<ITexture> resolveTexture;
  };

  /** @brief Mapping of index to specific color texture attachments */
  uint32_t numColorAttachments = 0;
  AttachmentDesc colorAttachments[RenderPipelineDesc::IGL_COLOR_ATTACHMENTS_MAX];
  /** @brief The depth texture attachment */
  AttachmentDesc depthAttachment;
  /** @brief The stencil texture attachment */
  AttachmentDesc stencilAttachment;

  std::string debugName;
};

/**
 * @brief Interface common to all frame buffers across all implementations
 */
class IFramebuffer {
 public:
  virtual ~IFramebuffer() = default;

  // Accessors
  virtual uint32_t getNumColorAttachments() const = 0;
  /** @brief Retrieve a specific color attachment by index */
  virtual std::shared_ptr<ITexture> getColorAttachment(uint32_t index) const = 0;
  /** @brief Retrieve a specific resolve color attachment by index */
  virtual std::shared_ptr<ITexture> getResolveColorAttachment(uint32_t index) const = 0;
  /** @brief Retrieve depth attachment */
  virtual std::shared_ptr<ITexture> getDepthAttachment() const = 0;
  /** @brief Retrieve resolve depth attachment */
  virtual std::shared_ptr<ITexture> getResolveDepthAttachment() const = 0;
  /** @brief Retrieve the stencil attachment */
  virtual std::shared_ptr<ITexture> getStencilAttachment() const = 0;

  // Methods
  /** @brief Copy color data from the color attachment at the specified index into 'pixelBytes'.
   * Some implementations may only support index 0. */
  virtual void copyBytesColorAttachment(size_t index,
                                        void* pixelBytes,
                                        size_t bytesPerRow,
                                        const TextureRangeDesc& range) const = 0;

  /** @brief Copy color data from the color attachment at the specified index into 'destTexture'.
   * Some implementations may only support index 0. */
  virtual void copyTextureColorAttachment(size_t index,
                                          std::shared_ptr<ITexture> destTexture,
                                          const TextureRangeDesc& range) const = 0;

  /** @brief Replaces color attachment at index 0 with the 'texture' specified. A null texture is
   * valid and unbinds the current color attachment at index 0. */
  virtual std::shared_ptr<ITexture> updateDrawable(std::shared_ptr<ITexture> texture) = 0;
};

} // namespace igl
