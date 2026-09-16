/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/apple/PresentationRateApple.h>

#import <MetalKit/MTKView.h> // IWYU pragma: keep
#include <cmath>
#include <memory>
#include <shell/shared/platform/TickSourceRateBackend.h>

#if IGL_PLATFORM_IOS
#import <QuartzCore/CADisplayLink.h>
#import <UIKit/UIKit.h>
#elif IGL_PLATFORM_MACOSX
#import <AppKit/AppKit.h>
#endif

namespace igl::shell {

namespace {

#if IGL_PLATFORM_IOS

bool infoPlistFlagSet(NSString* key) {
  return [[[NSBundle mainBundle] objectForInfoDictionaryKey:key] boolValue];
}

/// Whether the app opted out of CoreAnimation's minimum frame duration.
///
/// Without one of these Info.plist keys iOS holds every tick source to 60 Hz on a 120 Hz
/// ProMotion panel, while UIScreen keeps reporting 120. Reading them is what stops the
/// seam from granting a rate the app was never going to be given: the request would be
/// applied, the HUD would read 120, and the device would still be running at 60.
bool highRefreshRateOptedIn() {
  return infoPlistFlagSet(@"CADisableMinimumFrameDurationOnPhone") ||
         infoPlistFlagSet(@"CADisableMinimumFrameDuration");
}

/// The device's own screen, for the two callers with no view to ask: the CADisplayLink
/// path, which is bound to that screen by construction and never sees a view, and an
/// MTKView whose backend is installed before it is attached to a window. The shell's iOS
/// app is one window on one screen, so both reach the same display.
UIScreen* deviceScreen() {
  // @lint-ignore ASTGREP common/objcpp/no-uiscreen-mainscreen
  return [UIScreen mainScreen];
}

float maxRateHzForScreen(UIScreen* screen) {
  if (screen == nil) {
    return 0.0f;
  }
  const float screenMaxHz = static_cast<float>(screen.maximumFramesPerSecond);
  // What CoreAnimation clamps to without the Info.plist opt-out.
  constexpr float kMinimumFrameDurationCapHz = 60.0f;
  if (!highRefreshRateOptedIn() && screenMaxHz > kMinimumFrameDurationCapHz) {
    return kMinimumFrameDurationCapHz;
  }
  return screenMaxHz;
}

#elif IGL_PLATFORM_MACOSX

float maxRateHzForScreen(NSScreen* screen) {
  if (screen == nil) {
    return 0.0f;
  }
  return static_cast<float>(screen.maximumFramesPerSecond);
}

#endif

/// The highest rate `view` can be ticked at, in Hz, or zero when there is no screen to
/// ask. Reads window and screen state, so it belongs on the thread that owns the tick
/// source — the main thread on both Apple platforms.
float maxRateHzForView(MTKView* view) {
#if IGL_PLATFORM_IOS
  return maxRateHzForScreen(view.window.screen ?: deviceScreen());
#elif IGL_PLATFORM_MACOSX
  // NSWindow::screen is nil for a window that is offscreen or not yet placed.
  return maxRateHzForScreen(view.window.screen ?: [NSScreen mainScreen]);
#else
  (void)view;
  return 0.0f;
#endif
}

} // namespace

std::unique_ptr<PresentationRateController::Backend> createMTKViewPresentationRateBackend(
    MTKView* view) {
  __weak MTKView* weakView = view;
  return std::make_unique<TickSourceRateBackend>(
      [weakView]() -> float {
        MTKView* strongView = weakView;
        return strongView == nil ? 0.0f : maxRateHzForView(strongView);
      },
      [weakView](float hz, float /*maxRateHz*/) -> Result {
        MTKView* strongView = weakView;
        if (strongView == nil) {
          return Result{Result::Code::InvalidOperation,
                        "The view whose display link paces this platform has gone away."};
        }
        // MTKView takes whole frames per second and defaults to 60 whatever the panel
        // does, so this is also what lifts the Metal path off that default.
        strongView.preferredFramesPerSecond = static_cast<NSInteger>(std::lround(hz));
        return Result{};
      });
}

#if IGL_PLATFORM_IOS
std::unique_ptr<PresentationRateController::Backend> createDisplayLinkPresentationRateBackend(
    CADisplayLink* displayLink) {
  __weak CADisplayLink* weakLink = displayLink;
  return std::make_unique<TickSourceRateBackend>(
      []() -> float { return maxRateHzForScreen(deviceScreen()); },
      [weakLink](float hz, float /*maxRateHz*/) -> Result {
        CADisplayLink* strongLink = weakLink;
        if (strongLink == nil) {
          return Result{Result::Code::InvalidOperation,
                        "The display link that paces this platform has gone away."};
        }
        if (@available(iOS 15.0, *)) {
          // Pinned to a single rate rather than given a range: this exists so a profiling
          // run holds one cadence, and a range is an invitation for CoreAnimation to
          // drift inside it.
          strongLink.preferredFrameRateRange = CAFrameRateRangeMake(hz, hz, hz);
        } else {
          // `preferredFrameRateRange` and `CAFrameRateRangeMake()` are both iOS 15+, and
          // the demo's deployment target is lower, so an older OS would be sent a
          // selector it does not have. The whole-number property is the only knob there,
          // and a range whose three values are equal is what it already means.
          strongLink.preferredFramesPerSecond = static_cast<NSInteger>(std::lround(hz));
        }
        return Result{};
      });
}
#endif // IGL_PLATFORM_IOS

} // namespace igl::shell
