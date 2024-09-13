/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>

#include <igl/Framebuffer.h>
#include <igl/vulkan/Common.h>

namespace igl::vulkan {

class Device;
class VulkanFramebuffer;

/// @brief Implements the igl::IFramebuffer interface for Vulkan
/// Vulkan framebuffers are immutable and are made of one or more image views. This class keeps
/// track of all framebuffers for each combination of mip level, layer, and render pass in an
/// unordered map. Framebuffers are lazily created when requested with `getVkFramebuffer()`
class Framebuffer final : public IFramebuffer {
 public:
  Framebuffer(const Device& device, FramebufferDesc desc);
  ~Framebuffer() override = default;

  // Accessors
  std::vector<size_t> getColorAttachmentIndices() const override;
  std::shared_ptr<ITexture> getColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getResolveColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getDepthAttachment() const override;
  std::shared_ptr<ITexture> getResolveDepthAttachment() const override;
  std::shared_ptr<ITexture> getStencilAttachment() const override;
  [[nodiscard]] FramebufferMode getMode() const override;
  [[nodiscard]] bool isSwapchainBound() const override;

  /// @brief Copies color attachment to location pointed by `pixelBytes`. This function only
  /// supports copying one face, one layer, and one mip level at a time. This function is
  /// synchronous and the data is expected to be available at `pixelBytes` location upon return.
  void copyBytesColorAttachment(ICommandQueue& /* Not Used */,
                                size_t index,
                                void* pixelBytes,
                                const TextureRangeDesc& range,
                                size_t bytesPerRow = 0) const override;

  /// @brief Not implemented.
  void copyBytesDepthAttachment(ICommandQueue& cmdQueue,
                                void* pixelBytes,
                                const TextureRangeDesc& range,
                                size_t bytesPerRow = 0) const override;

  /// @brief Not implemented.
  void copyBytesStencilAttachment(ICommandQueue& cmdQueue,
                                  void* pixelBytes,
                                  const TextureRangeDesc& range,
                                  size_t bytesPerRow = 0) const override;

  ///  @brief Copies a range of the color attachment at `index` to destination texture. This
  ///  function is asynchronous and the data may or may not be available at the destination texture
  ///  upon return.
  void copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                  size_t index,
                                  std::shared_ptr<ITexture> destTexture,
                                  const TextureRangeDesc& range) const override;

  /// @brief Updates the framebuffer's color attachment at index 0 with the texture passed in as a
  /// parameter
  void updateDrawable(std::shared_ptr<ITexture> texture) override;

  /// @brief Updates the framebuffer's color attachment at index 0 and the depth/stencil attachment
  /// with the contents of SurfaceTextures passed in as a parameter. If a stencil texture is not
  /// provided, the stencil attachment is set to null.
  void updateDrawable(SurfaceTextures surfaceTextures) override;

  /// @brief Updates the color attachment's resolve texture at index 0 with the texture passed in as
  /// a parameter
  void updateResolveAttachment(std::shared_ptr<ITexture> texture) override;

  /** @brief Returns the underlying Vulkan framebuffer handle for the given mip level, layer, and
   * render pass. Vulkan framebuffers are immutable and are made of one or more image views. This
   * class keeps track of all framebuffers for each combination of mip level, layer, and render
   * pass. When requesting a framebuffer for a mip level, layer, or render pass, this function
   * looks for an existing framebuffer and returns it, if it exists. Otherwise, it creates a new
   * framebuffer and stores it in the cache.
   */
  VkFramebuffer getVkFramebuffer(uint32_t mipLevel, uint32_t layer, VkRenderPass pass) const;

  uint32_t getWidth() const {
    return width_;
  }
  uint32_t getHeight() const {
    return height_;
  }

  IGL_INLINE const FramebufferDesc& getDesc() const {
    return desc_;
  }

  VkRenderPassBeginInfo getRenderPassBeginInfo(VkRenderPass renderPass,
                                               uint32_t mipLevel,
                                               uint32_t layer,
                                               uint32_t numClearValues,
                                               const VkClearValue* clearValues) const;

  /// @brief Structure used as key for unordered map based on all framebuffer attachments
  struct Attachments {
    std::vector<VkImageView> attachments_;
    bool operator==(const Attachments& other) const {
      return attachments_ == other.attachments_;
    }
  };

  struct HashFunction {
    uint64_t operator()(const Attachments& attachments) const;
  };

 private:
  void validateAttachments();
  void updateDrawableInternal(SurfaceTextures surfaceTextures, bool updateDepthStencil);

  const igl::vulkan::Device& device_;
  FramebufferDesc desc_; // attachments

  uint32_t width_ = 0;
  uint32_t height_ = 0;

  /// @brief Cache of framebuffers created from the same set of attachments
  mutable std::unordered_map<Attachments, std::shared_ptr<VulkanFramebuffer>, HashFunction>
      framebuffers_;
};

} // namespace igl::vulkan
