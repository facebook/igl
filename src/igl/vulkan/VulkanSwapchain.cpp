/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanSwapchain.h"

#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanSemaphore.h>
#include <igl/vulkan/VulkanTexture.h>

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
  for (const auto& format : formats) {
    // The preferred format should be the one which is closer to the beginning of the formats
    // container. If BGR is encountered earlier, it should be picked as the format of choice. If RGB
    // happens to be earlier, take it.
    if (igl::vulkan::isTextureFormatRGB(format.format)) {
      return false;
    }
    if (igl::vulkan::isTextureFormatBGR(format.format)) {
      return true;
    }
  }
  return false;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats,
                                           igl::TextureFormat textureFormat,
                                           igl::ColorSpace colorSpace) {
  IGL_DEBUG_ASSERT(!formats.empty());

  const bool isNativeSwapchainBGR = isNativeSwapChainBGR(formats);
  auto vulkanTextureFormat = igl::vulkan::textureFormatToVkFormat(textureFormat);
  const bool isRequestedFormatBGR = igl::vulkan::isTextureFormatBGR(vulkanTextureFormat);
  if (isNativeSwapchainBGR != isRequestedFormatBGR) {
    vulkanTextureFormat = igl::vulkan::invertRedAndBlue(vulkanTextureFormat);
  }
  const auto preferred =
      VkSurfaceFormatKHR{vulkanTextureFormat, igl::vulkan::colorSpaceToVkColorSpace(colorSpace)};

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
      "format. Defaulting to first supported format.\n");
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

VkImageUsageFlags chooseUsageFlags(const VulkanFunctionTable& vf,
                                   VkPhysicalDevice pd,
                                   VkSurfaceKHR surface,
                                   VkFormat format,
                                   VkSurfaceCapabilitiesKHR& caps) {
  VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  VK_ASSERT(vf.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps));

  const bool isStorageSupported = (caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) > 0;

  VkFormatProperties props = {};
  vf.vkGetPhysicalDeviceFormatProperties(pd, format, &props);

  const bool isTilingOptimalSupported =
      (props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) > 0;

  if (isStorageSupported && isTilingOptimalSupported) {
    usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }

  return usageFlags;
}

} // namespace

