/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/core.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImageView.h>
#include <shell/openxr/XrLog.h>

#include "XrSwapchainProviderImplVulkan.h"

namespace igl::shell::openxr::desktop {
namespace {
void enumerateSwapchainImages(
    igl::IDevice& device,
    XrSwapchain swapchain,
    int64_t format,
    const XrViewConfigurationView& viewport,
    uint32_t numViews,
    VkImageUsageFlags usageFlags,
    VkImageAspectFlags aspectMask,
    std::vector<std::shared_ptr<igl::vulkan::VulkanTexture>>& outVulkanTextures) {
  uint32_t numImages = 0;
  XR_CHECK(xrEnumerateSwapchainImages(swapchain, 0, &numImages, NULL));

  IGL_LOG_INFO("numImages: %d", numImages);

  std::vector<XrSwapchainImageVulkanKHR> images(
      numImages, {.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR, .next = nullptr});
  XR_CHECK(xrEnumerateSwapchainImages(
      swapchain, numImages, &numImages, (XrSwapchainImageBaseHeader*)images.data()));

  const auto& actualDevice = static_cast<igl::vulkan::Device&>(device);
  const auto& ctx = actualDevice.getVulkanContext();
  outVulkanTextures.reserve(numImages);

  for (uint32_t i = 0; i < numImages; i++) {
    auto image = igl::vulkan::VulkanImage(
        ctx,
        ctx.device_->device_,
        images[i].image,
        fmt::format("Image: swapchain #{}", i).c_str(),
        usageFlags,
        true,
        VkExtent3D{viewport.recommendedImageRectWidth, viewport.recommendedImageRectHeight, 0},
        VK_IMAGE_TYPE_2D,
        static_cast<VkFormat>(format),
        1,
        numViews);

    auto imageView =
        image.createImageView(numViews > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
                              (VkFormat)format,
                              aspectMask,
                              0,
                              VK_REMAINING_MIP_LEVELS,
                              0,
                              numViews,
                              fmt::format("Image View: swapchain #{}", i).c_str());
    outVulkanTextures.emplace_back(
        std::make_shared<igl::vulkan::VulkanTexture>(ctx, std::move(image), std::move(imageView)));
  }
}

std::shared_ptr<igl::ITexture> getSurfaceTexture(
    igl::IDevice& device,
    const XrSwapchain& swapchain,
    const XrViewConfigurationView& viewport,
    uint32_t numViews,
    const std::vector<std::shared_ptr<igl::vulkan::VulkanTexture>>& vulkanTextures,
    int64_t externalTextureFormat,
    std::vector<std::shared_ptr<igl::ITexture>>& inOutTextures) {
  uint32_t imageIndex;
  XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  XR_CHECK(xrAcquireSwapchainImage(swapchain, &acquireInfo, &imageIndex));

  XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  waitInfo.timeout = XR_INFINITE_DURATION;
  XR_CHECK(xrWaitSwapchainImage(swapchain, &waitInfo));

  auto vulkanTexture = vulkanTextures[imageIndex];

  if (imageIndex >= inOutTextures.size()) {
    inOutTextures.resize((size_t)imageIndex + 1, nullptr);
  }

  const auto& actualDevice = static_cast<igl::vulkan::Device&>(device);
  const auto iglFormat =
      vulkan::vkFormatToTextureFormat(static_cast<VkFormat>(externalTextureFormat));
  const auto texture = inOutTextures[imageIndex];
  // allocate new drawable textures if its null or mismatches in size or format
  if (!texture || viewport.recommendedImageRectWidth != texture->getSize().width ||
      viewport.recommendedImageRectHeight != texture->getSize().height ||
      iglFormat != texture->getProperties().format) {
    TextureDesc textureDesc;
    if (numViews > 1) {
      textureDesc = TextureDesc::new2DArray(iglFormat,
                                            viewport.recommendedImageRectWidth,
                                            viewport.recommendedImageRectHeight,
                                            numViews,
                                            TextureDesc::TextureUsageBits::Attachment,
                                            "SwapChain Texture");
    } else {
      textureDesc = TextureDesc::new2D(iglFormat,
                                       viewport.recommendedImageRectWidth,
                                       viewport.recommendedImageRectHeight,
                                       TextureDesc::TextureUsageBits::Attachment,
                                       "SwapChain Texture");
    }

    inOutTextures[imageIndex] =
        std::make_shared<igl::vulkan::Texture>(actualDevice, vulkanTexture, textureDesc);
  }

  return inOutTextures[imageIndex];
}
} // namespace

void XrSwapchainProviderImplVulkan::enumerateImages(igl::IDevice& device,
                                                    XrSwapchain colorSwapchain,
                                                    XrSwapchain depthSwapchain,
                                                    int64_t selectedColorFormat,
                                                    int64_t selectedDepthFormat,
                                                    const XrViewConfigurationView& viewport,
                                                    uint32_t numViews) {
  enumerateSwapchainImages(device,
                           colorSwapchain,
                           selectedColorFormat,
                           viewport,
                           numViews,
                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                           VK_IMAGE_ASPECT_COLOR_BIT,
                           vulkanColorTextures_);
  auto vkDepthFormat = static_cast<VkFormat>(selectedDepthFormat);
  VkImageAspectFlags depthAspectFlags = 0;
  if (vulkan::VulkanImage::isDepthFormat(vkDepthFormat)) {
    depthAspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  if (vulkan::VulkanImage::isStencilFormat(vkDepthFormat)) {
    depthAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  enumerateSwapchainImages(device,
                           depthSwapchain,
                           selectedDepthFormat,
                           viewport,
                           numViews,
                           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                           depthAspectFlags,
                           vulkanDepthTextures_);
}

igl::SurfaceTextures XrSwapchainProviderImplVulkan::getSurfaceTextures(
    igl::IDevice& device,
    const XrSwapchain& colorSwapchain,
    const XrSwapchain& depthSwapchain,
    int64_t selectedColorFormat,
    int64_t selectedDepthFormat,
    const XrViewConfigurationView& viewport,
    uint32_t numViews) {
  auto colorTexture = getSurfaceTexture(device,
                                        colorSwapchain,
                                        viewport,
                                        numViews,
                                        vulkanColorTextures_,
                                        selectedColorFormat,
                                        colorTextures_);
  auto depthTexture = getSurfaceTexture(device,
                                        depthSwapchain,
                                        viewport,
                                        numViews,
                                        vulkanDepthTextures_,
                                        selectedDepthFormat,
                                        depthTextures_);

  return {colorTexture, depthTexture};
}
} // namespace igl::shell::openxr::desktop
