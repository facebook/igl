/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/openxr/XrComposition.h>

#include <shell/openxr/impl/XrAppImpl.h>

namespace igl::shell::openxr {
namespace {
inline void copyFov(igl::shell::Fov& dst, const XrFovf& src) {
  dst.angleLeft = src.angleLeft;
  dst.angleRight = src.angleRight;
  dst.angleUp = src.angleUp;
  dst.angleDown = src.angleDown;
}
} // namespace

XrComposition::XrComposition(impl::XrAppImpl& appImpl,
                             std::shared_ptr<igl::shell::Platform> platform,
                             XrSession session,
                             bool useSinglePassStereo) noexcept :
  appImpl_(appImpl),
  platform_(std::move(platform)),
  session_(session),
  useSinglePassStereo_(useSinglePassStereo) {}

XrComposition::~XrComposition() noexcept = default;

void XrComposition::updateSwapchainImageInfo(
    std::array<impl::SwapchainImageInfo, kNumViews> swapchainImageInfo) noexcept {
  if (swapchainImageInfo == swapchainImageInfo_) {
    return;
  }
  swapchainImageInfo_ = swapchainImageInfo;

  if (useSinglePassStereo_ && (swapchainImageInfo_[0] != swapchainImageInfo_[1])) {
    IGL_LOG_ERROR("Single pass stereo requires identical swapchain image info.\n");
    swapchainProviders_ = {};
    return;
  }

  for (uint32_t i = 0; i < renderPassesCount(); ++i) {
    swapchainProviders_[i] =
        std::make_unique<XrSwapchainProvider>(appImpl_.createSwapchainProviderImpl(),
                                              platform_,
                                              session_,
                                              swapchainImageInfo_[i],
                                              useSinglePassStereo_ ? kNumViews : 1u);
    if (!swapchainProviders_[i]->initialize()) {
      swapchainProviders_ = {};
      return;
    }
  }
}

[[nodiscard]] bool XrComposition::isValid() noexcept {
  // Need to check only the first swapchain provider.
  return swapchainProviders_[0] != nullptr;
}

uint32_t XrComposition::renderPassesCount() noexcept {
  return useSinglePassStereo_ ? 1u : kNumViews;
}

igl::SurfaceTextures XrComposition::beginRendering(
    uint32_t renderPassIndex,
    const std::array<XrView, kNumViews>& views,
    const std::array<glm::mat4, kNumViews>& viewTransforms,
    const std::array<glm::vec3, kNumViews>& cameraPositions,
    std::vector<ViewParams>& viewParams) noexcept {
  if (useSinglePassStereo_) {
    IGL_DEBUG_ASSERT(viewParams.size() == kNumViews);
    for (uint8_t i = 0; i < kNumViews; ++i) {
      viewParams[i].viewMatrix = viewTransforms[i];
      viewParams[i].cameraPosition = cameraPositions[i];
      viewParams[i].viewIndex = i;
      copyFov(viewParams[i].fov, views[i].fov);
    }
  } else {
    IGL_DEBUG_ASSERT(viewParams.size() == 1);
    viewParams[0].viewMatrix = viewTransforms[renderPassIndex];
    viewParams[0].cameraPosition = cameraPositions[renderPassIndex];
    viewParams[0].viewIndex = static_cast<uint8_t>(renderPassIndex);
    copyFov(viewParams[0].fov, views[renderPassIndex].fov);
  }
  return swapchainProviders_[renderPassIndex]->getSurfaceTextures();
}

void XrComposition::endRendering(uint32_t renderPassIndex) noexcept {
  swapchainProviders_[renderPassIndex]->releaseSwapchainImages();
}

} // namespace igl::shell::openxr
