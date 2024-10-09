/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

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
///   IGL_PLATFORM_WIN
/// Apple:
///   IGL_PLATFORM_APPLE
///   IGL_PLATFORM_IOS
///   IGL_PLATFORM_IOS_SIMULATOR
///   IGL_PLATFORM_MACCATALYST
///   IGL_PLATFORM_MACOS
/// Android:
///   IGL_PLATFORM_ANDROID
/// Linux:
///   IGL_PLATFORM_LINUX
/// WEBGL:
///   IGL_PLATFORM_EMSCRIPTEN

// clang-format off
// Windows
#if defined(_WIN32)
  #define IGL_PLATFORM_WIN 1
  #define IGL_PLATFORM_APPLE 0
  #define IGL_PLATFORM_IOS 0
  #define IGL_PLATFORM_IOS_SIMULATOR 0
  #define IGL_PLATFORM_MACCATALYST 0
  #define IGL_PLATFORM_MACOS 0
  #define IGL_PLATFORM_ANDROID 0
  #define IGL_PLATFORM_LINUX 0
  #define IGL_PLATFORM_EMSCRIPTEN 0
// Apple
#elif defined (__APPLE__)
  #define IGL_PLATFORM_WIN 0
  #define IGL_PLATFORM_ANDROID 0
  #define IGL_PLATFORM_LINUX 0
  #define IGL_PLATFORM_APPLE 1
  #define IGL_PLATFORM_EMSCRIPTEN 0

  #include <TargetConditionals.h>
  #if TARGET_OS_SIMULATOR
    #define IGL_PLATFORM_IOS 1 // iOS Simulator is iOS
    #define IGL_PLATFORM_IOS_SIMULATOR 1
    #define IGL_PLATFORM_MACCATALYST 0
    #define IGL_PLATFORM_MACOS 0
  #elif TARGET_OS_MACCATALYST
    #define IGL_PLATFORM_IOS 0
    #define IGL_PLATFORM_IOS_SIMULATOR 0
    #define IGL_PLATFORM_MACCATALYST 1
    #define IGL_PLATFORM_MACOS 0
  #elif TARGET_OS_IPHONE
    #define IGL_PLATFORM_IOS 1
    #define IGL_PLATFORM_IOS_SIMULATOR 0
    #define IGL_PLATFORM_MACCATALYST 0
    #define IGL_PLATFORM_MACOS 0
  #elif TARGET_OS_OSX
    #define IGL_PLATFORM_IOS 0
    #define IGL_PLATFORM_IOS_SIMULATOR 0
    #define IGL_PLATFORM_MACCATALYST 0
    #define IGL_PLATFORM_MACOS 1
  #else
    #error "Unknown Apple target"
  #endif
// Android
#elif defined(__ANDROID__)
  #define IGL_PLATFORM_WIN 0
  #define IGL_PLATFORM_APPLE 0
  #define IGL_PLATFORM_IOS 0
  #define IGL_PLATFORM_IOS_SIMULATOR 0
  #define IGL_PLATFORM_MACCATALYST 0
  #define IGL_PLATFORM_MACOS 0
  #define IGL_PLATFORM_ANDROID 1
  #define IGL_PLATFORM_LINUX 0
  #define IGL_PLATFORM_EMSCRIPTEN 0

#if __ANDROID_MIN_SDK_VERSION__ >= 26
  #define IGL_ANDROID_HWBUFFER_SUPPORTED
#endif // __ANDROID_MIN_SDK_VERSION__ >= 26

// Linux
#elif defined(__linux__)
  #define IGL_PLATFORM_WIN 0
  #define IGL_PLATFORM_APPLE 0
  #define IGL_PLATFORM_IOS 0
  #define IGL_PLATFORM_IOS_SIMULATOR 0
  #define IGL_PLATFORM_MACCATALYST 0
  #define IGL_PLATFORM_MACOS 0
  #define IGL_PLATFORM_ANDROID 0
  #define IGL_PLATFORM_LINUX 1
  #define IGL_PLATFORM_EMSCRIPTEN 0
