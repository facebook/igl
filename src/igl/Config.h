/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// @fb-only
// @fb-only
// @fb-only

///--------------------------------------
/// MARK: - Platform

///
/// Platform conditionals specify which OS the code is being compiled for.
///
/// MICROSOFT(WIN)/APPLE/ANDROID/LINUX are mutually exclusive.
/// MACOS/IOS(_SIMULATOR) are mutually exclusive.
///
/// The following set of conditionals exist and currently are supported:
///
/// Windows:
///   IGL_PLATFORM_WINDOWS
/// Apple:
///   IGL_PLATFORM_APPLE
///   IGL_PLATFORM_IOS
///   IGL_PLATFORM_IOS_SIMULATOR
///   IGL_PLATFORM_MACCATALYST
///   IGL_PLATFORM_MACOSX
/// Android:
///   IGL_PLATFORM_ANDROID
/// Linux:
///   IGL_PLATFORM_LINUX
/// WEBGL:
///   IGL_PLATFORM_EMSCRIPTEN

#if !defined(IGL_CMAKE_BUILD)

// clang-format off
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only
// clang-format on

#else // !defined(IGL_CMAKE_BUILD)

// clang-format off
// Windows
#if defined(_WIN32)
  #define IGL_PLATFORM_WINDOWS 1
  #define IGL_PLATFORM_APPLE 0
  #define IGL_PLATFORM_IOS 0
  #define IGL_PLATFORM_IOS_SIMULATOR 0
  #define IGL_PLATFORM_MACCATALYST 0
  #define IGL_PLATFORM_MACOSX 0
  #define IGL_PLATFORM_ANDROID 0
  #define IGL_PLATFORM_LINUX 0
  #define IGL_PLATFORM_EMSCRIPTEN 0
// Apple
#elif defined (__APPLE__)
  #define IGL_PLATFORM_WINDOWS 0
  #define IGL_PLATFORM_ANDROID 0
  #define IGL_PLATFORM_LINUX 0
  #define IGL_PLATFORM_APPLE 1
  #define IGL_PLATFORM_EMSCRIPTEN 0

  #include <TargetConditionals.h>
  #if TARGET_OS_SIMULATOR
    #define IGL_PLATFORM_IOS 1 // iOS Simulator is iOS
    #define IGL_PLATFORM_IOS_SIMULATOR 1
    #define IGL_PLATFORM_MACCATALYST 0
    #define IGL_PLATFORM_MACOSX 0
  #elif TARGET_OS_MACCATALYST
    #define IGL_PLATFORM_IOS 0
    #define IGL_PLATFORM_IOS_SIMULATOR 0
    #define IGL_PLATFORM_MACCATALYST 1
    #define IGL_PLATFORM_MACOSX 0
  #elif TARGET_OS_IPHONE
    #define IGL_PLATFORM_IOS 1
    #define IGL_PLATFORM_IOS_SIMULATOR 0
    #define IGL_PLATFORM_MACCATALYST 0
    #define IGL_PLATFORM_MACOSX 0
  #elif TARGET_OS_OSX
    #define IGL_PLATFORM_IOS 0
    #define IGL_PLATFORM_IOS_SIMULATOR 0
    #define IGL_PLATFORM_MACCATALYST 0
    #define IGL_PLATFORM_MACOSX 1
  #else
    #error "Unknown Apple target"
  #endif
// Android
#elif defined(__ANDROID__)
  #define IGL_PLATFORM_WINDOWS 0
  #define IGL_PLATFORM_APPLE 0
  #define IGL_PLATFORM_IOS 0
  #define IGL_PLATFORM_IOS_SIMULATOR 0
  #define IGL_PLATFORM_MACCATALYST 0
  #define IGL_PLATFORM_MACOSX 0
  #define IGL_PLATFORM_ANDROID 1
  #define IGL_PLATFORM_LINUX 0
  #define IGL_PLATFORM_EMSCRIPTEN 0
// Linux
#elif defined(__linux__)
  #define IGL_PLATFORM_WINDOWS 0
  #define IGL_PLATFORM_APPLE 0
  #define IGL_PLATFORM_IOS 0
  #define IGL_PLATFORM_IOS_SIMULATOR 0
  #define IGL_PLATFORM_MACCATALYST 0
  #define IGL_PLATFORM_MACOSX 0
  #define IGL_PLATFORM_ANDROID 0
  #define IGL_PLATFORM_LINUX 1
  #define IGL_PLATFORM_EMSCRIPTEN 0
#elif defined(__EMSCRIPTEN__)
  #define IGL_PLATFORM_WINDOWS 0
  #define IGL_PLATFORM_APPLE 0
  #define IGL_PLATFORM_IOS 0
  #define IGL_PLATFORM_IOS_SIMULATOR 0
  #define IGL_PLATFORM_MACCATALYST 0
  #define IGL_PLATFORM_MACOSX 0
  #define IGL_PLATFORM_ANDROID 0
  #define IGL_PLATFORM_LINUX 0
  #define IGL_PLATFORM_EMSCRIPTEN 1
