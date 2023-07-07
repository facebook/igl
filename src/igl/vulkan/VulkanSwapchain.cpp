/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanSwapchain.h"

#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanRenderPassBuilder.h>
#include <igl/vulkan/VulkanSemaphore.h>

namespace {

struct SwapchainCapabilities {
  VkSurfaceCapabilitiesKHR caps = {};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> modes;
};

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

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats,
                                           igl::ColorSpace colorSpace) {
  IGL_ASSERT(!formats.empty());

  const VkSurfaceFormatKHR preferred =
      igl::vulkan::colorSpaceToVkSurfaceFormat(colorSpace, isNativeSwapChainBGR(formats));

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

  IGL_LOG_INFO(
      "The system could not find a native swap chain format that matched our designed swapchain "
      "format. Defaulting to first supported format.");
  // fall back to first supported device color format. On Quest 2 it'll be VK_FORMAT_R8G8B8A8_UNORM
  return formats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
  if (std::find(modes.cbegin(), modes.cend(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.cend()) {
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
  }
  // On Android (Quest 2), FIFO prevents VK_ERROR_OUT_OF_DATE_KHR
#if !IGL_PLATFORM_ANDROID
  if (std::find(modes.cbegin(), modes.cend(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.cend()) {
    return VK_PRESENT_MODE_MAILBOX_KHR;
  }
#endif // !IGL_PLATFORM_ANDROID
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

namespace igl {
namespace vulkan {

VulkanSwapchain::VulkanSwapchain(const VulkanContext& ctx, uint32_t width, uint32_t height) :
  ctx_(ctx),
  device_(ctx.device_->getVkDevice()),
  graphicsQueue_(ctx.deviceQueues_.graphicsQueue),
  width_(width),
  height_(height) {
  surfaceFormat_ =
      chooseSwapSurfaceFormat(ctx.deviceSurfaceFormats_, ctx.config_.swapChainColorSpace);
  IGL_DEBUG_LOG(
      "Swapchain format: %s; colorSpace: %s\n",
      TextureFormatProperties::fromTextureFormat(vkFormatToTextureFormat(surfaceFormat_.format))
          .name,
      colorSpaceToString(vkColorSpaceToColorSpace(surfaceFormat_.colorSpace)));

  acquireSemaphore_ =
      std::make_unique<igl::vulkan::VulkanSemaphore>(device_, "Semaphore: swapchain-acquire");

  IGL_ASSERT_MSG(
      ctx.vkSurface_ != VK_NULL_HANDLE,
      "You are trying to create a swapchain but your OS surface is empty. Did you want to "
      "create an offscreen rendering context? If so, set 'width' and 'height' to 0 when you "
      "create your igl::IDevice");

#if defined(VK_KHR_surface)
  if (ctx.extensions_.enabled(VK_KHR_SURFACE_EXTENSION_NAME)) {
    VkBool32 queueFamilySupportsPresentation = VK_FALSE;
    VK_ASSERT(vkGetPhysicalDeviceSurfaceSupportKHR(ctx.getVkPhysicalDevice(),
                                                   ctx.deviceQueues_.graphicsQueueFamilyIndex,
                                                   ctx.vkSurface_,
                                                   &queueFamilySupportsPresentation));
    IGL_ASSERT_MSG(queueFamilySupportsPresentation == VK_TRUE,
                   "The queue family used with the swapchain does not support presentation");
  }
#endif

  const VkImageUsageFlags usageFlags =
      chooseUsageFlags(ctx.getVkPhysicalDevice(), ctx.vkSurface_, surfaceFormat_.format);

  VK_ASSERT(ivkCreateSwapchain(device_,
                               ctx.vkSurface_,
                               chooseSwapImageCount(ctx.deviceSurfaceCaps_),
                               surfaceFormat_,
                               chooseSwapPresentMode(ctx.devicePresentModes_),
                               &ctx.deviceSurfaceCaps_,
                               usageFlags,
                               ctx.deviceQueues_.graphicsQueueFamilyIndex,
                               width,
                               height,
                               &swapchain_));
  VK_ASSERT(vkGetSwapchainImagesKHR(device_, swapchain_, &numSwapchainImages_, nullptr));
  std::vector<VkImage> swapchainImages(numSwapchainImages_);
  swapchainImages.resize(numSwapchainImages_);
  VK_ASSERT(
      vkGetSwapchainImagesKHR(device_, swapchain_, &numSwapchainImages_, swapchainImages.data()));

  IGL_ASSERT(numSwapchainImages_ > 0);

  // Prevent underflow when doing (frameNumber_ - numSwapchainImages_).
  // Every resource submitted in the frame (frameNumber_ - numSwapchainImages_) or earlier is
  // guaranteed to be processed by the GPU in the frame (frameNumber_).
  frameNumber_ = numSwapchainImages_;

  // create images, image views and framebuffers
  swapchainTextures_.reserve(numSwapchainImages_);
  for (uint32_t i = 0; i < numSwapchainImages_; i++) {
    auto image = std::make_shared<VulkanImage>(
        ctx_, device_, swapchainImages[i], IGL_FORMAT("Image: swapchain #{}", i).c_str());
    // set usage flags for retrieved images
    image->usageFlags_ = usageFlags;
    image->imageFormat_ = surfaceFormat_.format;

    auto imageView = image->createImageView(VK_IMAGE_VIEW_TYPE_2D,
                                            surfaceFormat_.format,
                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                            0,
                                            VK_REMAINING_MIP_LEVELS,
                                            0,
                                            1,
                                            IGL_FORMAT("Image View: swapchain #{}", i).c_str());
    swapchainTextures_.emplace_back(
        std::make_shared<VulkanTexture>(ctx_, std::move(image), std::move(imageView)));
  }
}

VkImage VulkanSwapchain::getDepthVkImage() const {
  if (!depthImage_) {
    lazyAllocateDepthBuffer();
  }
  return depthImage_->getVkImage();
}

VkImageView VulkanSwapchain::getDepthVkImageView() const {
  if (!depthImageView_) {
    lazyAllocateDepthBuffer();
  }
  return depthImageView_->getVkImageView();
}

void VulkanSwapchain::lazyAllocateDepthBuffer() const {
  IGL_ASSERT(!depthImage_);
  IGL_ASSERT(!depthImageView_);

  const VkFormat depthFormat =
#if IGL_PLATFORM_APPLE
      VK_FORMAT_D32_SFLOAT;
#else
      VK_FORMAT_D24_UNORM_S8_UINT;
#endif
  const VkImageAspectFlags aspectMask =
#if IGL_PLATFORM_APPLE
      VK_IMAGE_ASPECT_DEPTH_BIT;
#else
      VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
#endif

  depthImage_ = std::make_shared<VulkanImage>(ctx_,
                                              device_,
                                              VkExtent3D{width_, height_, 1},
                                              VK_IMAGE_TYPE_2D,
                                              depthFormat,
                                              1,
                                              1,
                                              VK_IMAGE_TILING_OPTIMAL,
                                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                              0,
                                              VK_SAMPLE_COUNT_1_BIT,
                                              "Image: swapchain depth");
  depthImageView_ = depthImage_->createImageView(
      VK_IMAGE_VIEW_TYPE_2D, depthFormat, aspectMask, 0, 1, 0, 1, "Image View: swapchain depth");

  depthTexture_ = std::make_shared<VulkanTexture>(ctx_, depthImage_, depthImageView_);
}

VulkanSwapchain::~VulkanSwapchain() {
  vkDestroySwapchainKHR(device_, swapchain_, nullptr);
}

Result VulkanSwapchain::acquireNextImage() {
  IGL_PROFILER_FUNCTION();
  // when timeout is set to UINT64_MAX, we wait until the next image has been acquired
  VK_ASSERT_RETURN(vkAcquireNextImageKHR(device_,
                                         swapchain_,
                                         UINT64_MAX,
                                         acquireSemaphore_->vkSemaphore_,
                                         VK_NULL_HANDLE,
                                         &currentImageIndex_));
  // increase the frame number every time we acquire a new swapchain image
  frameNumber_++;
  return Result();
}

Result VulkanSwapchain::present(VkSemaphore waitSemaphore) {
  IGL_PROFILER_FUNCTION();

  IGL_PROFILER_ZONE("vkQueuePresent()", IGL_PROFILER_COLOR_PRESENT);
  VK_ASSERT_RETURN(ivkQueuePresent(graphicsQueue_, waitSemaphore, swapchain_, currentImageIndex_));
  IGL_PROFILER_ZONE_END();

  // Ready to call acquireNextImage() on the next getCurrentVulkanTexture();
  getNextImage_ = true;

  IGL_PROFILER_FRAME(nullptr);

  return Result();
}

} // namespace vulkan
} // namespace igl