namespace igl::vulkan {

VulkanSwapchain::VulkanSwapchain(VulkanContext& ctx, uint32_t width, uint32_t height) :
  ctx_(ctx),
  device_(ctx.device_->getVkDevice()),
  graphicsQueue_(ctx.deviceQueues_.graphicsQueue),
  width_(width),
  height_(height) {
  surfaceFormat_ = chooseSwapSurfaceFormat(ctx.deviceSurfaceFormats_,
                                           ctx.config_.requestedSwapChainTextureFormat,
                                           ctx.config_.swapChainColorSpace);
  IGL_LOG_DEBUG(
      "Swapchain format: %s; colorSpace: %s\n",
      TextureFormatProperties::fromTextureFormat(vkFormatToTextureFormat(surfaceFormat_.format))
          .name,
      colorSpaceToString(vkColorSpaceToColorSpace(surfaceFormat_.colorSpace)));

  IGL_DEBUG_ASSERT(
      ctx.vkSurface_ != VK_NULL_HANDLE,
      "You are trying to create a swapchain but your OS surface is empty. Did you want to "
      "create an offscreen rendering context? If so, set 'width' and 'height' to 0 when you "
      "create your igl::IDevice");

  VkBool32 queueFamilySupportsPresentation = VK_FALSE;
  VK_ASSERT(
      ctx_.vf_.vkGetPhysicalDeviceSurfaceSupportKHR(ctx.getVkPhysicalDevice(),
                                                    ctx.deviceQueues_.graphicsQueueFamilyIndex,
                                                    ctx.vkSurface_,
                                                    &queueFamilySupportsPresentation));
  IGL_DEBUG_ASSERT(queueFamilySupportsPresentation == VK_TRUE,
                   "The queue family used with the swapchain does not support presentation");

  const VkImageUsageFlags usageFlags = chooseUsageFlags(ctx.vf_,
                                                        ctx.getVkPhysicalDevice(),
                                                        ctx.vkSurface_,
                                                        surfaceFormat_.format,
                                                        ctx.deviceSurfaceCaps_);

  {
    const uint32_t requestedSwapchainImageCount = chooseSwapImageCount(ctx.deviceSurfaceCaps_);

    VK_ASSERT(ivkCreateSwapchain(&ctx_.vf_,
                                 device_,
                                 ctx.vkSurface_,
                                 requestedSwapchainImageCount,
                                 surfaceFormat_,
                                 chooseSwapPresentMode(ctx.devicePresentModes_),
                                 &ctx.deviceSurfaceCaps_,
                                 usageFlags,
                                 ctx.deviceQueues_.graphicsQueueFamilyIndex,
                                 width,
                                 height,
                                 &swapchain_));
  }
  VK_ASSERT(ctx.vf_.vkGetSwapchainImagesKHR(device_, swapchain_, &numSwapchainImages_, nullptr));
  std::vector<VkImage> swapchainImages(numSwapchainImages_);
  swapchainImages.resize(numSwapchainImages_);
  VK_ASSERT(ctx.vf_.vkGetSwapchainImagesKHR(
      device_, swapchain_, &numSwapchainImages_, swapchainImages.data()));

  IGL_DEBUG_ASSERT(numSwapchainImages_ > 0);

  // Prevent underflow when doing (frameNumber_ - numSwapchainImages_).
  // Every resource submitted in the frame (frameNumber_ - numSwapchainImages_) or earlier is
  // guaranteed to be processed by the GPU in the frame (frameNumber_).
  frameNumber_ = numSwapchainImages_;

  // create images, image views and framebuffers
  swapchainTextures_ = std::make_unique<std::shared_ptr<VulkanTexture>[]>(numSwapchainImages_);
  for (uint32_t i = 0; i < numSwapchainImages_; i++) {
    auto image = VulkanImage(
        ctx_, device_, swapchainImages[i], IGL_FORMAT("Image: swapchain #{}", i).c_str());
    image.extent_ = {width, height, 1};
    // set usage flags for retrieved images
    image.usageFlags_ = usageFlags;
    image.imageFormat_ = surfaceFormat_.format;

    auto imageView = image.createImageView(VK_IMAGE_VIEW_TYPE_2D,
                                           surfaceFormat_.format,
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           0,
                                           VK_REMAINING_MIP_LEVELS,
                                           0,
                                           1,
                                           IGL_FORMAT("Image View: swapchain #{}", i).c_str());
    swapchainTextures_[i] = std::make_shared<VulkanTexture>(std::move(image), std::move(imageView));
  }

  // create semaphores and fences for swapchain images
  for (uint32_t i = 0; i < numSwapchainImages_; ++i) {
    timelineWaitValues_.emplace_back(0);
    acquireSemaphores_.emplace_back(
        ctx_.vf_, device_, false, IGL_FORMAT("Semaphore: swapchain-acquire #{}", i).c_str());
    if (!ctx_.timelineSemaphore_) {
      // this can be removed once we switch to timeline semaphores
      acquireFences_.emplace_back(ctx_.vf_,
                                  device_,
                                  VK_FENCE_CREATE_SIGNALED_BIT,
                                  false,
                                  IGL_FORMAT("Fence: swapchain-acquire #{}", i).c_str());
    }
  }
}

VkImage VulkanSwapchain::getDepthVkImage() const {
  if (!depthTexture_) {
    lazyAllocateDepthBuffer();
  }
  return depthTexture_->image_.getVkImage();
}

VkImageView VulkanSwapchain::getDepthVkImageView() const {
  if (!depthTexture_) {
    lazyAllocateDepthBuffer();
  }
  return depthTexture_->imageView_.getVkImageView();
}

void VulkanSwapchain::lazyAllocateDepthBuffer() const {
  IGL_DEBUG_ASSERT(!depthTexture_);

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

  auto depthImage = VulkanImage(ctx_,
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
  auto depthImageView = depthImage.createImageView(
      VK_IMAGE_VIEW_TYPE_2D, depthFormat, aspectMask, 0, 1, 0, 1, "Image View: swapchain depth");

  depthTexture_ = std::make_shared<VulkanTexture>(std::move(depthImage), std::move(depthImageView));
}

VkSemaphore VulkanSwapchain::getSemaphore() const noexcept {
  return acquireSemaphores_[currentSemaphoreIndex_].vkSemaphore_;
}

VulkanSwapchain::~VulkanSwapchain() {
  for (auto& fence : acquireFences_) {
    // this can be removed once we switch to timeline semaphores
    fence.wait();
  }
  ctx_.vf_.vkDestroySwapchainKHR(device_, swapchain_, nullptr);
}

Result VulkanSwapchain::acquireNextImage() {
  IGL_PROFILER_FUNCTION();

  VkResult acquireResult = VK_SUCCESS;

  if (ctx_.timelineSemaphore_) {
    const VkSemaphoreWaitInfo waitInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &ctx_.timelineSemaphore_->vkSemaphore_,
        .pValues = &timelineWaitValues_[currentImageIndex_],
    };
    VK_ASSERT(ctx_.vf_.vkWaitSemaphoresKHR(device_, &waitInfo, UINT64_MAX));

    VkSemaphore acquireSemaphore = acquireSemaphores_[currentImageIndex_].getVkSemaphore();
    // when timeout is set to UINT64_MAX, we wait until the next image has been acquired
    acquireResult = ctx_.vf_.vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX, acquireSemaphore, VK_NULL_HANDLE, &currentImageIndex_);

    currentSemaphoreIndex_ =
        currentImageIndex_; // remove `currentSemaphoreIndex_` once we switch to timeline semaphores
                            // (use `currentImageIndex_` instead)

    getNextImage_ = false;

    ctx_.immediate_->waitSemaphore(acquireSemaphore);
  } else {
    // this entire branch can be removed once we switch to timeline semaphores

    // Check whether the semaphore can be used for acquiring by waiting on the acquireFence_
    //   If semaphore is not VK_NULL_HANDLE it must not have any uncompleted signal or wait
    //   operations pending
    //   (https://vulkan.lunarg.com/doc/view/1.3.275.0/windows/1.3-extensions/vkspec.html#VUID-vkAcquireNextImageKHR-semaphore-01779)
    acquireFences_[currentImageIndex_].wait();
    acquireFences_[currentImageIndex_].reset();

    currentSemaphoreIndex_ = currentImageIndex_;

    // when timeout is set to UINT64_MAX, we wait until the next image has been acquired
    acquireResult =
        ctx_.vf_.vkAcquireNextImageKHR(device_,
                                       swapchain_,
                                       UINT64_MAX,
                                       acquireSemaphores_[currentImageIndex_].vkSemaphore_,
                                       acquireFences_[currentImageIndex_].vkFence_,
                                       &currentImageIndex_);
  }