// Rest
#else
  #define IGL_PLATFORM_WINDOWS 0
  #define IGL_PLATFORM_APPLE 0
  #define IGL_PLATFORM_IOS 0
  #define IGL_PLATFORM_IOS_SIMULATOR 0
  #define IGL_PLATFORM_MACCATALYST 0
  #define IGL_PLATFORM_MACOSX 0
  #define IGL_PLATFORM_ANDROID 0
  #define IGL_PLATFORM_LINUX 0
  #define IGL_PLATFORM_EMSCRIPTEN 0

  #error "Platform not supported"
#endif

#endif // !defined(IGL_CMAKE_BUILD)

#if IGL_PLATFORM_ANDROID && __ANDROID_MIN_SDK_VERSION__ >= 26
  #define IGL_ANDROID_HWBUFFER_SUPPORTED
#endif // __ANDROID_MIN_SDK_VERSION__ >= 26

// IGL_PLATFORM_XR is for extended reality platforms like OpenXR
#if !defined(IGL_PLATFORM_XR)
  #define IGL_PLATFORM_XR 0
#endif
// clang-format on

///--------------------------------------
/// MARK: - Conditional backend support
//
// While libraries/utilities/helpers typically want to support all IGL backends, top level
// applications might want to be more selective about them to minimize dependencies. IGL exposes
// BUCK configs in defs.bzl to control backend support, and all BUCK modules that have a direct
// dependency on IGL backends are expected to use the definitions provided there for integration.
//
// Furthermore, the macros below are provided so that IGL clients can safely wrap backend specific
// code for conditional compilation.
#if defined(IGL_BACKEND_ENABLE_HEADLESS)
#define IGL_BACKEND_HEADLESS 1
#else
#define IGL_BACKEND_HEADLESS 0
#endif

#if defined(IGL_BACKEND_ENABLE_METAL)
#define IGL_BACKEND_METAL 1
#else
#define IGL_BACKEND_METAL 0
#endif

#if defined(IGL_BACKEND_ENABLE_OPENGL)
#define IGL_BACKEND_OPENGL 1
#else
#define IGL_BACKEND_OPENGL 0
#endif

#if defined(IGL_BACKEND_ENABLE_VULKAN)
#define IGL_BACKEND_VULKAN 1
#else
#define IGL_BACKEND_VULKAN 0
#endif

// @fb-only
// @fb-only
// @fb-only
// @fb-only
// @fb-only

///--------------------------------------
/// MARK: - Angle support
#if defined(FORCE_USE_ANGLE)
#define IGL_ANGLE 1
#define IGL_DISABLE_DEBUG_BUFFER_LABEL 1
#else
#define IGL_ANGLE 0
#endif

//--------------------------------------
/// MARK: - Linux with SwiftShader
//
// IGL_PLATFORM_LINUX_SWIFTSHADER is a use case of IGL_PLATFORM_LINUX that uses
// SwiftShader for rendering. For example, Rainbow uses SwiftShader in a CPU only
// environment currently.
#if defined(FORCE_USE_SWIFTSHADER) && IGL_PLATFORM_LINUX
#define IGL_PLATFORM_LINUX_SWIFTSHADER 1
#else
#define IGL_PLATFORM_LINUX_SWIFTSHADER 0
#endif

//--------------------------------------
/// MARK: - Linux with EGL
//
// IGL_PLATFORM_LINUX_USE_EGL enables EGL context on Linux, otherwise GLX is in use.
// GLX is used in CMake builds for samples and shell apps to use OpenGL 4.6 on Linux desktops.
#if IGL_PLATFORM_LINUX
#if !defined(IGL_PLATFORM_LINUX_USE_EGL)
#define IGL_PLATFORM_LINUX_USE_EGL 1
#endif
#else
#if !defined(IGL_PLATFORM_LINUX_USE_EGL)
#define IGL_PLATFORM_LINUX_USE_EGL 0
#endif
#endif

///--------------------------------------
/// MARK: - Debug

// clang-format off
#if !defined(IGL_DEBUG) // allow build systems to define it
#if defined(IGL_BUILD_MODE_OPT)
  // Forced opt build.
  #define IGL_DEBUG 0
#elif IGL_PLATFORM_ANDROID && !defined(FBANDROID_BUILD_MODE_OPT)
  // On Android, buck defines NDEBUG for all builds so the test above doesn't work.
  // FBANDROID_BUILD_MODE_OPT is only defined in production builds and was created
  // with the exact purpose of allowing native code to differentiate build modes.
  #define IGL_DEBUG 1
#elif IGL_PLATFORM_WINDOWS
  // Visual Studio never defines NDEBUG, it uses _DEBUG instead. See:
  // https://learn.microsoft.com/en-us/cpp/c-runtime-library/debug?view=msvc-170
  #if defined(_DEBUG) || defined(DEBUG)
    #define IGL_DEBUG 1
  #else
    #define IGL_DEBUG 0
  #endif
#elif !defined(NDEBUG)
  #define IGL_DEBUG 1
#else
  #define IGL_DEBUG 0
#endif
#endif
// clang-format on

// clang-format off
#if !defined(IGL_SOFT_ERROR_ENABLED)
  // Either we have IGL_DEBUG, or Windows/Linux/etc, since we don't have good detection mechanism there.
  #if IGL_DEBUG || (!IGL_PLATFORM_APPLE && !IGL_PLATFORM_ANDROID)
    #define IGL_SOFT_ERROR_ENABLED 1
  #else
    #define IGL_SOFT_ERROR_ENABLED 0
  #endif
#endif
// clang-format on
