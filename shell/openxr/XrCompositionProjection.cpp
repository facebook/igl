/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/openxr/XrCompositionProjection.h>

namespace igl::shell::openxr {

XrCompositionProjection::XrCompositionProjection(impl::XrAppImpl& appImpl,
                                                 std::shared_ptr<igl::shell::Platform> platform,
                                                 XrSession session,
                                                 bool useSinglePassStereo) noexcept :
  XrComposition(appImpl, std::move(platform), session, useSinglePassStereo) {}

// NOLINTNEXTLINE(bugprone-exception-escape)
void XrCompositionProjection::doComposition(
    const DepthParams& depthParams,
    const std::array<XrView, kNumViews>& views,
    const std::array<XrPosef, kNumViews>& viewStagePoses,
    XrSpace currentSpace,
    XrCompositionLayerFlags compositionFlags,
    std::vector<const XrCompositionLayerBaseHeader*>& layers) noexcept {
  for (uint8_t view = 0; view < kNumViews; ++view) {
    const auto subImageIndex = useSinglePassStereo_ ? static_cast<uint32_t>(view) : 0u;
    const auto swapchainProviderIndex = useSinglePassStereo_ ? 0u : static_cast<uint32_t>(view);

    const XrRect2Di imageRect = {{0, 0},
                                 {
                                     static_cast<int32_t>(swapchainImageInfo_[view].imageWidth),
                                     static_cast<int32_t>(swapchainImageInfo_[view].imageHeight),
                                 }};

    depthInfos_[view] = {.type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR,
                         .next = nullptr,
                         .subImage =
                             {
                                 swapchainProviders_[swapchainProviderIndex]->depthSwapchain(),
                                 imageRect,
                                 subImageIndex,
                             },
                         .minDepth = depthParams.minDepth,
                         .maxDepth = depthParams.maxDepth,
                         .nearZ = depthParams.nearZ,
                         .farZ = depthParams.farZ};

    projectionViews_[view] = {.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
                              .next = &depthInfos_[view],
                              .pose = viewStagePoses[view],
                              .fov = views[view].fov,
                              .subImage = {
                                  swapchainProviders_[swapchainProviderIndex]->colorSwapchain(),
                                  imageRect,
                                  subImageIndex,
                              }};
  }

  projectionLayer_ = {
      .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
      .next = nullptr,
      .layerFlags = compositionFlags,
      .space = currentSpace,
      .viewCount = static_cast<uint32_t>(kNumViews),
      .views = projectionViews_.data(),
  };

  layers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer_));
}

} // namespace igl::shell::openxr
