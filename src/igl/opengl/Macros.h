/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// @fb-only

#include <igl/Macros.h>

#if IGL_PLATFORM_IOS_SIMULATOR || IGL_PLATFORM_IOS || IGL_PLATFORM_MACCATALYST || IGL_ANGLE || \
    IGL_PLATFORM_ANDROID || IGL_PLATFORM_EMSCRIPTEN || IGL_PLATFORM_LINUX_USE_EGL
#define IGL_OPENGL_ES 1
#define IGL_OPENGL 0
#else
#define IGL_OPENGL_ES 0
#define IGL_OPENGL 1
#endif

// Here we enable IGL_EGL on Linux to load GL functions by eglGetProcAddress.
#if (IGL_OPENGL_ES && !IGL_PLATFORM_APPLE) || IGL_PLATFORM_LINUX
#define IGL_EGL 1
#else
#define IGL_EGL 0
#endif

#if IGL_PLATFORM_WIN && !IGL_OPENGL_ES
#define IGL_WGL 1
#else
#define IGL_WGL 0
#endif
