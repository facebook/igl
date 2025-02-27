/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/openxr/XrComposition.h>

namespace igl::shell::openxr {

class XrCompositionProjection final : public XrComposition {
 public:
  XrCompositionProjection(impl::XrAppImpl& appImpl,
                          std::shared_ptr<Platform> platform,
                          XrSession session,
                          bool useSinglePassStereo) noexcept;

  void doComposition(const DepthParams& depthParams,
                     const std::array<XrView, kNumViews>& views,
                     const std::array<XrPosef, kNumViews>& viewStagePoses,
                     XrSpace currentSpace,
                     XrCompositionLayerFlags compositionFlags,
                     std::vector<const XrCompositionLayerBaseHeader*>& layers) noexcept override;

 private:
  std::array<XrCompositionLayerProjectionView, kNumViews> projectionViews_{};
  std::array<XrCompositionLayerDepthInfoKHR, kNumViews> depthInfos_{};
  XrCompositionLayerProjection projectionLayer_{};
};

} // namespace igl::shell::openxr
