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

/*
 * Create flags
 */
enum NVGcreateFlags {
  /*
   * Flag indicating if geometry based anti-aliasing is used (may not be needed when using MSAA).
   */
  NVG_ANTIALIAS = 1 << 0,
  /*
   * Flag indicating if strokes should be drawn using stencil buffer.
   * The rendering will be a little slower, but path overlaps
   * (i.e. self-intersecting or sharp turns) will be drawn just once.
   */
  NVG_STENCIL_STROKES = 1 << 1,
  /*
   * Flag indicating if double buffering scheme is used.
   * Flag indicating that additional debug checks are done.
   */
  NVG_DEBUG = 1 << 2,
};

/*
 * These are additional flags on top of NVGimageFlags.
 */
enum NVGimageFlags {
  /*
   * Do not delete texture handle.
   */
  NVG_IMAGE_NODELETE = 1 << 16,
};

/*
 * Creates a new NanoVG context.  The `flags` should be combination of `NVGcreateFlags` above.
 */
NVGcontext* CreateContext(igl::IDevice* device, int flags);

/*
 * Set RenderCommandEncoder form outside.
 * @param matrix , use outside matrix, for example : vulkan preRotate matrix.
 */
void SetRenderCommandEncoder(NVGcontext* ctx,
                             igl::IFramebuffer* framebuffer,
                             igl::IRenderCommandEncoder*,
                             float* matrix);

/*
 * Deletes the specified NanoVG context.
 */
void DestroyContext(NVGcontext* ctx);

} // namespace iglu::nanovg
