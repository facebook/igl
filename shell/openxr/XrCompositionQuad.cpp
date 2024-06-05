/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/openxr/XrCompositionQuad.h>

namespace igl::shell::openxr {

XrCompositionQuad::XrCompositionQuad(impl::XrAppImpl& appImpl,
                                     std::shared_ptr<igl::shell::Platform> platform,
                                     XrSession session,
                                     bool useSinglePassStereo,
                                     bool isAlphaBlendCompositionSupported,
                                     glm::vec3 position,
                                     glm::vec2 size,
                                     LayerBlendMode blendMode) noexcept :
  XrComposition(appImpl, std::move(platform), session, useSinglePassStereo),
  position_(position),
  size_(size),
  blendMode_(blendMode),
  isAlphaBlendCompositionSupported_(isAlphaBlendCompositionSupported) {
#ifdef XR_FB_composition_layer_alpha_blend
  additiveBlending_ = {.type = XR_TYPE_COMPOSITION_LAYER_ALPHA_BLEND_FB,
                       .next = nullptr,
                       .srcFactorColor = XR_BLEND_FACTOR_ONE_MINUS_DST_ALPHA_FB,
                       .dstFactorColor = XR_BLEND_FACTOR_ONE_FB,
                       .srcFactorAlpha = XR_BLEND_FACTOR_ZERO_FB,
                       .dstFactorAlpha = XR_BLEND_FACTOR_ONE_FB};
#endif
}

void XrCompositionQuad::updatePosition(glm::vec3 position) noexcept {
  position_ = position;
}

void XrCompositionQuad::updateSize(glm::vec2 size) noexcept {
  size_ = size;
}

void XrCompositionQuad::updateBlendMode(LayerBlendMode blendMode) noexcept {
  blendMode_ = blendMode;
}

// NOLINTNEXTLINE(bugprone-exception-escape)
void XrCompositionQuad::doComposition(
    const DepthParams& /* depthParams */,
    const std::array<XrView, kNumViews>& /* views */,
    const std::array<XrPosef, kNumViews>& /* viewStagePoses */,
    XrSpace currentSpace,
    XrCompositionLayerFlags compositionFlags,
    std::vector<const XrCompositionLayerBaseHeader*>& layers) noexcept {
  compositionFlags |= (blendMode_ == igl::shell::LayerBlendMode::AlphaBlend)
                          ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT
                          : 0;

  for (uint8_t view = 0; view < kNumViews; ++view) {
    const auto subImageIndex = useSinglePassStereo_ ? static_cast<uint32_t>(view) : 0u;
    const auto swapchainProviderIndex = useSinglePassStereo_ ? 0u : static_cast<uint32_t>(view);

    const XrRect2Di imageRect = {{0, 0},
                                 {
                                     static_cast<int32_t>(swapchainImageInfo_[view].imageWidth),
                                     static_cast<int32_t>(swapchainImageInfo_[view].imageHeight),
                                 }};

    quadLayers_[view] = {
        .type = XR_TYPE_COMPOSITION_LAYER_QUAD,
#ifdef XR_FB_composition_layer_alpha_blend
        .next = (isAlphaBlendCompositionSupported_ &&
                 blendMode_ == igl::shell::LayerBlendMode::AlphaAdditive)
                    ? &additiveBlending_
                    : nullptr,
#else
        .next = nullptr,
#endif // XR_FB_composition_layer_alpha_blend
        .layerFlags = compositionFlags,
        .space = currentSpace,
        .eyeVisibility = (view == 0) ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT,
        .subImage =
            {
                swapchainProviders_[swapchainProviderIndex]->colorSwapchain(),
                imageRect,
                subImageIndex,
            },
        .pose = {.orientation = {0.f, 0.f, 0.f, 1.f},
                 .position = {position_.x, position_.y, position_.z}},
        .size = {size_.x, size_.y},
    };

    layers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quadLayers_[view]));
  }
}

} // namespace igl::shell::openxr
