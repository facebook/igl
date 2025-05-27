/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/openxr/XrPlatform.h>

#include <cstdint>
#include <vector>

namespace igl::shell::openxr {

class XrRefreshRate final {
 public:
  enum RefreshRateMode : uint8_t {
    kUseDefault = 0,
    kUseMaxRefreshRate,
    kUseSpecificRefreshRate,
  };
  struct Params {
    RefreshRateMode refreshRateMode = RefreshRateMode::kUseDefault;
    float desiredSpecificRefreshRate = 90.0f;
  };

  XrRefreshRate(XrInstance instance, XrSession session) noexcept;
  ~XrRefreshRate() noexcept;

  [[nodiscard]] bool initialize(const Params& params) noexcept;

  [[nodiscard]] static const std::vector<const char*>& getExtensions() noexcept;

  [[nodiscard]] float getCurrentRefreshRate() const noexcept;
  [[nodiscard]] float getMaxRefreshRate() const noexcept;

  bool setRefreshRate(float refreshRate) noexcept;
  void setMaxRefreshRate() noexcept;

  [[nodiscard]] bool isRefreshRateSupported(float refreshRate) const noexcept;
  [[nodiscard]] const std::vector<float>& getSupportedRefreshRates() const noexcept;

 private:
  void queryCurrentRefreshRate() noexcept;
  void querySupportedRefreshRates() noexcept;

  // NOLINTNEXTLINE(misc-misplaced-const)
  const XrSession session_;

  // API
  PFN_xrGetDisplayRefreshRateFB xrGetDisplayRefreshRateFB_ = nullptr;
  PFN_xrEnumerateDisplayRefreshRatesFB xrEnumerateDisplayRefreshRatesFB_ = nullptr;
  PFN_xrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB_ = nullptr;

  std::vector<float> supportedRefreshRates_;
  float currentRefreshRate_ = 0.0f;
};

} // namespace igl::shell::openxr
