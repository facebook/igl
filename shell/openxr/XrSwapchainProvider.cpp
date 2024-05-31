/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/Core.h>

#include <shell/openxr/XrLog.h>
#include <shell/openxr/XrSwapchainProvider.h>
#include <shell/openxr/impl/XrSwapchainProviderImpl.h>

namespace igl::shell::openxr {
XrSwapchainProvider::XrSwapchainProvider(std::unique_ptr<impl::XrSwapchainProviderImpl>&& impl,
                                         std::shared_ptr<igl::shell::Platform> platform,
                                         XrSession session,
                                         impl::SwapchainImageInfo swapchainImageInfo,
                                         uint8_t numViews) noexcept :
  impl_(std::move(impl)),
  platform_(std::move(platform)),
  session_(session),
  swapchainImageInfo_(swapchainImageInfo),
  numViews_(numViews) {}

XrSwapchainProvider::~XrSwapchainProvider() noexcept {
  if (colorSwapchain_ != XR_NULL_HANDLE) {
    xrDestroySwapchain(colorSwapchain_);
  }
  if (depthSwapchain_ != XR_NULL_HANDLE) {
    xrDestroySwapchain(depthSwapchain_);
  }
}

bool XrSwapchainProvider::initialize() noexcept {
  uint32_t numSwapchainFormats = 0;
  XR_CHECK(xrEnumerateSwapchainFormats(session_, 0, &numSwapchainFormats, nullptr));

  std::vector<int64_t> swapchainFormats(numSwapchainFormats);
  XR_CHECK(xrEnumerateSwapchainFormats(
      session_, numSwapchainFormats, &numSwapchainFormats, swapchainFormats.data()));

  const auto colorFormat = swapchainImageInfo_.colorFormat != impl::kSwapchainImageInvalidFormat
                               ? swapchainImageInfo_.colorFormat
                               : impl_->preferredColorFormat();
  if (std::any_of(std::begin(swapchainFormats),
                  std::end(swapchainFormats),
                  [&](const auto& format) { return format == colorFormat; })) {
    swapchainImageInfo_.colorFormat = colorFormat;
  } else {
    IGL_ASSERT_MSG(false, "No supported color format found");
    return false;
  }

  colorSwapchain_ =
      createXrSwapchain(XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, swapchainImageInfo_.colorFormat);

  const auto depthFormat = swapchainImageInfo_.depthFormat != impl::kSwapchainImageInvalidFormat
                               ? swapchainImageInfo_.depthFormat
                               : impl_->preferredDepthFormat();
  if (std::any_of(std::begin(swapchainFormats),
                  std::end(swapchainFormats),
                  [&](const auto& format) { return format == depthFormat; })) {
    swapchainImageInfo_.depthFormat = depthFormat;
  } else {
    IGL_ASSERT_MSG(false, "No supported depth format found");
    return false;
  }

  depthSwapchain_ = createXrSwapchain(XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                      swapchainImageInfo_.depthFormat);

  impl_->enumerateImages(
      platform_->getDevice(), colorSwapchain_, depthSwapchain_, swapchainImageInfo_, numViews_);

  return true;
}

XrSwapchain XrSwapchainProvider::createXrSwapchain(XrSwapchainUsageFlags extraUsageFlags,
                                                   int64_t format) noexcept {
  const XrSwapchainCreateInfo swapChainCreateInfo = {.type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
                                                     .next = nullptr,
                                                     .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                                                                   extraUsageFlags,
                                                     .format = format,
                                                     .sampleCount = 1,
                                                     .width = swapchainImageInfo_.imageWidth,
                                                     .height = swapchainImageInfo_.imageHeight,
                                                     .faceCount = 1,
                                                     .arraySize = numViews_,
                                                     .mipCount = 1};

  XrSwapchain swapchain;
  XR_CHECK(xrCreateSwapchain(session_, &swapChainCreateInfo, &swapchain));
  IGL_LOG_INFO("XrSwapchain created\n");
  IGL_LOG_INFO("XrSwapchain image width: %d\n", swapchainImageInfo_.imageWidth);
  IGL_LOG_INFO("XrSwapchain image height: %d\n", swapchainImageInfo_.imageHeight);

  return swapchain;
}

igl::SurfaceTextures XrSwapchainProvider::getSurfaceTextures() const noexcept {
  return impl_->getSurfaceTextures(
      platform_->getDevice(), colorSwapchain_, depthSwapchain_, swapchainImageInfo_, numViews_);
}

void XrSwapchainProvider::releaseSwapchainImages() const noexcept {
  XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  XR_CHECK(xrReleaseSwapchainImage(colorSwapchain_, &releaseInfo));
  XR_CHECK(xrReleaseSwapchainImage(depthSwapchain_, &releaseInfo));
}
} // namespace igl::shell::openxr
