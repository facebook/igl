/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <cstdint>
#include <igl/Core.h>

#include <shell/openxr/XrLog.h>
#include <shell/openxr/XrSwapchainProvider.h>
#include <shell/openxr/impl/XrSwapchainProviderImpl.h>

#include <algorithm>

namespace igl::shell::openxr {
namespace {
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
[[nodiscard]] int64_t findFormat(const std::vector<int64_t>& sortedSwapchainFormats,
                                 const std::vector<int64_t>& formats) noexcept {
  for (const int64_t format : formats) {
    if (std::binary_search(sortedSwapchainFormats.begin(), sortedSwapchainFormats.end(), format)) {
      return format;
    }
  }
  return impl::kSwapchainImageInvalidFormat;
}
} // namespace

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
  std::sort(swapchainFormats.begin(), swapchainFormats.end());

  swapchainImageInfo_.colorFormat =
      findFormat(swapchainFormats,
                 swapchainImageInfo_.colorFormat != impl::kSwapchainImageInvalidFormat
                     ? std::vector<int64_t>{swapchainImageInfo_.colorFormat}
                     : impl_->preferredColorFormats());
  if (swapchainImageInfo_.colorFormat == impl::kSwapchainImageInvalidFormat) {
    IGL_DEBUG_ABORT("No supported color format found");
    return false;
  }

  colorSwapchain_ =
      createXrSwapchain(XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, swapchainImageInfo_.colorFormat);

  swapchainImageInfo_.depthFormat =
      findFormat(swapchainFormats,
                 swapchainImageInfo_.depthFormat != impl::kSwapchainImageInvalidFormat
                     ? std::vector<int64_t>{swapchainImageInfo_.depthFormat}
                     : impl_->preferredDepthFormats());
  if (swapchainImageInfo_.depthFormat == impl::kSwapchainImageInvalidFormat) {
    IGL_DEBUG_ABORT("No supported depth format found");
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

  XrSwapchain swapchain = nullptr;
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
  const XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  XR_CHECK(xrReleaseSwapchainImage(colorSwapchain_, &releaseInfo));
  XR_CHECK(xrReleaseSwapchainImage(depthSwapchain_, &releaseInfo));
}
} // namespace igl::shell::openxr
