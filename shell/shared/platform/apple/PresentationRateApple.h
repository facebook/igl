/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <shell/shared/platform/PresentationRateController.h>

// Obj-C++ only: the tick sources these back onto are Objective-C objects, and a plain C++
// translation unit has no way to name them.
#if defined(__OBJC__)

@class CADisplayLink;
@class MTKView;

namespace igl::shell {

/// Presentation-rate backend for a view whose frames come from MTKView's own display link
/// — the Metal path on both Apple platforms, and the one that has no CADisplayLink of its
/// own for the shell to reach.
///
/// The view is held weakly, because it outlives nothing here: a request that arrives after
/// the window has gone is refused rather than followed into a dangling pointer.
std::unique_ptr<PresentationRateController::Backend> createMTKViewPresentationRateBackend(
    MTKView* view);

#if IGL_PLATFORM_IOS
/// Presentation-rate backend for a tick source that is a CADisplayLink the shell drives
/// itself — the non-Metal iOS path. Held weakly, for the same reason as above.
std::unique_ptr<PresentationRateController::Backend> createDisplayLinkPresentationRateBackend(
    CADisplayLink* displayLink);
#endif // IGL_PLATFORM_IOS

} // namespace igl::shell

#endif // defined(__OBJC__)
