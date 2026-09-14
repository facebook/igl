/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/mac/DisplayLinkRatePacer.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace igl::shell {

void DisplayLinkRatePacer::setRefreshesPerFrame(int refreshesPerFrame) noexcept {
  const int clamped = refreshesPerFrame < 1 ? 1
                                            : std::min(refreshesPerFrame, kMaxRefreshesPerFrame);
  refreshesPerFrame_.store(clamped, std::memory_order_relaxed);
}

int DisplayLinkRatePacer::getRefreshesPerFrame() const noexcept {
  return refreshesPerFrame_.load(std::memory_order_relaxed);
}

bool DisplayLinkRatePacer::shouldRender() noexcept {
  const int refreshesPerFrame = std::max(1, refreshesPerFrame_.load(std::memory_order_relaxed));
  if (refreshesPerFrame != lastRefreshesPerFrame_) {
    // A new rung starts its own group here, rather than finishing out a group sized for
    // the rate it replaced. Without this the first callback after a change is dropped,
    // which is one stutter every time the selector moves.
    lastRefreshesPerFrame_ = refreshesPerFrame;
    refreshesSinceRender_ = 0;
  }
  const bool render = refreshesSinceRender_ == 0;
  refreshesSinceRender_ = (refreshesSinceRender_ + 1) % refreshesPerFrame;
  return render;
}

std::unique_ptr<PresentationRateController::Backend> createDisplayLinkPacerBackend(
    std::weak_ptr<DisplayLinkRatePacer> pacer,
    TickSourceRateBackend::MaxRateProvider maxRateProvider) {
  auto applier = [pacer](float hz, float maxRateHz) -> Result {
    const std::shared_ptr<DisplayLinkRatePacer> owned = pacer.lock();
    if (!owned) {
      return Result{Result::Code::InvalidOperation,
                    "The view whose display link this paces has gone away."};
    }
    // `maxRateHz` is the snapshot the cadence was snapped against, not a fresh reading, so
    // the divisor recovered here is the same whole number the granted rate came from.
    const float refreshesPerFrame = maxRateHz / hz;
    // Range-checked while it is still a float. std::lround() on a value beyond `long` is
    // undefined and returns something unspecified, and a perfectly finite float ratio sits
    // far beyond `long` for any rate small enough — so converting first and checking after
    // can accept a cadence nobody asked for.
    constexpr auto kMaxRatio = static_cast<float>(DisplayLinkRatePacer::kMaxRefreshesPerFrame);
    if (!std::isfinite(refreshesPerFrame) || refreshesPerFrame > kMaxRatio) {
      return Result{Result::Code::ArgumentOutOfRange,
                    "That rate would need more skipped refreshes than this display link "
                    "paces, so it cannot be held to it."};
    }
    // At least one refresh per frame. The cadence is never above the link's own rate, so
    // this only absorbs float noise at exactly that rate, where every refresh is right.
    owned->setRefreshesPerFrame(static_cast<int>(std::max(1L, std::lround(refreshesPerFrame))));
    return Result{};
  };
  return std::make_unique<TickSourceRateBackend>(std::move(maxRateProvider), std::move(applier));
}

} // namespace igl::shell
