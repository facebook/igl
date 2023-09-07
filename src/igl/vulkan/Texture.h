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

#include <vector>

namespace igl {
namespace vulkan {

class Device;
class VulkanImage;
class VulkanImageView;
class VulkanTexture;
class PlatformDevice;

class Texture final : public ITexture {
  friend class Device;
  friend class PlatformDevice;

 public:
  Texture(const igl::vulkan::Device& device, TextureFormat format);
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
  void generateMipmap(ICommandQueue& cmdQueue) const override;
  void generateMipmap(ICommandBuffer& cmdBuffer) const override;
  bool isRequiredGenerateMipmap() const override;
  uint64_t getTextureId() const override;
  VkFormat getVkFormat() const;

  VkImageView getVkImageView() const;
  VkImageView getVkImageViewForFramebuffer(uint32_t mipLevel,
                                           uint32_t layer,
                                           FramebufferMode mode) const; // framebuffers can render
                                                                        // only into 1 mip-level
  VkImage getVkImage() const;
  VulkanTexture& getVulkanTexture() const {
    IGL_ASSERT(texture_);
    return *texture_.get();
  }

  bool isSwapchainTexture() const;

  static uint32_t getVkLayer(TextureType type, uint32_t face, uint32_t layer);
  uint32_t getNumVkLayers() const;

 private:
  Result create(const TextureDesc& desc);

  Result uploadInternal(TextureType type,
                        const TextureRangeDesc& range,
                        const void* data,
                        size_t bytesPerRow) const final;

 protected:
  const igl::vulkan::Device& device_;
  TextureDesc desc_;

  std::shared_ptr<VulkanTexture> texture_;
  mutable std::vector<std::shared_ptr<VulkanImageView>> imageViewForFramebuffer_;
};

} // namespace vulkan
} // namespace igl
