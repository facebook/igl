#ifndef NANOVG_MTL_H_
#define NANOVG_MTL_H_

#include <igl/IGL.h>

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
NVGcontext* nvgCreateMTL(igl::IDevice * device, int flags);

void nvgSetColorTexture(NVGcontext* ctx, std::shared_ptr<igl::IFramebuffer> framebuffer, std::shared_ptr<igl::IRenderCommandEncoder>);

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
