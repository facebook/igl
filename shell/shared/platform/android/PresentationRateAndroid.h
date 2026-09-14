/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>

struct ANativeWindow;

namespace igl::shell {

/// Asks SurfaceFlinger to run `window` at `hz`, or to choose for itself when `hz` is zero.
///
/// This is ANativeWindow_setFrameRate(), resolved with dlsym() because the shell's
/// minSdkVersion is 21 and the NDK declares the symbol only from API 30 — an older device
/// is told Unsupported rather than the build failing to link.
///
/// **Ok means SurfaceFlinger accepted a preference, not that the panel reached the rate.**
/// The API offers no confirmation and a single-mode panel has nothing to switch to, so this
/// must never be the thing a PresentationRateGrant rests on: the grant contract is "the rate
/// now in effect", and nothing here can establish that. It is called once when a
/// presentation-rate backend is installed, to ask for the panel's best mode, and the rungs
/// are then whole divisions of whatever mode the panel is actually in — a number that comes
/// back from the display rather than from this call.
Result setNativeWindowFrameRate(ANativeWindow* window, float hz);

} // namespace igl::shell
