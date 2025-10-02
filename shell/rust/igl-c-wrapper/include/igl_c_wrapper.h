/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef IGL_C_WRAPPER_H
#define IGL_C_WRAPPER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle types
typedef struct IGLPlatform IGLPlatform;
typedef struct IGLRenderSession IGLRenderSession;
typedef void* IGLNativeWindowHandle;

// Surface textures structure
typedef struct {
    void* color_texture;
    void* depth_texture;
} IGLSurfaceTextures;

// Platform creation/destruction
IGLPlatform* igl_platform_create_metal(IGLNativeWindowHandle window_handle,
                                       int32_t width,
                                       int32_t height);
void igl_platform_destroy(IGLPlatform* platform);

// RenderSession creation/destruction
IGLRenderSession* igl_render_session_create(IGLPlatform* platform);
void igl_render_session_destroy(IGLRenderSession* session);

// RenderSession lifecycle
bool igl_render_session_initialize(IGLRenderSession* session);
bool igl_render_session_update(IGLRenderSession* session);
void igl_render_session_teardown(IGLRenderSession* session);

// Platform helpers
bool igl_platform_get_surface_textures(IGLPlatform* platform,
                                       IGLSurfaceTextures* out_textures);
void igl_platform_present(IGLPlatform* platform);

// Window management
void igl_platform_resize(IGLPlatform* platform, int32_t width, int32_t height);

#ifdef __cplusplus
}
#endif

#endif // IGL_C_WRAPPER_H
