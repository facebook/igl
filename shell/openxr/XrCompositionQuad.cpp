/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/openxr/XrCompositionQuad.h>

#include <igl/RenderPipelineState.h>

namespace igl::shell::openxr {
namespace {
#ifdef XR_FB_composition_layer_alpha_blend
inline XrBlendFactorFB iglToOpenXR(igl::BlendFactor factor) noexcept {
  switch (factor) {
  case igl::BlendFactor::Zero:
    return XR_BLEND_FACTOR_ZERO_FB;
  case igl::BlendFactor::One:
    return XR_BLEND_FACTOR_ONE_FB;
  case igl::BlendFactor::SrcAlpha:
    return XR_BLEND_FACTOR_SRC_ALPHA_FB;
  case igl::BlendFactor::OneMinusSrcAlpha:
    return XR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA_FB;
  case igl::BlendFactor::DstAlpha:
    return XR_BLEND_FACTOR_DST_ALPHA_FB;
  case igl::BlendFactor::OneMinusDstAlpha:
    return XR_BLEND_FACTOR_ONE_MINUS_DST_ALPHA_FB;
  default:
    IGL_ASSERT_MSG(false, "Not supported blend factor (%d)", static_cast<int>(factor));
    break;
  }
  return XR_BLEND_FACTOR_ZERO_FB;
}
#endif // XR_FB_composition_layer_alpha_blend
} // namespace

XrCompositionQuad::XrCompositionQuad(impl::XrAppImpl& appImpl,
                                     std::shared_ptr<igl::shell::Platform> platform,
                                     XrSession session,
                                     bool useSinglePassStereo,
                                     bool isAlphaBlendCompositionSupported,
                                     const QuadLayerInfo& info) noexcept :
  XrComposition(appImpl, std::move(platform), session, useSinglePassStereo),
  isAlphaBlendCompositionSupported_(isAlphaBlendCompositionSupported) {
  updateQuadLayerInfo(info);
}

void XrCompositionQuad::updateQuadLayerInfo(const QuadLayerInfo& info) noexcept {
  info_ = info;

#ifdef XR_FB_composition_layer_alpha_blend
  customBlending_ = {.type = XR_TYPE_COMPOSITION_LAYER_ALPHA_BLEND_FB,
                     .next = nullptr,
                     .srcFactorColor = iglToOpenXR(info.customSrcRGBBlendFactor),
                     .dstFactorColor = iglToOpenXR(info.customDstRGBBlendFactor),
                     .srcFactorAlpha = iglToOpenXR(info.customSrcAlphaBlendFactor),
                     .dstFactorAlpha = iglToOpenXR(info.customDstAlphaBlendFactor)};
#endif
}

// NOLINTNEXTLINE(bugprone-exception-escape)
void XrCompositionQuad::doComposition(
    const DepthParams& /* depthParams */,
    const std::array<XrView, kNumViews>& /* views */,
    const std::array<XrPosef, kNumViews>& /* viewStagePoses */,
    XrSpace currentSpace,
    XrCompositionLayerFlags compositionFlags,
    std::vector<const XrCompositionLayerBaseHeader*>& layers) noexcept {
  compositionFlags |= (info_.blendMode == igl::shell::LayerBlendMode::AlphaBlend)
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
                 info_.blendMode == igl::shell::LayerBlendMode::Custom)
                    ? &customBlending_
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
                 .position = {info_.position.x, info_.position.y, info_.position.z}},
        .size = {info_.size.x, info_.size.y},
    };

    layers.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quadLayers_[view]));
  }
}

} // namespace igl::shell::openxr
