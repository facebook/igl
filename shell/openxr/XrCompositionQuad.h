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
                    std::shared_ptr<Platform> platform,
                    XrSession session,
                    bool useSinglePassStereo,
                    bool isAlphaBlendCompositionSupported,
                    const QuadLayerInfo& quadLayerInfo) noexcept;

  void updateQuadLayerInfo(const QuadLayerInfo& info) noexcept;

  void doComposition(const DepthParams& depthParams,
                     const std::array<XrView, kNumViews>& views,
                     const std::array<XrPosef, kNumViews>& viewStagePoses,
                     XrSpace currentSpace,
                     XrCompositionLayerFlags compositionFlags,
                     std::vector<const XrCompositionLayerBaseHeader*>& layers) noexcept override;

 private:
#ifdef XR_FB_composition_layer_alpha_blend
  XrCompositionLayerAlphaBlendFB customBlending_{};
#endif
  std::array<XrCompositionLayerQuad, kNumViews> quadLayers_{};

  QuadLayerInfo info_;

  const bool isAlphaBlendCompositionSupported_;
};

} // namespace igl::shell::openxr
