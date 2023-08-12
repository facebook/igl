/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanImage.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImageView.h>

#ifndef VK_USE_PLATFORM_WIN32_KHR
#include <unistd.h>
#endif

namespace {
uint32_t ivkGetMemoryTypeIndex(const VkPhysicalDeviceMemoryProperties& memProps,
                               const uint32_t typeBits,
                               const VkMemoryPropertyFlags requiredProperties) {
  // Search memory types to find the index with the requested properties.
  for (uint32_t type = 0; type < memProps.memoryTypeCount; type++) {
    if ((typeBits & (1 << type)) != 0) {
      // Test if this memory type has the required properties.
      const VkFlags propertyFlags = memProps.memoryTypes[type].propertyFlags;
      if ((propertyFlags & requiredProperties) == requiredProperties) {
        return type;
      }
    }
  }
  LLOGW("Memory type %d with properties %d not found.", typeBits, requiredProperties);
  return 0;
}

} // namespace

namespace lvk::vulkan {

VulkanImage::VulkanImage(const VulkanContext& ctx,
                         VkDevice device,
                         VkImage image,
                         VkImageUsageFlags usageFlags,
                         VkFormat imageFormat,
                         VkExtent3D extent,
                         const char* debugName) :
  ctx_(ctx),
  physicalDevice_(ctx.getVkPhysicalDevice()),
  device_(device),
  vkImage_(image),
  usageFlags_(usageFlags),
  isExternallyManaged_(true),
  extent_(extent),
  type_(VK_IMAGE_TYPE_2D),
  imageFormat_(imageFormat),
  isDepthFormat_(isDepthFormat(imageFormat)),
  isStencilFormat_(isStencilFormat(imageFormat)) {
  VK_ASSERT(ivkSetDebugObjectName(device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));
}

VulkanImage::VulkanImage(const VulkanContext& ctx,
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
                         const char* debugName) :
  ctx_(ctx),
  physicalDevice_(ctx.getVkPhysicalDevice()),
  device_(device),
  usageFlags_(usageFlags),
  extent_(extent),
  type_(type),
  imageFormat_(format),
  levels_(mipLevels),
  layers_(arrayLayers),
  samples_(samples),
  isDepthFormat_(isDepthFormat(format)),
  isStencilFormat_(isStencilFormat(format)) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  LVK_ASSERT_MSG(levels_ > 0, "The image must contain at least one mip-level");
  LVK_ASSERT_MSG(layers_ > 0, "The image must contain at least one layer");
  LVK_ASSERT_MSG(imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid VkFormat value");
  LVK_ASSERT_MSG(samples_ > 0, "The image must contain at least one sample");
  LVK_ASSERT(extent.width > 0);
  LVK_ASSERT(extent.height > 0);
  LVK_ASSERT(extent.depth > 0);

  const VkImageCreateInfo ci =
      ivkGetImageCreateInfo(type, imageFormat_, tiling, usageFlags, extent_, levels_, layers_, createFlags, samples);

  if (LVK_VULKAN_USE_VMA) {
    vmaAllocInfo_.usage = memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_AUTO;
    VkResult result = vmaCreateImage((VmaAllocator)ctx_.getVmaAllocator(), &ci, &vmaAllocInfo_, &vkImage_, &vmaAllocation_, nullptr);

    if (!LVK_VERIFY(result == VK_SUCCESS)) {
      LLOGW("failed: error result: %d, memflags: %d,  imageformat: %d\n", result, memFlags, imageFormat_);
    }

    // handle memory-mapped buffers
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vmaMapMemory((VmaAllocator)ctx_.getVmaAllocator(), vmaAllocation_, &mappedPtr_);
    }
  } else {
    // create image
    VK_ASSERT(vkCreateImage(device_, &ci, nullptr, &vkImage_));

    // back the image with some memory
    {
      VkMemoryRequirements memRequirements;
      vkGetImageMemoryRequirements(device, vkImage_, &memRequirements);

      VK_ASSERT(ivkAllocateMemory(physicalDevice_, device_, &memRequirements, memFlags, &vkMemory_));
      VK_ASSERT(vkBindImageMemory(device_, vkImage_, vkMemory_, 0));
    }

    // handle memory-mapped images
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      VK_ASSERT(vkMapMemory(device_, vkMemory_, 0, VK_WHOLE_SIZE, 0, &mappedPtr_));
    }
  }

  VK_ASSERT(ivkSetDebugObjectName(device_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));

  // Get physical device's properties for the image's format
  vkGetPhysicalDeviceFormatProperties(physicalDevice_, imageFormat_, &formatProperties_);
}

