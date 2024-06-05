/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glm/glm.hpp>
#include <shell/openxr/XrComposition.h>
#include <shell/shared/renderSession/QuadLayerParams.h>

namespace igl::shell::openxr {

class XrCompositionQuad final : public XrComposition {
 public:
  XrCompositionQuad(impl::XrAppImpl& appImpl,
                    std::shared_ptr<igl::shell::Platform> platform,
                    XrSession session,
                    bool useSinglePassStereo,
                    bool isAlphaBlendCompositionSupported,
                    glm::vec3 position,
                    glm::vec2 size,
                    LayerBlendMode blendMode) noexcept;

  void updatePosition(glm::vec3 position) noexcept;
  void updateSize(glm::vec2 size) noexcept;
  void updateBlendMode(LayerBlendMode blendMode) noexcept;

  void doComposition(const DepthParams& depthParams,
                     const std::array<XrView, kNumViews>& views,
                     const std::array<XrPosef, kNumViews>& viewStagePoses,
                     XrSpace currentSpace,
                     XrCompositionLayerFlags compositionFlags,
                     std::vector<const XrCompositionLayerBaseHeader*>& layers) noexcept override;

 private:
#ifdef XR_FB_composition_layer_alpha_blend
  XrCompositionLayerAlphaBlendFB additiveBlending_{};
#endif
  std::array<XrCompositionLayerQuad, kNumViews> quadLayers_{};

  glm::vec3 position_;
  glm::vec2 size_{1.0f, 1.0f};
  LayerBlendMode blendMode_ = LayerBlendMode::Opaque;

  const bool isAlphaBlendCompositionSupported_;
};

} // namespace igl::shell::openxr
