/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <igl/Macros.h> // IWYU pragma: export

#if defined(USE_VULKAN_BACKEND) && IGL_BACKEND_VULKAN
#include <igl/vulkan/Common.h> // IWYU pragma: export
#endif // defined(USE_VULKAN_BACKEND) && IGL_BACKEND_VULKAN

#if defined(USE_OPENGL_BACKEND) && IGL_BACKEND_OPENGL
#include <igl/opengl/GLIncludes.h> // IWYU pragma: export
#endif // defined(USE_OPENGL_BACKEND) && IGL_BACKEND_OPENGL

#if IGL_PLATFORM_ANDROID
#include <android/native_window_jni.h> // IWYU pragma: export
#include <jni.h> // IWYU pragma: export
#if IGL_BACKEND_OPENGL
#include <EGL/egl.h> // IWYU pragma: export
#endif // IGL_BACKEND_OPENGL
#endif // IGL_PLATFORM_ANDROID

#if IGL_PLATFORM_WINDOWS
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <unknwn.h> // IWYU pragma: export
#include <windows.h> // IWYU pragma: export
#endif // IGL_PLATFORM_WINDOWS

#include <openxr/openxr.h> // IWYU pragma: export
#include <openxr/openxr_platform.h> // IWYU pragma: export
