/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef IGL_PLATFORM_HELPER_H
#define IGL_PLATFORM_HELPER_H

#include "igl_graphics.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IGLPlatform IGLPlatform;

// Get current frame's textures (acquires drawable)
typedef struct {
    IGLTexture* color;
    IGLTexture* depth;
} IGLFrameTextures;

bool igl_platform_get_frame_textures(IGLPlatform* platform, IGLFrameTextures* out_textures);
void igl_platform_present_frame(IGLPlatform* platform);

#ifdef __cplusplus
}
#endif

#endif // IGL_PLATFORM_HELPER_H