  if (acquireResult == VK_SUBOPTIMAL_KHR) {
    IGL_LOG_INFO_ONCE(
        "vkAcquireNextImageKHR returned VK_SUBOPTIMAL_KHR. The Vulkan swapchain is no longer "
        "compatible with the surface");
  } else {
    VK_ASSERT_RETURN(acquireResult);
  }

  return Result();
}

Result VulkanSwapchain::present(VkSemaphore waitSemaphore) {
  IGL_PROFILER_FUNCTION();

  IGL_PROFILER_ZONE("vkQueuePresentKHR()", IGL_PROFILER_COLOR_PRESENT);
  const VkPresentInfoKHR pi = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1u,
      .pWaitSemaphores = &waitSemaphore,
      .swapchainCount = 1u,
      .pSwapchains = &swapchain_,
      .pImageIndices = &currentImageIndex_,
  };
  const VkResult presentResult = ctx_.vf_.vkQueuePresentKHR(graphicsQueue_, &pi);

  if (presentResult == VK_SUBOPTIMAL_KHR) {
    IGL_LOG_INFO_ONCE(
        "vkQueuePresentKHR() returned VK_SUBOPTIMAL_KHR. The Vulkan swapchain is no longer "
        "compatible with the surface");
  } else {
    VK_ASSERT_RETURN(presentResult);
  }
  IGL_PROFILER_ZONE_END();

  // Ready to call acquireNextImage() on the next getCurrentVulkanTexture();
  getNextImage_ = true;
  frameNumber_++;

  IGL_PROFILER_FRAME(nullptr);

  return Result();
}

} // namespace igl::vulkan
