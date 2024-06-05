/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/openxr/XrPlatform.h>
#include <shell/openxr/XrSwapchainProvider.h>
#include <shell/openxr/impl/XrSwapchainProviderImpl.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/DepthParams.h>
#include <shell/shared/renderSession/ViewParams.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace igl::shell::openxr {
namespace impl {
class XrAppImpl;
} // namespace impl

class XrComposition {
 public:
  static constexpr uint8_t kNumViews = 2; // 2 for stereo

  XrComposition(impl::XrAppImpl& appImpl,
                std::shared_ptr<igl::shell::Platform> platform,
                XrSession session,
                bool useSinglePassStereo) noexcept;
  virtual ~XrComposition() noexcept;

  void updateSwapchainImageInfo(
      std::array<impl::SwapchainImageInfo, kNumViews> swapchainImageInfo) noexcept;

  [[nodiscard]] bool isValid() noexcept;

  [[nodiscard]] uint32_t renderPassesCount() noexcept;
  [[nodiscard]] igl::SurfaceTextures beginRendering(
      uint32_t renderPassIndex,
      const std::array<XrView, kNumViews>& views,
      const std::array<glm::mat4, kNumViews>& viewTransforms,
      const std::array<glm::vec3, kNumViews>& cameraPositions,
      std::vector<ViewParams>& viewParams) noexcept;
  void endRendering(uint32_t renderPassIndex) noexcept;

  virtual void doComposition(const DepthParams& depthParams,
                             const std::array<XrView, kNumViews>& views,
                             const std::array<XrPosef, kNumViews>& viewStagePoses,
                             XrSpace currentSpace,
                             XrCompositionLayerFlags compositionFlags,
                             std::vector<const XrCompositionLayerBaseHeader*>& layers) noexcept = 0;

 protected:
  impl::XrAppImpl& appImpl_;
  std::shared_ptr<igl::shell::Platform> platform_;
  // NOLINTNEXTLINE(misc-misplaced-const)
  const XrSession session_;

  std::array<impl::SwapchainImageInfo, kNumViews> swapchainImageInfo_;

  // If useSinglePassStereo_ is true, only one XrSwapchainProvider is used (with index 0).
  std::array<std::unique_ptr<XrSwapchainProvider>, kNumViews> swapchainProviders_{};

  const bool useSinglePassStereo_;
};

} // namespace igl::shell::openxr