#elif defined(__EMSCRIPTEN__)
  #define IGL_PLATFORM_WIN 0
  #define IGL_PLATFORM_APPLE 0
  #define IGL_PLATFORM_IOS 0
  #define IGL_PLATFORM_IOS_SIMULATOR 0
  #define IGL_PLATFORM_MACCATALYST 0
  #define IGL_PLATFORM_MACOS 0
  #define IGL_PLATFORM_ANDROID 0
  #define IGL_PLATFORM_LINUX 0
  #define IGL_PLATFORM_EMSCRIPTEN 1
// Rest
#else
  #define IGL_PLATFORM_WIN 0
  #define IGL_PLATFORM_APPLE 0
  #define IGL_PLATFORM_IOS 0
  #define IGL_PLATFORM_IOS_SIMULATOR 0
  #define IGL_PLATFORM_MACCATALYST 0
  #define IGL_PLATFORM_MACOS 0
  #define IGL_PLATFORM_ANDROID 0
  #define IGL_PLATFORM_LINUX 0
  #define IGL_PLATFORM_EMSCRIPTEN 0

  #error "Platform not supported"
#endif

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
#if defined(FORCE_USE_SWIFTSHADER) && defined(IGL_PLATFORM_LINUX)
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
#elif defined(XR_DEBUG_BUILD)
  #define IGL_DEBUG 1
#elif IGL_PLATFORM_ANDROID && !defined(FBANDROID_BUILD_MODE_OPT)
  // On Android, buck defines NDEBUG for all builds so the test above doesn't work.
  // FBANDROID_BUILD_MODE_OPT is only defined in production builds and was created
  // with the exact purpose of allowing native code to differentiate build modes.
  #define IGL_DEBUG 1
#elif defined(_MSC_VER)
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

///--------------------------------------
/// MARK: - String Macros

// Concatenation
#define IGL_CONCAT(a, b) _IGL_CONCAT_ARGS_WRAPPER(a, b)
#define _IGL_CONCAT_ARGS_WRAPPER(a, b) _IGL_CONCAT_ARGS(a, b)
#define _IGL_CONCAT_ARGS(a, b) a##b

#define IGL_CONCAT3(a, b, c) IGL_CONCAT(IGL_CONCAT(a, b), c)

// Convert Macro to String Literal
#define IGL_TO_STRING(a) _IGL_TO_STRING_WRAPPER(a)
#define _IGL_TO_STRING_WRAPPER(a) #a

///--------------------------------------
/// MARK: - Macro Helpers

#define _IGL_EXPAND(x) x
#define _IGL_ECHO(...) __VA_ARGS__
#define _IGL_CALL(macro, args) macro args

// Returns the 64th argument.
// clang-format off
#define _IGL_ARG_64(_,\
   _64,_63,_62,_61,_60,_59,_58,_57,_56,_55,_54,_53,_52,_51,_50,_49, \
   _48,_47,_46,_45,_44,_43,_42,_41,_40,_39,_38,_37,_36,_35,_34,_33, \
   _32,_31,_30,_29,_28,_27,_26,_25,_24,_23,_22,_21,_20,_19,_18,_17, \
   _16,_15,_14,_13,_12,_11,_10,_9,_8,_7,_6,_5,_4,_3,_2,ARG,...) ARG
// clang-format on

// Returns 0 if __VA_ARGS__ has a comma, 1 otherwise
// Does NOT handle the case when __VA_ARGS__ is empty
// Handles up to 64 arguments
// clang-format off
#define _IGL_HAS_COMMA(...) _IGL_EXPAND(_IGL_ARG_64(__VA_ARGS__, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ,1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0))
// clang-format on

///--------------------------------------
/// MARK: - Linkage

#if defined(__cplusplus)
#define IGL_EXTERN extern "C"
#define IGL_EXTERN_BEGIN extern "C" {
#define IGL_EXTERN_END } // extern "C"
#else
#define IGL_EXTERN extern
#define IGL_EXTERN_BEGIN
#define IGL_EXTERN_END
#endif

