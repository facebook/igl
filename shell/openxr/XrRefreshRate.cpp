/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/openxr/XrRefreshRate.h>

#include <shell/openxr/XrLog.h>

#include <algorithm>

namespace igl::shell::openxr {

XrRefreshRate::XrRefreshRate(XrInstance instance, XrSession session) noexcept : session_(session) {
  XR_CHECK(xrGetInstanceProcAddr(
      instance, "xrGetDisplayRefreshRateFB", (PFN_xrVoidFunction*)(&xrGetDisplayRefreshRateFB_)));
  IGL_DEBUG_ASSERT(xrGetDisplayRefreshRateFB_ != nullptr);
  XR_CHECK(xrGetInstanceProcAddr(instance,
                                 "xrEnumerateDisplayRefreshRatesFB",
                                 (PFN_xrVoidFunction*)(&xrEnumerateDisplayRefreshRatesFB_)));
  IGL_DEBUG_ASSERT(xrEnumerateDisplayRefreshRatesFB_ != nullptr);
  XR_CHECK(xrGetInstanceProcAddr(instance,
                                 "xrRequestDisplayRefreshRateFB",
                                 (PFN_xrVoidFunction*)(&xrRequestDisplayRefreshRateFB_)));
  IGL_DEBUG_ASSERT(xrRequestDisplayRefreshRateFB_ != nullptr);
}

XrRefreshRate::~XrRefreshRate() noexcept = default;

bool XrRefreshRate::initialize(const Params& params) noexcept {
  queryCurrentRefreshRate();
  if (params.refreshRateMode_ == RefreshRateMode::UseMaxRefreshRate) {
    setMaxRefreshRate();
  } else if (params.refreshRateMode_ == RefreshRateMode::UseSpecificRefreshRate) {
    setRefreshRate(params.desiredSpecificRefreshRate_);
  } else {
    // Do nothing. Use default refresh rate.
  }

  querySupportedRefreshRates();
  return true;
}

// NOLINTNEXTLINE(bugprone-exception-escape)
const std::vector<const char*>& XrRefreshRate::getExtensions() noexcept {
  static const std::vector<const char*> kExtensions{XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME};
  return kExtensions;
}

float XrRefreshRate::getCurrentRefreshRate() const noexcept {
  return currentRefreshRate_;
}

float XrRefreshRate::getMaxRefreshRate() const noexcept {
  const std::vector<float>& supportedRefreshRates = getSupportedRefreshRates();
  if (supportedRefreshRates.empty()) {
    return 0.0f;
  }

  const float maxRefreshRate = supportedRefreshRates.back();
  return maxRefreshRate;
}

bool XrRefreshRate::setRefreshRate(float refreshRate) noexcept {
  if ((refreshRate == currentRefreshRate_) || !isRefreshRateSupported(refreshRate)) {
    return false;
  }

  const XrResult result = xrRequestDisplayRefreshRateFB_(session_, refreshRate);
  if (result != XR_SUCCESS) {
    return false;
  }

  IGL_LOG_INFO(
      "setRefreshRate changed from %.2f Hz to %.2f Hz\n", currentRefreshRate_, refreshRate);
  currentRefreshRate_ = refreshRate;

  return true;
}

void XrRefreshRate::setMaxRefreshRate() noexcept {
  const float maxRefreshRate = getMaxRefreshRate();
  IGL_LOG_INFO("maxRefreshRate = %.2f Hz\n", maxRefreshRate);
  if (maxRefreshRate > 0.0f) {
    setRefreshRate(maxRefreshRate);
  }
}

bool XrRefreshRate::isRefreshRateSupported(float refreshRate) const noexcept {
  const std::vector<float>& supportedRefreshRates = getSupportedRefreshRates();
  return std::find(supportedRefreshRates.begin(), supportedRefreshRates.end(), refreshRate) !=
         supportedRefreshRates.end();
}

const std::vector<float>& XrRefreshRate::getSupportedRefreshRates() const noexcept {
  return supportedRefreshRates_;
}

void XrRefreshRate::queryCurrentRefreshRate() noexcept {
  const XrResult result = xrGetDisplayRefreshRateFB_(session_, &currentRefreshRate_);
  if (result == XR_SUCCESS) {
    IGL_LOG_INFO("getCurrentRefreshRate success, current Hz = %.2f.\n", currentRefreshRate_);
  }
}

// NOLINTNEXTLINE(bugprone-exception-escape)
void XrRefreshRate::querySupportedRefreshRates() noexcept {
  if (!supportedRefreshRates_.empty()) {
    return;
  }

  uint32_t numRefreshRates = 0;
  XrResult result = xrEnumerateDisplayRefreshRatesFB_(session_, 0, &numRefreshRates, nullptr);

  if ((result == XR_SUCCESS) && (numRefreshRates > 0)) {
    supportedRefreshRates_.resize(numRefreshRates);
    result = xrEnumerateDisplayRefreshRatesFB_(
        session_, numRefreshRates, &numRefreshRates, supportedRefreshRates_.data());

    if (result == XR_SUCCESS) {
      std::sort(supportedRefreshRates_.begin(), supportedRefreshRates_.end());
    }

    for (const float refreshRate : supportedRefreshRates_) {
      (void)refreshRate;
      IGL_LOG_INFO("querySupportedRefreshRates Hz = %.2f.\n", refreshRate);
    }
  }
}

} // namespace igl::shell::openxr
