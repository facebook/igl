/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#import "DisplayLinkRatePacing.h"

#include <shell/shared/platform/Platform.h>
#include <shell/shared/platform/PresentationRateController.h>
#include <shell/shared/platform/mac/DisplayLinkRatePacer.h>

namespace igl::shell {

namespace {

/// The rate `displayLink` fires at, in Hz, or zero when CoreVideo will not name one.
///
/// Nominal rather than actual: the actual period is measured and drifts frame to frame, and
/// a granted rate that moves is not a granted rate. Read from the link rather than from
/// NSScreen because a link created with CVDisplayLinkCreateWithActiveCGDisplays() follows
/// whichever display it is driving, which need not be the main one.
float refreshRateHz(CVDisplayLinkRef displayLink) {
  if (displayLink == nullptr) {
    return 0.0f;
  }
  const CVTime period = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(displayLink);
  if ((period.flags & kCVTimeIsIndefinite) != 0 || period.timeValue == 0 || period.timeScale == 0) {
    return 0.0f;
  }
  return static_cast<float>(static_cast<double>(period.timeScale) /
                            static_cast<double>(period.timeValue));
}

} // namespace

DisplayLinkRatePacing::DisplayLinkRatePacing(Platform& platform, CVDisplayLinkRef displayLink) :
  platform_(&platform), displayLink_(displayLink) {
  if (displayLink_ == nullptr) {
    return;
  }
  pacer_ = std::make_shared<DisplayLinkRatePacer>();
  const std::weak_ptr<DisplayLinkRatePacer> weakPacer = pacer_;
  CVDisplayLinkRef link = displayLink_;
  platform_->getPresentationRateController().setBackend(
      createDisplayLinkPacerBackend(weakPacer, [weakPacer, link]() -> float {
        // detach() stops the link and drops the pacer before the view releases the handle,
        // so an expired pacer is how the provider knows the handle is no longer its to read.
        return weakPacer.expired() ? 0.0f : refreshRateHz(link);
      }));
}

DisplayLinkRatePacing::~DisplayLinkRatePacing() {
  detach();
}

bool DisplayLinkRatePacing::shouldRender() noexcept {
  // No pacer means no pacing, which is what an unpaced link does: render every callback.
  return !pacer_ || pacer_->shouldRender();
}

void DisplayLinkRatePacing::detach() noexcept {
  if (displayLink_ != nullptr) {
    // First, and the reason the rest is safe. CVDisplayLinkStop() does not return while a
    // callback is running, so afterwards nothing is inside shouldRender() and nothing will
    // read the link handle the backend captured. Everything below is then an ordinary
    // main-thread teardown with no other thread to race.
    CVDisplayLinkStop(displayLink_);
    displayLink_ = nullptr;
  }
  if (platform_ != nullptr) {
    // Only one macOS view drives the platform at a time — the Metal, Vulkan and OpenGL
    // paths are mutually exclusive — so the backend being removed is always this one's.
    platform_->getPresentationRateController().setBackend(nullptr);
    platform_ = nullptr;
  }
  pacer_.reset();
}

} // namespace igl::shell
