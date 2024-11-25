/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include "nanovg.h"
#include <igl/IGL.h>

namespace iglu::nanovg {

// Create flags
enum NVGcreateFlags {
  // Flag indicating if geometry based anti-aliasing is used (may not be
  // needed when using MSAA).
  NVG_ANTIALIAS = 1 << 0,
  // Flag indicating if strokes should be drawn using stencil buffer.
  // The rendering will be a little slower, but path overlaps
  // (i.e. self-intersecting or sharp turns) will be drawn just once.
  NVG_STENCIL_STROKES = 1 << 1,
  // Flag indicating if double buffering scheme is used.
  NVG_DOUBLE_BUFFER = 1 << 12,
  // Flag indicating if triple buffering scheme is used.
  NVG_TRIPLE_BUFFER = 1 << 13,
  // Flag indicating that additional debug checks are done.
  NVG_DEBUG = 1 << 2,
};

// These are additional flags on top of NVGimageFlags.
enum NVGimageFlagsMetal {
  NVG_IMAGE_NODELETE = 1 << 16, // Do not delete Metal texture handle.
};

// The possible OS targets.
enum MNVGTarget {
  MNVG_IOS,
  MNVG_MACOS,
  MNVG_SIMULATOR,
  MNVG_TVOS,
  MNVG_UNKNOWN,
};

struct MNVGframebuffer {
  NVGcontext* ctx;
  int image;
};
typedef struct MNVGframebuffer MNVGframebuffer;

// Creates a new NanoVG context.  The `flags` should be combination of `NVGcreateFlags` above.
NVGcontext* CreateContext(igl::IDevice* device, int flags);

void SetRenderCommandEncoder(NVGcontext* ctx,
                        std::shared_ptr<igl::IFramebuffer> framebuffer,
                        std::shared_ptr<igl::IRenderCommandEncoder>);

// Deletes the specified NanoVG context.
void DeleteContext(NVGcontext* ctx);

} // namespace iglu::nanovg