VulkanImage::~VulkanImage() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  if (!isExternallyManaged_) {
    if (LVK_VULKAN_USE_VMA) {
      if (mappedPtr_) {
        vmaUnmapMemory((VmaAllocator)ctx_.getVmaAllocator(), vmaAllocation_);
      }
      ctx_.deferredTask(std::packaged_task<void()>([vma = ctx_.getVmaAllocator(), image = vkImage_, allocation = vmaAllocation_]() {
        vmaDestroyImage((VmaAllocator)vma, image, allocation);
      }));
    } else {
      if (mappedPtr_) {
        vkUnmapMemory(device_, vkMemory_);
      }
      ctx_.deferredTask(std::packaged_task<void()>([device = device_, image = vkImage_, memory = vkMemory_]() {
        vkDestroyImage(device, image, nullptr);
        if (memory != VK_NULL_HANDLE) {
          vkFreeMemory(device, memory, nullptr);
        }
      }));
    }
  }
}

std::shared_ptr<VulkanImageView> VulkanImage::createImageView(VkImageViewType type,
                                                              VkFormat format,
                                                              VkImageAspectFlags aspectMask,
                                                              uint32_t baseLevel,
                                                              uint32_t numLevels,
                                                              uint32_t baseLayer,
                                                              uint32_t numLayers,
                                                              const char* debugName) const {
  return std::make_shared<VulkanImageView>(
      ctx_, device_, vkImage_, type, format, aspectMask, baseLevel, numLevels ? numLevels : levels_, baseLayer, numLayers, debugName);
}

void VulkanImage::transitionLayout(VkCommandBuffer commandBuffer,
                                   VkImageLayout newImageLayout,
                                   VkPipelineStageFlags srcStageMask,
                                   VkPipelineStageFlags dstStageMask,
                                   const VkImageSubresourceRange& subresourceRange) const {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_TRANSITION);

  VkAccessFlags srcAccessMask = 0;
  VkAccessFlags dstAccessMask = 0;

  if (imageLayout_ == VK_IMAGE_LAYOUT_UNDEFINED) {
    // we do not need to wait for any previous operations in this case
    srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  }

  switch (srcStageMask) {
  case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
  case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
  case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
  case VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:
  case VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:
  case VK_PIPELINE_STAGE_TRANSFER_BIT:
  case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
    break;
  default:
    LVK_ASSERT_MSG(false, "Automatic access mask deduction is not implemented (yet) for this srcStageMask");
    break;
  }

  // once you want to add a new pipeline stage to this block of if's, don't forget to add it to the
  // switch() statement above
  if (srcStageMask & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) {
    srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) {
    srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_TRANSFER_BIT) {
    srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
    srcAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
  }

  switch (dstStageMask) {
  case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
  case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
  case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
  case VK_PIPELINE_STAGE_TRANSFER_BIT:
  case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
  case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
    break;
  default:
    LVK_ASSERT_MSG(false, "Automatic access mask deduction is not implemented (yet) for this dstStageMask");
    break;
  }

  // once you want to add a new pipeline stage to this block of if's, don't forget to add it to the
  // switch() statement above
  if (dstStageMask & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
    dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) {
    dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) {
    dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_TRANSFER_BIT) {
    dstAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
  }

  ivkImageMemoryBarrier(
      commandBuffer, vkImage_, srcAccessMask, dstAccessMask, imageLayout_, newImageLayout, srcStageMask, dstStageMask, subresourceRange);

  imageLayout_ = newImageLayout;
}

VkImageAspectFlags VulkanImage::getImageAspectFlags() const {
  VkImageAspectFlags flags = 0;

  flags |= isDepthFormat_ ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
  flags |= isStencilFormat_ ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
  flags |= !(isDepthFormat_ || isStencilFormat_) ? VK_IMAGE_ASPECT_COLOR_BIT : 0;

  return flags;
}

