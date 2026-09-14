/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/FrameDividerRateBackend.h>

#include <cmath>
#include <memory>
#include <utility>
#include <shell/shared/platform/TickSourceRateBackend.h>

namespace igl::shell {

namespace {

/// How far above the current mode a target may sit and still count as the same mode.
///
/// One mode gets reported two ways: Android's supported-modes list gives a nominal 60.0
/// while the live query for the very same mode gives 59.94, a tenth of a percent apart.
/// Read literally, DisplayMaximum would then be permanently above the mode already running
/// and refused forever on a panel that is in fact at its best. Half a percent absorbs that
/// while staying far below the smallest real gap between modes — 48 to 50 is four percent,
/// and 60 to 90 is fifty.
constexpr float kSameModeTolerance = 1.005f;

class FrameDividerRateBackend final : public PresentationRateController::Backend {
 public:
  FrameDividerRateBackend(DisplayRatesProvider displayRates, FrameDivider divider) :
    displayRates_(std::move(displayRates)), divider_(std::move(divider)) {
    IGL_DEBUG_ASSERT(displayRates_, "A frame-divider backend needs a way to read the display.");
  }

  [[nodiscard]] PresentationRateCapabilities getCapabilities() const noexcept override {
    const DisplayRates rates = queryRates();
    return PresentationRateCapabilities{
        // The panel's best mode rather than the one it happens to be in, so a request for
        // the higher one stays expressible and reaches the compositor instead of being
        // refused by the seam's own range check before any backend sees it.
        .maxRefreshRateHz = rates.maxHz,
        // Capping happens by skipping refreshes, so there is no floor to report.
        .minRefreshRateHz = 0.0f,
        .canSetFixedRate = static_cast<bool>(divider_.setDivisor) && rates.currentHz > 0.0f,
    };
  }

  Result apply(const PresentationRateRequest& request, float& outGrantedHz) override {
    const DisplayRates rates = queryRates();
    if (rates.currentHz == 0.0f) {
      return Result{Result::Code::Unsupported,
                    "This platform did not report the rate its display is running at, so "
                    "there is no cadence to divide."};
    }
    const float targetHz = request.mode == PresentationRateMode::DisplayMaximum
                               ? (rates.maxHz > 0.0f ? rates.maxHz : rates.currentHz)
                               : request.hz;

    if (targetHz > rates.currentHz * kSameModeTolerance) {
      // Dividing cannot multiply, and no lever here switches modes. Reported rather than
      // granted: the compositor has been asked to run the panel at its best mode, and if it
      // does, this request is re-applied against the new rate when the backend is
      // reinstalled, and granted then.
      return Result{Result::Code::Unsupported,
                    "That rate is above the mode the display is currently running in, and "
                    "only the compositor can switch modes."};
    }

    // Without a divider the only cadence reachable is the display's own, so one refresh per
    // frame is the whole range. Asking for more than that then comes back as a refusal
    // below rather than as a call into a lever that is not there.
    const int maxDivisor = divider_.setDivisor ? divider_.maxDivisor : 1;
    const int refreshesPerFrame =
        TickSourceRateBackend::refreshesPerFrame(targetHz, rates.currentHz, maxDivisor);
    if (refreshesPerFrame == 0) {
      return Result{Result::Code::ArgumentOutOfRange,
                    "That rate needs more display refreshes between frames than this "
                    "platform's frame divider will wait for."};
    }

    if (divider_.setDivisor) {
      // Set even when the divisor is one: that is what undoes a smaller rung that was in
      // force a moment ago. Skipping it would leave the old division running under the new
      // rung's granted rate.
      //
      // The divider is the only thing this request touches, so a failure here leaves the
      // display exactly as it was — which is what the controller assumes when it keeps an
      // earlier grant across a refusal.
      Result applied = divider_.setDivisor(refreshesPerFrame);
      if (!applied.isOk()) {
        return applied;
      }
    }

    // Derived from the same reading the divisor was, so the cadence being held and the rate
    // reported back cannot disagree.
    outGrantedHz = rates.currentHz / static_cast<float>(refreshesPerFrame);
    return Result{};
  }

 private:
  /// The provider's answer, with anything that is not a finite rate above zero folded to
  /// zero so a single check covers "cannot say" and "said something unusable".
  // The only throwing path clang-tidy can see is calling an empty std::function, which the
  // guard below rules out; the provider itself is a platform display query, and IGL builds
  // with exceptions disabled besides.
  // NOLINTNEXTLINE(bugprone-exception-escape)
  [[nodiscard]] DisplayRates queryRates() const noexcept {
    if (!displayRates_) {
      return {};
    }
    const DisplayRates reported = displayRates_();
    DisplayRates usable;
    // Negated rather than `<= 0.0f` so a NaN is caught too.
    if (std::isfinite(reported.currentHz) && reported.currentHz > 0.0f) {
      usable.currentHz = reported.currentHz;
    }
    if (std::isfinite(reported.maxHz) && reported.maxHz > 0.0f) {
      usable.maxHz = reported.maxHz;
    }
    // A maximum below the mode the panel is already in describes no display that exists;
    // trusting it would cap DisplayMaximum below what is running right now.
    if (usable.maxHz < usable.currentHz) {
      usable.maxHz = usable.currentHz;
    }
    return usable;
  }

  DisplayRatesProvider displayRates_;
  FrameDivider divider_;
};

} // namespace

std::unique_ptr<PresentationRateController::Backend> createFrameDividerRateBackend(
    DisplayRatesProvider displayRates,
    FrameDivider divider) {
  return std::make_unique<FrameDividerRateBackend>(std::move(displayRates), std::move(divider));
}

} // namespace igl::shell
