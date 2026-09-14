/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#import <CoreVideo/CVDisplayLink.h>
#include <memory>

namespace igl::shell {

class DisplayLinkRatePacer;
class Platform;

/// The one owner of presentation-rate pacing for a CVDisplayLink-driven view.
///
/// Every macOS path that renders from a CVDisplayLink needs the same three things — a pacer
/// for the callback to consult, a rate backend installed on the platform, and a teardown
/// that runs in the one order that is safe — so they live here once instead of at each view.
///
/// **The teardown order is the point of this class.** The callback runs on the link's own
/// thread and the installed backend holds the raw CVDisplayLinkRef, so tearing down in the
/// obvious order races both. detach() stops the link first: CVDisplayLinkStop() does not
/// return while a callback is running, so once it has, nothing is reading the pacer and
/// nothing will read the link handle again. Only then does it drop the backend and the
/// pacer.
///
/// **An owning view must call detach() before it touches the handle holding this object,
/// and only then release the link.** Destroying the object also calls detach(), but that is
/// a safety net and not a substitute: `std::unique_ptr::reset()` stores null *before* it
/// runs the pointee's destructor, so a callback reading that same handle races the write
/// and can see null while the link is still firing. detach() first, while the handle is
/// stable, closes that window — after it returns there is no other thread to race.
///
///     ratePacing->detach();          // stops the link; callbacks are quiesced
///     ratePacing.reset();            // now nothing else reads the handle
///     CVDisplayLinkRelease(link);    // and nothing else holds the link
class DisplayLinkRatePacing {
 public:
  /// Installs a presentation-rate backend on `platform` for `displayLink`. Both must
  /// outlive this object, which is what detach() at teardown guarantees.
  DisplayLinkRatePacing(Platform& platform, CVDisplayLinkRef displayLink);
  ~DisplayLinkRatePacing();

  DisplayLinkRatePacing(const DisplayLinkRatePacing&) = delete;
  DisplayLinkRatePacing& operator=(const DisplayLinkRatePacing&) = delete;
  DisplayLinkRatePacing(DisplayLinkRatePacing&&) = delete;
  DisplayLinkRatePacing& operator=(DisplayLinkRatePacing&&) = delete;

  /// Whether the current display-link callback should render. Called from the link's own
  /// thread, exactly once per callback.
  [[nodiscard]] bool shouldRender() noexcept;

  /// Teardown, not a pause: it stops the link as well as removing the pacing, because
  /// stopping is what makes removing the rest safe. Idempotent, and for the main thread.
  void detach() noexcept;

 private:
  Platform* platform_ = nullptr;
  CVDisplayLinkRef displayLink_ = nullptr;
  std::shared_ptr<DisplayLinkRatePacer> pacer_;
};

} // namespace igl::shell
