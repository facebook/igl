/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/android/PresentationRateAndroid.h>

#include <cmath>
#include <cstdint>
#include <dlfcn.h>

namespace igl::shell {

namespace {

using SetFrameRateFn = int32_t (*)(ANativeWindow*, float, int8_t);

/// ANativeWindow_setFrameRate(), or null on a device older than Android 11.
///
/// Resolved once and cached: dlsym() walks the whole linker namespace. A null answer is
/// cached too — it is the permanent state of a device whose libandroid never exported the
/// symbol.
SetFrameRateFn setFrameRateFn() {
  static const auto kFn =
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) dlsym() returns void*.
      reinterpret_cast<SetFrameRateFn>(dlsym(RTLD_DEFAULT, "ANativeWindow_setFrameRate"));
  return kFn;
}

/// ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_FIXED_SOURCE. Spelled out rather than included,
/// because the NDK declares the constant alongside the function it belongs to, above the
/// shell's minSdkVersion. It tells SurfaceFlinger the rate is the source's own cadence
/// rather than a soft preference, which is what lets it pick a mode the rate divides into
/// evenly instead of one that is merely close.
constexpr int8_t kFrameRateCompatibilityFixedSource = 1;

} // namespace

Result setNativeWindowFrameRate(ANativeWindow* window, float hz) {
  if (window == nullptr) {
    return Result{Result::Code::InvalidOperation,
                  "This platform has no native window to set a frame rate on."};
  }
  // Negated so a NaN lands here too. Zero is legal and asks SurfaceFlinger to choose.
  if (!std::isfinite(hz) || !(hz >= 0.0f)) {
    return Result{Result::Code::ArgumentOutOfRange,
                  "A native-window frame rate must be a finite rate at or above zero."};
  }
  const SetFrameRateFn setFrameRate = setFrameRateFn();
  if (setFrameRate == nullptr) {
    return Result{Result::Code::Unsupported,
                  "This device predates Android 11, where a window first gained a settable "
                  "frame rate."};
  }
  if (setFrameRate(window, hz, kFrameRateCompatibilityFixedSource) != 0) {
    return Result{Result::Code::RuntimeError,
                  "The compositor would not take this window's frame rate."};
  }
  return Result{};
}

} // namespace igl::shell