// Visibility
#if defined(_MSC_VER)
#define _IGL_PUBLIC_ATTRIBUTE __declspec(dllexport)
#elif defined(__clang__) || defined(__GNUC__)
#define _IGL_PUBLIC_ATTRIBUTE __attribute__((visibility("default")))
#else
#define _IGL_PUBLIC_ATTRIBUTE
#endif

#define IGL_EXPORT _IGL_PUBLIC_ATTRIBUTE

// C API
#define IGL_API IGL_EXTERN _IGL_PUBLIC_ATTRIBUTE

///--------------------------------------
/// MARK: - Restrict

#if defined(__GNUC__) || defined(__clang__)
#define IGL_RESTRICT __restrict__
#elif defined(_MSC_VER) && (_MSC_VER >= 1900) // VS2015
#define IGL_RESTRICT __restrict
#else
#define IGL_RESTRICT
#endif

///--------------------------------------
/// MARK: - Inline

#if !defined(IGL_INLINE)
#define IGL_INLINE inline
#endif

///--------------------------------------
/// MARK: - Function Signature

#if defined(__GNUC__) || defined(__clang__)
#define IGL_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define IGL_FUNCTION __FUNCSIG__
#elif defined(__cplusplus) && (__cplusplus >= 201103L) // C++11
#define IGL_FUNCTION __func__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) // C99
#define IGL_FUNCTION __func__
#else
#define IGL_FUNCTION "<function> " __FILE__ "(" IGL_MACRO_TO_STRING(__LINE__) ")"
#endif

///--------------------------------------
/// MARK: - Newline

#if IGL_PLATFORM_WIN
#define IGL_NEWLINE "\r\n"
#else
#define IGL_NEWLINE "\n"
#endif

///--------------------------------------
/// MARK: - C++17 attributes (these should be inlined after all of our compilers are on c++17)

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(maybe_unused)
#define IGL_MAYBE_UNUSED [[maybe_unused]]
#endif
#endif

#if !defined(IGL_MAYBE_UNUSED)
#define IGL_MAYBE_UNUSED
#endif

///--------------------------------------
/// MARK: Visual Studio compatibility

// not all control paths return a value
#if defined(_MSC_VER) || defined(__GNUC__)
#define IGL_UNREACHABLE_RETURN(value) \
  IGL_DEBUG_ASSERT_NOT_REACHED();     \
  return value;
#else
#define IGL_UNREACHABLE_RETURN(value)
#endif // _MSC_VER

///--------------------------------------
/// MARK: Integrated profiling

/// Do not use Tracy for GPU profiling if IGL_WITH_TRACY isn't defined
#if defined(IGL_WITH_TRACY_GPU) && !defined(IGL_WITH_TRACY)
#undef IGL_WITH_TRACY_GPU
#endif

#if defined(IGL_WITH_TRACY) && defined(__cplusplus)
#include "tracy/Tracy.hpp"
// predefined 0xRGB colors for "heavy" point-of-interest operations
#define IGL_PROFILER_COLOR_WAIT 0xff0000
#define IGL_PROFILER_COLOR_SUBMIT 0x0000ff
#define IGL_PROFILER_COLOR_PRESENT 0x00ff00
#define IGL_PROFILER_COLOR_CREATE 0xff6600
#define IGL_PROFILER_COLOR_DESTROY 0xffa500
#define IGL_PROFILER_COLOR_TRANSITION 0xffffff
#define IGL_PROFILER_COLOR_UPDATE 0xffa500
#define IGL_PROFILER_COLOR_DRAW 0x00ff00

// GPU profiling macros
#if defined(IGL_WITH_TRACY_GPU)
#define IGL_PROFILER_ZONE_GPU_OGL(name) TracyGpuZone(name);
#define IGL_PROFILER_ZONE_GPU_COLOR_OGL(name, color) TracyGpuZoneC(name, color);

#define IGL_PROFILER_ZONE_GPU_VK(name, profilingContext, cmdBuffer) \
  TracyVkZone(profilingContext, cmdBuffer, name);
