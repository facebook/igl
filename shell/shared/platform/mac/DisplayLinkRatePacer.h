/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <atomic>
#include <memory>
#include <shell/shared/platform/TickSourceRateBackend.h>

namespace igl::shell {

/// Paces a CVDisplayLink-driven render loop.
///
/// Unlike CADisplayLink and MTKView, a CVDisplayLink has no preferred-rate API: it fires
/// once per display refresh and that is all it will ever do. A lower rate is reached by
/// rendering one callback in N and returning early from the rest. The link keeps running
/// at the display's rate; only the frame does not.
///
/// shouldRender() runs on the display link's own thread while setRefreshesPerFrame() runs
/// on the thread that services the rate request, so the divisor is atomic and the counter
/// belongs to the callback thread alone.
class DisplayLinkRatePacer {
 public:
  /// The largest group this counts to. A request needing more is refused by
  /// createDisplayLinkPacerBackend() rather than clamped, because a clamped divisor is a
  /// cadence that silently disagrees with the rate the caller was told it was granted.
  static constexpr int kMaxRefreshesPerFrame = 10000;

  /// Renders one callback in `refreshesPerFrame`. Values outside
  /// [1, kMaxRefreshesPerFrame] are clamped, which is defence for a direct caller — the
  /// rate backend rejects them before they reach here.
  void setRefreshesPerFrame(int refreshesPerFrame) noexcept;

  [[nodiscard]] int getRefreshesPerFrame() const noexcept;

  /// Whether this callback should render. Advances the counter, so call it exactly once
  /// per display-link callback and nowhere else. A divisor that changed since the last
  /// callback takes effect on this one: a new rung starts a new group rather than
  /// finishing out a group sized for the rate it replaced.
  [[nodiscard]] bool shouldRender() noexcept;

 private:
  std::atomic<int> refreshesPerFrame_{1};
  /// Both callback thread only. The divisor is compared here rather than reset from
  /// setRefreshesPerFrame(), because the counter belongs to this thread and the setter
  /// runs on another one.
  int refreshesSinceRender_ = 0;
  int lastRefreshesPerFrame_ = 1;
};

/// Presentation-rate backend for a CVDisplayLink-driven loop paced by `pacer`.
///
/// `maxRateProvider` reports the rate the link itself runs at; the caller owns that query
/// so this file stays plain C++ and testable without a display. It is read once per
/// request, by TickSourceRateBackend, and the snapshot is handed to the applier — the
/// divisor is derived from the same number the granted rate was, never from a second look
/// at a display that may have changed in between.
///
/// The pacer is held weakly because it belongs to the view that owns the link: once that
/// view is gone, a request is refused rather than applied to something nothing reads.
std::unique_ptr<PresentationRateController::Backend> createDisplayLinkPacerBackend(
    std::weak_ptr<DisplayLinkRatePacer> pacer,
    TickSourceRateBackend::MaxRateProvider maxRateProvider);

} // namespace igl::shell
