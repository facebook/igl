/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>
#include <memory>
#include <shell/shared/platform/PresentationRateController.h>

namespace igl::shell {

/// The two rates a display with switchable modes has.
///
/// A display link has only one — it ticks at whatever the panel is doing and cannot change
/// it — which is why TickSourceRateBackend's single MaxRateProvider cannot describe this
/// kind of display, and why this is a separate backend rather than another provider.
struct DisplayRates {
  /// What the panel is refreshing at right now. Every cadence this backend can hold is a
  /// whole division of this, so it is the number a divisor must be derived from. Zero when
  /// the platform will not say, which leaves the backend refusing everything.
  float currentHz = 0.0f;
  /// The highest mode the panel can be switched into, reported as the capability ceiling so
  /// a caller can express a request for it. Switching modes belongs to the compositor, so
  /// reaching this is not something this backend does or promises. Zero when unknown, and
  /// then `currentHz` stands in.
  float maxHz = 0.0f;
};

/// Read on every request rather than cached. A mode switch changes `currentHz` underneath
/// us, and a divisor derived from a stale reading paces at the wrong rate — a 120 Hz panel
/// that dropped to 60 while a divisor of 2 was in force presents at 30, not the 60 the
/// caller was told it was granted.
using DisplayRatesProvider = std::function<DisplayRates()>;

/// The one lever that actually holds a cadence: present one frame every N refreshes of the
/// mode the display is currently in.
struct FrameDivider {
  /// Applies a divisor in [1, maxDivisor] — eglSwapInterval() on GLES, a callback-skipping
  /// pacer on a loop driven by a vsync callback. Null when the platform has no such lever,
  /// and then every request is refused rather than answered with something unverifiable.
  std::function<Result(int refreshesPerFrame)> setDivisor;
  /// The largest divisor the lever will honor: EGL_MAX_SWAP_INTERVAL for a swap interval,
  /// the counting limit for a pacer. A rate needing more is refused rather than clamped,
  /// because a clamped divisor is a cadence that silently disagrees with the granted rate.
  int maxDivisor = 1;
};

/// Presentation-rate backend for a display whose rate is held by dividing its current mode.
///
/// Every grant it records is one it can hold by construction. The divisor is a whole number
/// of refreshes of a rate the platform reported during that same call, the lever that
/// applies it is the only thing the request touches, and the granted rate is computed from
/// the same reading the divisor was — so the cadence being held and the rate the caller is
/// told about cannot drift apart. Nothing here treats an unconfirmed hint as a grant.
///
/// A rate above the current mode needs a mode switch, and dividing cannot multiply. Such a
/// request is reported, never granted: granting the current mode under a higher rate's name
/// is exactly the silent failure the seam exists to prevent.
std::unique_ptr<PresentationRateController::Backend> createFrameDividerRateBackend(
    DisplayRatesProvider displayRates,
    FrameDivider divider);

} // namespace igl::shell