#define IGL_PROFILER_ZONE_GPU_COLOR_VK(name, profilingContext, cmdBuffer, color) \
  TracyVkZoneC(profilingContext, cmdBuffer, name, color);

#define IGL_PROFILER_ZONE_TRANSIENT_GPU_OGL(varname, name) \
  {                                                        \
    TracyGpuZoneTransientS(varname, name, 0, true);
#define IGL_PROFILER_ZONE_TRANSIENT_GPU_VK(profilingContext, varname, cmdBuffer, name) \
  {                                                                                    \
    TracyVkZoneTransientS(profilingContext, varname, cmdBuffer, name, 0, true);
#define IGL_PROFILER_ZONE_GPU_END() }
#else // IGL_WITH_TRACY_GPU

#define IGL_PROFILER_ZONE_GPU_OGL(name)
#define IGL_PROFILER_ZONE_GPU_COLOR_OGL(name, color)

#define IGL_PROFILER_ZONE_GPU_VK(name, profilingContext, cmdBuffer)
#define IGL_PROFILER_ZONE_GPU_COLOR_VK(name, profilingContext, cmdBuffer, color)

#define IGL_PROFILER_ZONE_TRANSIENT_GPU_OGL(varname, name)
#define IGL_PROFILER_ZONE_TRANSIENT_GPU_VK(profilingContext, varname, cmdBuffer, name)
#define IGL_PROFILER_ZONE_GPU_END()

#endif // IGL_WITH_TRACY_GPU

#define IGL_PROFILER_FUNCTION() ZoneScoped
#define IGL_PROFILER_FUNCTION_COLOR(color) ZoneScopedC(color)
#define IGL_PROFILER_ZONE(name, color) \
  {                                    \
    ZoneScopedC(color);                \
    ZoneName(name, strlen(name))
#define IGL_PROFILER_ZONE_END() }
#define IGL_PROFILER_THREAD(name) tracy::SetThreadName(name)
#define IGL_PROFILER_FRAME(name) FrameMarkNamed(name)

#else
#define IGL_PROFILER_ZONE_GPU_OGL(name)
#define IGL_PROFILER_ZONE_GPU_COLOR_OGL(name, color)

#define IGL_PROFILER_ZONE_GPU_VK(name, profilingContext, cmdBuffer)
#define IGL_PROFILER_ZONE_GPU_COLOR_VK(name, profilingContext, cmdBuffer, color)

#define IGL_PROFILER_ZONE_TRANSIENT_GPU_OGL(varname, name)
#define IGL_PROFILER_ZONE_TRANSIENT_GPU_VK(profilingContext, varname, cmdBuffer, name)
#define IGL_PROFILER_ZONE_GPU_END()

#define IGL_PROFILER_FUNCTION()
#define IGL_PROFILER_FUNCTION_COLOR(color)
#define IGL_PROFILER_ZONE(name, color) {
#define IGL_PROFILER_ZONE_END() }
#define IGL_PROFILER_THREAD(name)
#define IGL_PROFILER_FRAME(name)
#endif // IGL_WITH_TRACY

#if !defined(IGL_ENUM_TO_STRING)
#define IGL_ENUM_TO_STRING(enum, res) \
  case enum ::res:                    \
    return IGL_TO_STRING(res);
#endif // IGL_ENUM_TO_STRING

// Define this to 1 to enable shader dumping. Currently only the Vulkan Device supports it.
// It will dump the SPIR-V code into files in the specified path below in
// Device::createShaderModule(...)
#define IGL_SHADER_DUMP 0

// Replace IGL_SHADER_DUMP_PATH with your own path according to the platform
// Ex. for Android your filepath should be specific to the package name:
// "/sdcard/Android/data/<packageName>/files/"
// @fb-only An example path used with the CodecAvatars app:
// @fb-only "/sdcard/Android/data/com.meta.ar.codecavatars/files/"
#define IGL_SHADER_DUMP_PATH "/path/to/output/file/"
