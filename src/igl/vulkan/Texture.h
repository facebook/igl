/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/Framebuffer.h>
#include <igl/Texture.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanImageView.h>

#include <vector>

namespace igl::vulkan {

class Device;
class VulkanImage;
class VulkanTexture;
class PlatformDevice;

/// @brief Implements the igl::ITexture interface
class Texture : public ITexture {
  friend class Device;
  friend class PlatformDevice;

 public:
  /// @brief Initializes an instance of the class, but does not create the resource on the device
  /// until `create()` is called
  Texture(const igl::vulkan::Device& device, TextureFormat format);

  /// @brief Initializes an instance of the class with an existing VulkanTexture object.
  Texture(const igl::vulkan::Device& device,
          std::shared_ptr<VulkanTexture> vkTexture,
          TextureDesc desc) :
    Texture(device, desc.format) {
    texture_ = std::move(vkTexture);
    desc_ = std::move(desc);
  }

  // Accessors
  Dimensions getDimensions() const override;
  size_t getNumLayers() const override;
  TextureType getType() const override;
  TextureDesc::TextureUsage getUsage() const override;
  uint32_t getSamples() const override;
  uint32_t getNumMipLevels() const override;
  void generateMipmap(ICommandQueue& cmdQueue,
                      const TextureRangeDesc* IGL_NULLABLE range = nullptr) const override;
  void generateMipmap(ICommandBuffer& cmdBuffer,
                      const TextureRangeDesc* IGL_NULLABLE range = nullptr) const override;
  bool isRequiredGenerateMipmap() const override;
  uint64_t getTextureId() const override;
  bool isSwapchainTexture() const override;
  VkFormat getVkFormat() const;

  VkImageView getVkImageView() const;

  /// @brief Specialization of `getVkImageView()` that returns an image view specific to a mip level
  /// and layer of an image. Used to retrieve image views to be used with framebuffers
  VkImageView getVkImageViewForFramebuffer(uint32_t mipLevel,
                                           uint32_t layer,
                                           FramebufferMode mode) const; // framebuffers can render
                                                                        // only into 1 mip-level
  VkImage getVkImage() const;
  VulkanTexture& getVulkanTexture() const {
    IGL_ASSERT(texture_);
    return *texture_;
  }

  uint32_t getNumVkLayers() const;

 private:
  [[nodiscard]] bool needsRepacking(const TextureRangeDesc& range, size_t bytesPerRow) const final;

  /// @brief Uploads the texture's data to the device using the staging device in the context. This
  /// function is not synchronous and the data may or may not be available to the GPU upon return.
  Result uploadInternal(TextureType type,
                        const TextureRangeDesc& range,
                        const void* data,
                        size_t bytesPerRow) const final;

  void clearColorTexture(const igl::Color& rgba);

 protected:
  const igl::vulkan::Device& device_;
  TextureDesc desc_;

  std::shared_ptr<VulkanTexture> texture_;
  mutable std::vector<VulkanImageView> imageViewsForFramebufferMono_;
  mutable std::vector<VulkanImageView> imageViewsForFramebufferStereo_;

  /// @brief Creates the resource on the device given the properties in `desc`. This function should
  /// only be called by the `Device` class, from its `vulkan::Device::createTexture()`
  virtual Result create(const TextureDesc& desc);
};

} // namespace igl::vulkan
