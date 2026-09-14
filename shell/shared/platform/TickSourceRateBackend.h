/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>
#include <shell/shared/platform/PresentationRateController.h>

namespace igl::shell {

/// Presentation-rate backend for a platform whose frames come from a tick source with a
/// settable preferred rate — a display link, or a view that owns one.
///
/// The tick source is reached through two callables so this stays plain C++. Everything
/// that has to be reasoned about — resolving PresentationRateMode::DisplayMaximum against
/// what the display reports, working out the cadence the tick source can actually deliver,
/// and naming the refusal when it cannot deliver one — lives here rather than once per
/// windowing system.
class TickSourceRateBackend final : public PresentationRateController::Backend {
 public:
  /// The highest rate the tick source can be driven at, in Hz, or zero when the platform
  /// will not say. Queried on every call rather than cached, so a window moved to another
  /// display is picked up. Called on the thread that owns the tick source.
  using MaxRateProvider = std::function<float()>;

  /// Asks the tick source to run at `hz` — always finite, above zero, and already snapped
  /// to a cadence the tick source can deliver.
  ///
  /// `maxRateHz` is the same snapshot `hz` was derived from, handed over rather than left
  /// to be re-queried: a leg that converts the rate into something of its own must not do
  /// that arithmetic against a display that changed in between, or the cadence it sets and
  /// the rate the caller is told about stop agreeing.
  ///
  /// A non-Ok Result is reported to the caller verbatim and no grant is recorded, so a leg
  /// that cannot represent the cadence says why instead of succeeding at something else.
  using RateApplier = std::function<Result(float hz, float maxRateHz)>;

  TickSourceRateBackend(MaxRateProvider maxRateProvider, RateApplier rateApplier);

  [[nodiscard]] PresentationRateCapabilities getCapabilities() const noexcept override;

  Result apply(const PresentationRateRequest& request, float& outGrantedHz) override;

  /// The rate a tick source running at `maxRateHz` actually delivers when asked for
  /// `requestedHz`. A display link fires on display refreshes, so it can only halve,
  /// third, quarter and so on: a 60 Hz display asked for 50 Hz gives 60, not 50. Returns
  /// zero when either argument is not a finite rate above zero, and when `requestedHz` is
  /// so far below `maxRateHz` that no cadence reaches it.
  [[nodiscard]] static float snapToTickCadence(float requestedHz, float maxRateHz) noexcept;

 private:
  /// The provider's answer, with anything that is not a finite rate above zero folded to
  /// zero so a single check covers "cannot say" and "said something unusable".
  [[nodiscard]] float queryMaxRateHz() const noexcept;

  MaxRateProvider maxRateProvider_;
  RateApplier rateApplier_;
};

} // namespace igl::shell
