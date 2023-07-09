/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// clang-format off
// Defining the IGL_DEBUG only in cases where the build_mode_opt is not defined.
#if !defined(IGL_BUILD_MODE_OPT)
  #if !defined(NDEBUG) && (defined DEBUG || defined _DEBUG || defined __DEBUG)
    #define IGL_DEBUG 1
  #else
    #define IGL_DEBUG 0
  #endif
#elif !defined(NDEBUG)
  #define IGL_DEBUG 1
#else
  #define IGL_DEBUG 0
#endif
// clang-format on

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

// not all control paths return a value
#if defined(_MSC_VER)
#define IGL_UNREACHABLE_RETURN(value) \
  IGL_ASSERT_NOT_REACHED();           \
  return value;
#else
#define IGL_UNREACHABLE_RETURN(value)
#endif // _MSC_VER

#if defined(IGL_WITH_TRACY) && defined(__cplusplus)
#include "tracy/Tracy.hpp"
// predefined 0xRGB colors for "heavy" point-of-interest operations
#define IGL_PROFILER_COLOR_WAIT 0xff0000
#define IGL_PROFILER_COLOR_SUBMIT 0x0000ff
#define IGL_PROFILER_COLOR_PRESENT 0x00ff00
#define IGL_PROFILER_COLOR_CREATE 0xff6600
#define IGL_PROFILER_COLOR_DESTROY 0xffa500
#define IGL_PROFILER_COLOR_TRANSITION 0xffffff
//
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
#define IGL_PROFILER_FUNCTION()
#define IGL_PROFILER_FUNCTION_COLOR(color)
#define IGL_PROFILER_ZONE(name, color) {
#define IGL_PROFILER_ZONE_END() }
#define IGL_PROFILER_THREAD(name)
#define IGL_PROFILER_FRAME(name)
#endif // IGL_WITH_TRACY
