/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/mac/DisplayLinkRatePacer.h>

#include <algorithm>
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
    const int refreshesPerFrame = TickSourceRateBackend::refreshesPerFrame(
        hz, maxRateHz, DisplayLinkRatePacer::kMaxRefreshesPerFrame);
    if (refreshesPerFrame == 0) {
      return Result{Result::Code::ArgumentOutOfRange,
                    "That rate would need more skipped refreshes than this display link "
                    "paces, so it cannot be held to it."};
    }
    owned->setRefreshesPerFrame(refreshesPerFrame);
    return Result{};
  };
  return std::make_unique<TickSourceRateBackend>(std::move(maxRateProvider), std::move(applier));
}

} // namespace igl::shell
