/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "NativeHWBuffer.h"

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

#include <android/hardware_buffer.h>
#include <vulkan/vulkan_android.h>

#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl::vulkan::android {

NativeHWTextureBuffer::NativeHWTextureBuffer(const igl::vulkan::Device& device,
                                             TextureFormat format) :
  Super(device, format) {}

NativeHWTextureBuffer::~NativeHWTextureBuffer() {}

Result NativeHWTextureBuffer::createTextureInternal(const TextureDesc& desc,
                                                    AHardwareBuffer* buffer) {
  const VkImageUsageFlags usageFlags =
      VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  const VkFormat format = igl::vulkan::textureFormatToVkFormat(desc.format);
  const uint32_t mipLevels = igl::TextureDesc::calcNumMipLevels(desc.width, desc.height);

  auto vulkanImage = VulkanImage::createWithExportMemory(
      device_.getVulkanContext(),
      device_.getVulkanContext().getVkDevice(),
      VkExtent3D{(uint32_t)desc.width, (uint32_t)desc.height, 1},
      VK_IMAGE_TYPE_2D,
      format,
      mipLevels,
      1,
      VK_IMAGE_TILING_OPTIMAL,
      usageFlags,
      0,
      VK_SAMPLE_COUNT_1_BIT,
      buffer,
      "Image: SurfaceTexture");

  if (vulkanImage.getVkImage() == VK_NULL_HANDLE) {
    return Result(Result::Code::RuntimeError, "Failed to create vulkan image");
  }

  auto vulkanImageView =
      vulkanImage.createImageView(VK_IMAGE_VIEW_TYPE_2D, // VkImageViewType
                                  format, // VkFormat
                                  VK_IMAGE_ASPECT_COLOR_BIT, // VkImageAspectFlags
                                  0, // baseLevel
                                  VK_REMAINING_MIP_LEVELS, // numLevels
                                  0, // baseLayer
                                  1, // numLayers
                                  "Image View: SurfaceTexture");

  auto vkTexture = device_.getVulkanContext().createTexture(
      std::move(vulkanImage), std::move(vulkanImageView), "SurfaceTexture");

  if (!vkTexture) {
    return Result(Result::Code::RuntimeError, "Failed to create vulkan texture");
  }

  desc_ = std::move(desc);
  texture_ = std::move(vkTexture);

  return Result{Result::Code::Ok};
}

Result NativeHWTextureBuffer::create(const TextureDesc& desc) {
  return createHWBuffer(desc, false, false);
}

} // namespace igl::vulkan::android

#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
