/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <lvk/LVK.h>
#include <igl/vulkan/Common.h>

#include <vector>

namespace igl {
namespace vulkan {

class Device;
class VulkanImage;
class VulkanImageView;
class VulkanTexture;

class Texture final : public ITexture {
  friend class Device;

 public:
  Texture(const igl::vulkan::Device& device, TextureFormat format);
  Texture(const igl::vulkan::Device& device,
          std::shared_ptr<VulkanTexture> vkTexture,
          TextureDesc desc) :
    Texture(device, desc.format) {
    texture_ = std::move(vkTexture);
    desc_ = std::move(desc);
  }

  Result upload(const TextureRangeDesc& range, const void* data[]) const override;

  // Accessors
  Dimensions getDimensions() const override;
  void generateMipmap() const override;
  uint32_t getTextureId() const override;

  VkFormat getVkFormat() const;
  VkImageView getVkImageView() const; // all mip-levels
  VkImageView getVkImageViewForFramebuffer(uint32_t level) const; // framebuffers can render only into 1 mip-level
  VkImage getVkImage() const;
  VulkanTexture& getVulkanTexture() const {
    IGL_ASSERT(texture_.get());
    return *texture_.get();
  }

  bool isSwapchainTexture() const;

 private:
  Result create(const TextureDesc& desc);
  Result validateRange(const igl::TextureRangeDesc& range) const;

 protected:
  const igl::vulkan::Device& device_;
  TextureDesc desc_;

  std::shared_ptr<VulkanTexture> texture_;
  mutable std::vector<std::shared_ptr<VulkanImageView>> imageViewForFramebuffer_;
};

} // namespace vulkan
} // namespace igl
