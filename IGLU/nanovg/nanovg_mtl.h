// Copyright (c) 2017 Ollix
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// ---
// Author: olliwang@ollix.com (Olli Wang)

#ifndef NANOVG_MTL_H_
#define NANOVG_MTL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "nanovg.h"

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

// Creates a new NanoVG context. The `metalLayer` parameter should be a
// `CAMetalLayer` object, and the `flags` should be combination of
// `NVGcreateFlags` above.
NVGcontext* nvgCreateMTL(void* metalLayer, int flags);

// Deletes the specified NanoVG context.
void nvgDeleteMTL(NVGcontext* ctx);

//
// Framebuffer
//

// Binds the specified framebuffer as the current render pass.
void mnvgBindFramebuffer(MNVGframebuffer* framebuffer);

// Creates a new framebuffer.
MNVGframebuffer* mnvgCreateFramebuffer(NVGcontext* ctx, int width,
                                       int height, int imageFlags);

// Deletes the specified framebuffer.
void mnvgDeleteFramebuffer(MNVGframebuffer* framebuffer);

//
// Metal bridging functions
//

// Clear context on next frame, must be called before nvgEndFrame
void mnvgClearWithColor(NVGcontext* ctx, NVGcolor color);

// Returns a pointer to the corresponded `id<MTLCommandQueue>` object.
void* mnvgCommandQueue(NVGcontext* ctx);

// Creates an image id from a `id<MTLTexture>` object pointer.
int mnvgCreateImageFromHandle(NVGcontext* ctx, void* textureId, int imageFlags);

// Returns a pointer to the corresponded `id<MTLDevice>` object.
void* mnvgDevice(NVGcontext* ctx);

// Returns a pointer to the `id<MTLTexture>` object of the specified image.
void* mnvgImageHandle(NVGcontext* ctx, int image);

// Copies the pixels from the specified image into the specified `data`.
void mnvgReadPixels(NVGcontext* ctx, int image, int x, int y, int width,
                    int height, void* data);

// Returns the current OS target.
enum MNVGTarget mnvgTarget();

#ifdef __cplusplus
}
#endif

#endif  // NANOVG_MTL_H_
