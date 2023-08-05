/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <vector>

#include "XrSwapchainProvider.h"
#include <shell/openxr/XrLog.h>
#include <shell/openxr/impl/XrSwapchainProviderImpl.h>

namespace igl::shell::openxr {
XrSwapchainProvider::XrSwapchainProvider(std::unique_ptr<impl::XrSwapchainProviderImpl>&& impl,
                                         const std::shared_ptr<igl::shell::Platform>& platform,
                                         const XrSession& session,
                                         const XrViewConfigurationView& viewport,
                                         uint32_t numViews) :
  impl_(std::move(impl)),
  platform_(platform),
  session_(session),
  viewport_(viewport),
  numViews_(numViews) {}
XrSwapchainProvider::~XrSwapchainProvider() {
  xrDestroySwapchain(colorSwapchain_);
  xrDestroySwapchain(depthSwapchain_);
}

bool XrSwapchainProvider::initialize() {
  uint32_t numSwapchainFormats = 0;
  XR_CHECK(xrEnumerateSwapchainFormats(session_, 0, &numSwapchainFormats, nullptr));

  std::vector<int64_t> swapchainFormats(numSwapchainFormats);
  XR_CHECK(xrEnumerateSwapchainFormats(
      session_, numSwapchainFormats, &numSwapchainFormats, swapchainFormats.data()));

  auto colorFormat = impl_->preferredColorFormat();
  if (std::any_of(std::begin(swapchainFormats),
                  std::end(swapchainFormats),
                  [&](const auto& format) { return format == colorFormat; })) {
    selectedColorFormat_ = colorFormat;
  }
#if defined(__APPLE__)
  selectedColorFormat_ = impl_->preferredColorFormat();
#endif

  colorSwapchain_ =
      createXrSwapchain(XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, selectedColorFormat_);

  auto depthFormat = impl_->preferredDepthFormat();
  if (std::any_of(std::begin(swapchainFormats),
                  std::end(swapchainFormats),
                  [&](const auto& format) { return format == depthFormat; })) {
    selectedDepthFormat_ = depthFormat;
  }
#if defined(__APPLE__)
  selectedDepthFormat_ = impl_->preferredDepthFormat();
#endif

  depthSwapchain_ =
      createXrSwapchain(XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, selectedDepthFormat_);

  impl_->enumerateImages(platform_->getDevice(),
                         colorSwapchain_,
                         depthSwapchain_,
                         selectedColorFormat_,
                         selectedDepthFormat_,
                         viewport_,
                         numViews_);

  return true;
}

XrSwapchain XrSwapchainProvider::createXrSwapchain(XrSwapchainUsageFlags extraUsageFlags,
                                                   int64_t format) {
  XrSwapchainCreateInfo swapChainCreateInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO, nullptr};
  swapChainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | extraUsageFlags;
  swapChainCreateInfo.format = format;
  swapChainCreateInfo.sampleCount = 1;
  swapChainCreateInfo.width = viewport_.recommendedImageRectWidth;
  swapChainCreateInfo.height = viewport_.recommendedImageRectHeight;
  swapChainCreateInfo.faceCount = 1;
  swapChainCreateInfo.arraySize = numViews_;
  swapChainCreateInfo.mipCount = 1;

  XrSwapchain swapchain;
  XR_CHECK(xrCreateSwapchain(session_, &swapChainCreateInfo, &swapchain));
  IGL_LOG_INFO("XrSwapchain created");
  IGL_LOG_INFO("XrSwapchain viewport width: %d", viewport_.recommendedImageRectWidth);
  IGL_LOG_INFO("XrSwapchain viewport height: %d", viewport_.recommendedImageRectHeight);

  return swapchain;
}

igl::SurfaceTextures XrSwapchainProvider::getSurfaceTextures() const {
  return impl_->getSurfaceTextures(platform_->getDevice(),
                                   colorSwapchain_,
                                   depthSwapchain_,
                                   selectedColorFormat_,
                                   selectedDepthFormat_,
                                   viewport_,
                                   numViews_);
}
void XrSwapchainProvider::releaseSwapchainImages() const {
  XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
  XR_CHECK(xrReleaseSwapchainImage(colorSwapchain_, &releaseInfo));
  XR_CHECK(xrReleaseSwapchainImage(depthSwapchain_, &releaseInfo));
}
} // namespace igl::shell::openxr