void VulkanImage::generateMipmap(VkCommandBuffer commandBuffer) const {
  LVK_PROFILER_FUNCTION();

  // Check if device supports downscaling for color or depth/stencil buffer based on image format
  {
    const uint32_t formatFeatureMask = (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

    const bool hardwareDownscalingSupported = ((formatProperties_.optimalTilingFeatures & formatFeatureMask) == formatFeatureMask);

    if (!LVK_VERIFY(hardwareDownscalingSupported)) {
      LVK_ASSERT_MSG(false, "Doesn't support hardware downscaling of this image format: {}");
      return;
    }
  }

  // Choose linear filter for color formats if supported by the device, else use nearest filter
  // Choose nearest filter by default for depth/stencil formats
  const VkFilter blitFilter = [](bool isDepthOrStencilFormat, bool imageFilterLinear) {
    if (isDepthOrStencilFormat) {
      return VK_FILTER_NEAREST;
    }
    if (imageFilterLinear) {
      return VK_FILTER_LINEAR;
    }
    return VK_FILTER_NEAREST;
  }(isDepthFormat_ || isStencilFormat_, formatProperties_.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

  const VkImageAspectFlags imageAspectFlags = getImageAspectFlags();

  ivkCmdBeginDebugUtilsLabel(commandBuffer, "Generate mipmaps", lvk::Color(1.f, 0.75f, 0.f).toFloatPtr());

  const VkImageLayout originalImageLayout = imageLayout_;

  LVK_ASSERT(originalImageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

  // 0: Transition the first mip-level - all layers - to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  transitionLayout(commandBuffer,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VkImageSubresourceRange{imageAspectFlags, 0, 1, 0, layers_});

  for (uint32_t layer = 0; layer < layers_; ++layer) {
    auto mipWidth = (int32_t)extent_.width;
    auto mipHeight = (int32_t)extent_.height;

    for (uint32_t i = 1; i < levels_; ++i) {
      // 1: Transition the i-th level to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
      //    It will be copied into from the (i-1)-th layer
      ivkImageMemoryBarrier(commandBuffer,
                            vkImage_,
                            0, /* srcAccessMask */
                            VK_ACCESS_TRANSFER_WRITE_BIT, /* dstAccessMask */
                            VK_IMAGE_LAYOUT_UNDEFINED, /* oldImageLayout */
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, /* newImageLayout */
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, /* srcStageMask */
                            VK_PIPELINE_STAGE_TRANSFER_BIT, /* dstStageMask */
                            VkImageSubresourceRange{imageAspectFlags, i, 1, layer, 1});

      const int32_t nextLevelWidth = mipWidth > 1 ? mipWidth / 2 : 1;
      const int32_t nextLevelHeight = mipHeight > 1 ? mipHeight / 2 : 1;

      const VkOffset3D srcOffsets[2] = {
          VkOffset3D{0, 0, 0},
          VkOffset3D{mipWidth, mipHeight, 1},
      };
      const VkOffset3D dstOffsets[2] = {
          VkOffset3D{0, 0, 0},
          VkOffset3D{nextLevelWidth, nextLevelHeight, 1},
      };

      // 2: Blit the image from the prev mip-level (i-1) (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) to
      // the current mip level (i) (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
#if LVK_VULKAN_PRINT_COMMANDS
      LLOGL("%p vkCmdBlitImage()\n", commandBuffer);
#endif // LVK_VULKAN_PRINT_COMMANDS
      ivkCmdBlitImage(commandBuffer,
                      vkImage_,
                      vkImage_,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      srcOffsets,
                      dstOffsets,
                      VkImageSubresourceLayers{imageAspectFlags, i - 1, layer, 1},
                      VkImageSubresourceLayers{imageAspectFlags, i, layer, 1},
                      blitFilter);

      // 3: Transition i-th level to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL as it will be read from in
      // the next iteration
      ivkImageMemoryBarrier(commandBuffer,
                            vkImage_,
                            VK_ACCESS_TRANSFER_WRITE_BIT, /* srcAccessMask */
                            VK_ACCESS_TRANSFER_READ_BIT, /* dstAccessMask */
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, /* oldImageLayout */
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, /* newImageLayout */
                            VK_PIPELINE_STAGE_TRANSFER_BIT, /* srcStageMask */
                            VK_PIPELINE_STAGE_TRANSFER_BIT /* dstStageMask */,
                            VkImageSubresourceRange{imageAspectFlags, i, 1, layer, 1});

      // Compute the size of the next mip level
      mipWidth = nextLevelWidth;
      mipHeight = nextLevelHeight;
    }
  }

  // 4: Transition all levels and layers/faces to their final layout
  ivkImageMemoryBarrier(commandBuffer,
                        vkImage_,
                        VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask
                        0, // dstAccessMask
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // oldImageLayout
                        originalImageLayout, // newImageLayout
                        VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dstStageMask
                        VkImageSubresourceRange{imageAspectFlags, 0, levels_, 0, layers_});
  ivkCmdEndDebugUtilsLabel(commandBuffer);

  imageLayout_ = originalImageLayout;
}

bool VulkanImage::isDepthFormat(VkFormat format) {
  return (format == VK_FORMAT_D16_UNORM) || (format == VK_FORMAT_X8_D24_UNORM_PACK32) || (format == VK_FORMAT_D32_SFLOAT) ||
         (format == VK_FORMAT_D16_UNORM_S8_UINT) || (format == VK_FORMAT_D24_UNORM_S8_UINT) || (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

bool VulkanImage::isStencilFormat(VkFormat format) {
  return (format == VK_FORMAT_S8_UINT) || (format == VK_FORMAT_D16_UNORM_S8_UINT) || (format == VK_FORMAT_D24_UNORM_S8_UINT) ||
         (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

} // namespace lvk::vulkan
