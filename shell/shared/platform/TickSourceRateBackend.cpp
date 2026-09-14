/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/TickSourceRateBackend.h>

#include <cmath>
#include <utility>

namespace igl::shell {

TickSourceRateBackend::TickSourceRateBackend(MaxRateProvider maxRateProvider,
                                             RateApplier rateApplier) :
  maxRateProvider_(std::move(maxRateProvider)), rateApplier_(std::move(rateApplier)) {
  IGL_DEBUG_ASSERT(maxRateProvider_,
                   "A tick-source backend needs a way to ask what the display can do.");
  IGL_DEBUG_ASSERT(rateApplier_, "A tick-source backend needs a way to set the rate.");
}

// The only throwing path clang-tidy can see is calling an empty std::function, which the
// guard below rules out; the providers themselves are platform screen queries, and IGL
// builds with exceptions disabled besides.
// NOLINTNEXTLINE(bugprone-exception-escape)
float TickSourceRateBackend::queryMaxRateHz() const noexcept {
  if (!maxRateProvider_) {
    return 0.0f;
  }
  const float maxRateHz = maxRateProvider_();
  // Negated rather than `<= 0.0f` so a NaN answer is caught too.
  if (!std::isfinite(maxRateHz) || !(maxRateHz > 0.0f)) {
    return 0.0f;
  }
  return maxRateHz;
}

PresentationRateCapabilities TickSourceRateBackend::getCapabilities() const noexcept {
  const float maxRateHz = queryMaxRateHz();
  return PresentationRateCapabilities{
      .maxRefreshRateHz = maxRateHz,
      // A tick source caps by skipping refreshes rather than by slowing the panel, so
      // there is no floor to report — it will fire once a second if asked. Zero leaves
      // the seam's lower-bound check switched off rather than inventing a limit.
      .minRefreshRateHz = 0.0f,
      .canSetFixedRate = maxRateHz > 0.0f,
  };
}

Result TickSourceRateBackend::apply(const PresentationRateRequest& request, float& outGrantedHz) {
  const float maxRateHz = queryMaxRateHz();
  if (maxRateHz == 0.0f) {
    return Result{Result::Code::Unsupported,
                  "This platform did not report a refresh rate, so there is no tick "
                  "cadence to pace against."};
  }

  const float requestedHz = request.mode == PresentationRateMode::DisplayMaximum ? maxRateHz
                                                                                 : request.hz;
  const float cadenceHz = snapToTickCadence(requestedHz, maxRateHz);
  if (cadenceHz == 0.0f) {
    return Result{Result::Code::ArgumentOutOfRange,
                  "The requested rate does not resolve to a cadence this tick source can "
                  "be driven at."};
  }

  if (!rateApplier_) {
    return Result{Result::Code::InvalidOperation,
                  "This presentation-rate backend has no way to reach its tick source."};
  }
  // The same `maxRateHz` the cadence was snapped against, so the leg cannot resolve the
  // request against a different one.
  // Not const: it is returned below, and const would block the move out.
  Result applied = rateApplier_(cadenceHz, maxRateHz);
  if (!applied.isOk()) {
    return applied;
  }
  outGrantedHz = cadenceHz;
  return Result{};
}

float TickSourceRateBackend::snapToTickCadence(float requestedHz, float maxRateHz) noexcept {
  // Negated comparisons so a NaN on either side lands here instead of falling through.
  if (!std::isfinite(requestedHz) || !std::isfinite(maxRateHz) || !(requestedHz > 0.0f) ||
      !(maxRateHz > 0.0f)) {
    return 0.0f;
  }
  if (requestedHz >= maxRateHz) {
    return maxRateHz;
  }
  // At least 1, because the branch above took every rate at or over the refresh rate.
  const float refreshesPerFrame = std::round(maxRateHz / requestedHz);
  if (!std::isfinite(refreshesPerFrame)) {
    // The request sits so far below the refresh rate that the number of refreshes to skip
    // overflowed. No cadence reaches it, so refuse rather than round to something absurd.
    return 0.0f;
  }
  return maxRateHz / refreshesPerFrame;
}

int TickSourceRateBackend::refreshesPerFrame(float hz,
                                             float maxRateHz,
                                             int maxRefreshesPerFrame) noexcept {
  // Negated comparisons so a NaN on either side lands here instead of falling through.
  if (!std::isfinite(hz) || !(hz > 0.0f) || !std::isfinite(maxRateHz) || !(maxRateHz > 0.0f) ||
      maxRefreshesPerFrame < 1) {
    return 0;
  }
  const float ratio = maxRateHz / hz;
  if (!std::isfinite(ratio)) {
    // The rate sits so far below the refresh rate that the ratio overflowed. No divisor
    // reaches it, so refuse rather than round to something absurd.
    return 0;
  }
  // Rounded before the range check, not after. The divisor is a whole number of refreshes,
  // so the cap is a bound on that whole number — comparing the unrounded ratio instead
  // rejects a legal boundary divisor whenever float division overshoots it, and it
  // routinely does: 23.976 / (23.976 / 11) is 11.00000095, which fails a cap of 11 for no
  // reason a caller could act on.
  const float rounded = std::round(ratio);
  // At least one refresh per frame. A cadence above the display's own rate rounds to zero
  // here, and no tick source presents twice in one refresh.
  if (rounded < 1.0f) {
    return 1;
  }
  // Widened to double for the comparison, never float. `maxRefreshesPerFrame` is an int and
  // float carries 24 bits of mantissa, so casting a cap above 2^24 to float lands on a
  // nearby representable value instead of the cap: 16777219 becomes 16777220, letting a
  // divisor one past the cap through, and INT_MAX becomes 2147483648, one past the largest
  // int, which would make the conversion below undefined. Every int is exactly
  // representable in double, so widening removes both.
  if (static_cast<double>(rounded) > static_cast<double>(maxRefreshesPerFrame)) {
    return 0;
  }
  // Safe: `rounded` is a whole number in [1, maxRefreshesPerFrame], and that bound is an
  // int, so the conversion cannot overflow. std::lround() is deliberately not used — it is
  // undefined for a value beyond `long`, which is exactly the case the check above rules
  // out only once the value is known to be small.
  return static_cast<int>(rounded);
}

} // namespace igl::shell
