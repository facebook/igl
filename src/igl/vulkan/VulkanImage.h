/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

#include <lvk/vulkan/VulkanUtils.h>

namespace lvk {
namespace vulkan {

class VulkanContext;
class VulkanImageView;

/**
 * @brief Encapsulates a Vulkan Image object (`VkImage`) along with some of its properties
 */
class VulkanImage final {
 public:
  /**
   * @brief Constructs a `VulkanImage` object from a `VkImage` object. If a debug name is provided,
   * the constructor will assign it to the `VkImage` object. No other Vulkan functions are called
   */
  VulkanImage(const VulkanContext& ctx,
              VkDevice device,
              VkExtent3D extent,
              VkImageType type,
              VkFormat format,
              uint32_t mipLevels,
              uint32_t arrayLayers,
              VkImageTiling tiling,
              VkImageUsageFlags usageFlags,
              VkMemoryPropertyFlags memFlags,
              VkImageCreateFlags createFlags,
              VkSampleCountFlagBits samples,
              const char* debugName = nullptr);
  // for swapchain images
  VulkanImage(const VulkanContext& ctx,
              VkDevice device,
              VkImage image,
              VkImageUsageFlags usageFlags,
              VkFormat imageFormat,
              VkExtent3D extent,
              const char* debugName);
  ~VulkanImage();

  VulkanImage(const VulkanImage&) = delete;
  VulkanImage& operator=(const VulkanImage&) = delete;

  VkImage getVkImage() const {
    return vkImage_;
  }

  VkImageUsageFlags getVkImageUsageFlags() const {
    return usageFlags_;
  }

  bool isSampledImage() const {
    return (usageFlags_ & VK_IMAGE_USAGE_SAMPLED_BIT) > 0;
  }

  bool isStorageImage() const {
    return (usageFlags_ & VK_IMAGE_USAGE_STORAGE_BIT) > 0;
  }

  /**
   * @brief Creates a `VkImageView` object from the `VkImage` stored in the object.
   *
   * Setting `numLevels` to a non-zero value will override `mipLevels_` value from the original
   * vulkan image, and can be used to create image views with different number of levels
   */
  std::shared_ptr<VulkanImageView> createImageView(VkImageViewType type,
                                                   VkFormat format,
                                                   VkImageAspectFlags aspectMask,
                                                   uint32_t baseLevel,
                                                   uint32_t numLevels = VK_REMAINING_MIP_LEVELS,
                                                   uint32_t baseLayer = 0,
                                                   uint32_t numLayers = 1,
                                                   const char* debugName = nullptr) const;

  void generateMipmap(VkCommandBuffer commandBuffer) const;

  /**
   * @brief Transitions the `VkImage`'s layout from the current layout (stored in the object) to the
   * `newImageLayout` by recording an Image Memory Barrier into the commandBuffer.
   *
   * The source and destination access masks for the transition are automatically deduced based on
   * the `srcStageMask` and the `dstStageMask` parameters. Not not all `VkPipelineStageFlags` are
   * supported.
   */
  void transitionLayout(VkCommandBuffer commandBuffer,
                        VkImageLayout newImageLayout,
                        VkPipelineStageFlags srcStageMask,
                        VkPipelineStageFlags dstStageMask,
                        const VkImageSubresourceRange& subresourceRange) const;

  VkImageAspectFlags getImageAspectFlags() const;

  static bool isDepthFormat(VkFormat format);
  static bool isStencilFormat(VkFormat format);

 public:
  const VulkanContext& ctx_;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkImage vkImage_ = VK_NULL_HANDLE;
  VkImageUsageFlags usageFlags_ = 0;
  VkDeviceMemory vkMemory_ = VK_NULL_HANDLE;
  VmaAllocationCreateInfo vmaAllocInfo_ = {};
  VmaAllocation vmaAllocation_ = VK_NULL_HANDLE;
  VkFormatProperties formatProperties_ = {};
  void* mappedPtr_ = nullptr;
  bool isExternallyManaged_ = false;
  VkExtent3D extent_ = {0, 0, 0};
  VkImageType type_ = VK_IMAGE_TYPE_MAX_ENUM;
  VkFormat imageFormat_ = VK_FORMAT_UNDEFINED;
  uint32_t levels_ = 1u;
  uint32_t layers_ = 1u;
  VkSampleCountFlagBits samples_ = VK_SAMPLE_COUNT_1_BIT;
  bool isDepthFormat_ = false;
  bool isStencilFormat_ = false;
  mutable VkImageLayout imageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED; // current image layout
};

} // namespace vulkan
} // namespace lvk
