/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanSwapchain.h"

#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <lvk/vulkan/VulkanUtils.h>

#include <algorithm>

namespace {

uint32_t chooseSwapImageCount(const VkSurfaceCapabilitiesKHR& caps) {
  const uint32_t desired = caps.minImageCount + 1;
  const bool exceeded = caps.maxImageCount > 0 && desired > caps.maxImageCount;
  return exceeded ? caps.maxImageCount : desired;
}

bool isNativeSwapChainBGR(const std::vector<VkSurfaceFormatKHR>& formats) {
  for (auto& format : formats) {
    // The preferred format should be the one which is closer to the beginning of the formats
    // container. If BGR is encountered earlier, it should be picked as the format of choice. If RGB
    // happens to be earlier, take it.
    if (format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_SRGB ||
        format.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32) {
      return false;
    }
    if (format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_SRGB ||
        format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32) {
      return true;
    }
  }
  return false;
}

namespace {

VkSurfaceFormatKHR colorSpaceToVkSurfaceFormat(lvk::ColorSpace colorSpace, bool isBGR) {
  switch (colorSpace) {
  case lvk::ColorSpace_SRGB_LINEAR:
    // the closest thing to sRGB linear
    return VkSurfaceFormatKHR{isBGR ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_BT709_LINEAR_EXT};
  case lvk::ColorSpace_SRGB_NONLINEAR:
    [[fallthrough]];
  default:
    // default to normal sRGB non linear.
    return VkSurfaceFormatKHR{isBGR ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  }
}

} // namespace

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats,
                                           lvk::ColorSpace colorSpace) {
  LVK_ASSERT(!formats.empty());

  const VkSurfaceFormatKHR preferred = colorSpaceToVkSurfaceFormat(colorSpace, isNativeSwapChainBGR(formats));

  for (const auto& curFormat : formats) {
    if (curFormat.format == preferred.format && curFormat.colorSpace == preferred.colorSpace) {
      return curFormat;
    }
  }

  // if we can't find a matching format and color space, fallback on matching only format
  for (const auto& curFormat : formats) {
    if (curFormat.format == preferred.format) {
      return curFormat;
    }
  }

  LLOGL(
      "The system could not find a native swap chain format that matched our designed swapchain "
      "format. Defaulting to first supported format.");
  // fall back to first supported device color format. On Quest 2 it'll be VK_FORMAT_R8G8B8A8_UNORM
  return formats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
#if defined(__linux__)
  if (std::find(modes.cbegin(), modes.cend(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.cend()) {
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
  }
#endif // __linux__
  if (std::find(modes.cbegin(), modes.cend(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.cend()) {
    return VK_PRESENT_MODE_MAILBOX_KHR;
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkImageUsageFlags chooseUsageFlags(VkPhysicalDevice pd, VkSurfaceKHR surface, VkFormat format) {
  VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  VkSurfaceCapabilitiesKHR caps = {};
  VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps));

  const bool isStorageSupported = (caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) > 0;

  VkFormatProperties props = {};
  vkGetPhysicalDeviceFormatProperties(pd, format, &props);

  const bool isTilingOptimalSupported =
      (props.optimalTilingFeatures & VK_IMAGE_USAGE_STORAGE_BIT) > 0;

  if (isStorageSupported && isTilingOptimalSupported) {
    usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }

  return usageFlags;
}

} // namespace

namespace lvk {
namespace vulkan {

VulkanSwapchain::VulkanSwapchain(VulkanContext& ctx, uint32_t width, uint32_t height) :
  ctx_(ctx), device_(ctx.vkDevice_), graphicsQueue_(ctx.deviceQueues_.graphicsQueue), width_(width), height_(height) {
  surfaceFormat_ = chooseSwapSurfaceFormat(ctx.deviceSurfaceFormats_, ctx.config_.swapChainColorSpace);

  acquireSemaphore_ = lvk::createSemaphore(device_, "Semaphore: swapchain-acquire");

  LVK_ASSERT_MSG(ctx.vkSurface_ != VK_NULL_HANDLE,
                 "You are trying to create a swapchain but your OS surface is empty. Did you want to "
                 "create an offscreen rendering context? If so, set 'width' and 'height' to 0 when you "
                 "create your lvk::IDevice");

  VkBool32 queueFamilySupportsPresentation = VK_FALSE;
  VK_ASSERT(vkGetPhysicalDeviceSurfaceSupportKHR(
      ctx.getVkPhysicalDevice(), ctx.deviceQueues_.graphicsQueueFamilyIndex, ctx.vkSurface_, &queueFamilySupportsPresentation));
  LVK_ASSERT_MSG(queueFamilySupportsPresentation == VK_TRUE, "The queue family used with the swapchain does not support presentation");

  const VkImageUsageFlags usageFlags = chooseUsageFlags(ctx.getVkPhysicalDevice(), ctx.vkSurface_, surfaceFormat_.format);
  const bool isCompositeAlphaOpaqueSupported = (ctx.deviceSurfaceCaps_.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0;
  const VkSwapchainCreateInfoKHR ci = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = ctx.vkSurface_,
      .minImageCount = chooseSwapImageCount(ctx.deviceSurfaceCaps_),
      .imageFormat = surfaceFormat_.format,
      .imageColorSpace = surfaceFormat_.colorSpace,
      .imageExtent = {.width = width, .height = height},
      .imageArrayLayers = 1,
      .imageUsage = usageFlags,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &ctx.deviceQueues_.graphicsQueueFamilyIndex,
      .preTransform = ctx.deviceSurfaceCaps_.currentTransform,
      .compositeAlpha = isCompositeAlphaOpaqueSupported ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
      .presentMode = chooseSwapPresentMode(ctx.devicePresentModes_),
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };
  VK_ASSERT(vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_));
  VK_ASSERT(vkGetSwapchainImagesKHR(device_, swapchain_, &numSwapchainImages_, nullptr));
  std::vector<VkImage> swapchainImages(numSwapchainImages_);
  swapchainImages.resize(numSwapchainImages_);
  VK_ASSERT(vkGetSwapchainImagesKHR(device_, swapchain_, &numSwapchainImages_, swapchainImages.data()));

  LVK_ASSERT(numSwapchainImages_ > 0);

  // create images, image views and framebuffers
  swapchainTextures_.reserve(numSwapchainImages_);
  for (uint32_t i = 0; i < numSwapchainImages_; i++) {
    char debugNameImage[256] = {0};
    char debugNameImageView[256] = {0};
    snprintf(debugNameImage, sizeof(debugNameImage) - 1, "Image: swapchain %u", i);
    snprintf(debugNameImageView, sizeof(debugNameImageView) - 1, "Image View: swapchain %u", i);
    auto image = std::make_shared<VulkanImage>(ctx_,
                                               device_,
                                               swapchainImages[i],
                                               usageFlags,
                                               surfaceFormat_.format,
                                               VkExtent3D{.width = width_, .height = height_, .depth = 1},
                                               debugNameImage);
    auto imageView = image->createImageView(
        VK_IMAGE_VIEW_TYPE_2D, surfaceFormat_.format, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1, debugNameImageView);
    swapchainTextures_.push_back(ctx_.texturesPool_.create(VulkanTexture(std::move(image), std::move(imageView))));
  }
}

VulkanSwapchain::~VulkanSwapchain() {
  for (TextureHandle handle : swapchainTextures_) {
    ctx_.texturesPool_.destroy(handle);
  }
  vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  vkDestroySemaphore(device_, acquireSemaphore_, nullptr);
}

VkImage VulkanSwapchain::getCurrentVkImage() const {
  if (LVK_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
    lvk::vulkan::VulkanTexture* tex = ctx_.texturesPool_.get(swapchainTextures_[currentImageIndex_]);
    return tex->image_->getVkImage();
  }
  return VK_NULL_HANDLE;
}

VkImageView VulkanSwapchain::getCurrentVkImageView() const {
  if (LVK_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
    lvk::vulkan::VulkanTexture* tex = ctx_.texturesPool_.get(swapchainTextures_[currentImageIndex_]);
    return tex->imageView_->getVkImageView();
  }
  return VK_NULL_HANDLE;
}

TextureHandle VulkanSwapchain::getCurrentTexture() {
  LVK_PROFILER_FUNCTION();

  if (getNextImage_) {
    // when timeout is set to UINT64_MAX, we wait until the next image has been acquired
    VK_ASSERT(vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, acquireSemaphore_, VK_NULL_HANDLE, &currentImageIndex_));
    getNextImage_ = false;
  }

  if (LVK_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
    return swapchainTextures_[currentImageIndex_];
  }

  return {};
}

Result VulkanSwapchain::present(VkSemaphore waitSemaphore) {
  LVK_PROFILER_FUNCTION();

  LVK_PROFILER_ZONE("vkQueuePresent()", LVK_PROFILER_COLOR_PRESENT);
  const VkPresentInfoKHR pi = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &waitSemaphore,
      .swapchainCount = 1u,
      .pSwapchains = &swapchain_,
      .pImageIndices = &currentImageIndex_,
  };
  VK_ASSERT_RETURN(vkQueuePresentKHR(graphicsQueue_, &pi));
  LVK_PROFILER_ZONE_END();

  // Ready to call acquireNextImage() on the next getCurrentVulkanTexture();
  getNextImage_ = true;

  LVK_PROFILER_FRAME(nullptr);

  return Result();
}

} // namespace vulkan
} // namespace lvk
