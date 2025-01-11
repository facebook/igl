/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#if !defined(IGL_CMAKE_BUILD)
#include <folly/CppAttributes.h>
#include <folly/Portability.h>
#include <folly/Preprocessor.h>
#else

#include <utility>

// folly/Portability.h

#if __has_include(<features.h>)
#include <features.h> // @manual
#endif

#if __has_include(<bits/c++config.h>)
#include <bits/c++config.h> // @manual
#endif

#if __has_include(<__config>)
#include <__config> // @manual
#endif

#ifdef __ANDROID__
#include <android/api-level.h> // @manual
#endif

#ifdef __APPLE__
#include <Availability.h> // @manual
#include <AvailabilityMacros.h> // @manual
#include <TargetConditionals.h> // @manual
#endif

#ifndef __has_extension
#define FOLLY_HAS_EXTENSION(x) 0
#else
#define FOLLY_HAS_EXTENSION(x) __has_extension(x)
#endif

#if defined(__has_feature)
#define FOLLY_HAS_FEATURE(...) __has_feature(__VA_ARGS__)
#else
#define FOLLY_HAS_FEATURE(...) 0
#endif

#if FOLLY_HAS_EXTENSION(nullability)
#define FOLLY_NULLABLE                                   \
  FOLLY_PUSH_WARNING                                     \
  FOLLY_CLANG_DISABLE_WARNING("-Wnullability-extension") \
  _Nullable FOLLY_POP_WARNING
#define FOLLY_NONNULL                                    \
  FOLLY_PUSH_WARNING                                     \
  FOLLY_CLANG_DISABLE_WARNING("-Wnullability-extension") \
  _Nonnull FOLLY_POP_WARNING
#else
#define FOLLY_NULLABLE
#define FOLLY_NONNULL
#endif

// Generalize warning push/pop.
#if defined(__GNUC__) || defined(__clang__)
// Clang & GCC
#define FOLLY_PUSH_WARNING _Pragma("GCC diagnostic push")
#define FOLLY_POP_WARNING _Pragma("GCC diagnostic pop")
#define FOLLY_GNU_DISABLE_WARNING_INTERNAL2(warningName) #warningName
#define FOLLY_GNU_DISABLE_WARNING(warningName) \
  _Pragma(FOLLY_GNU_DISABLE_WARNING_INTERNAL2(GCC diagnostic ignored warningName))
#ifdef __clang__
#define FOLLY_CLANG_DISABLE_WARNING(warningName) FOLLY_GNU_DISABLE_WARNING(warningName)
#define FOLLY_GCC_DISABLE_WARNING(warningName)
#else
#define FOLLY_CLANG_DISABLE_WARNING(warningName)
#define FOLLY_GCC_DISABLE_WARNING(warningName) FOLLY_GNU_DISABLE_WARNING(warningName)
#endif
#define FOLLY_MSVC_DISABLE_WARNING(warningNumber)
#elif defined(_MSC_VER)
#define FOLLY_PUSH_WARNING __pragma(warning(push))
#define FOLLY_POP_WARNING __pragma(warning(pop))
// Disable the GCC warnings.
#define FOLLY_GNU_DISABLE_WARNING(warningName)
#define FOLLY_GCC_DISABLE_WARNING(warningName)
#define FOLLY_CLANG_DISABLE_WARNING(warningName)
#define FOLLY_MSVC_DISABLE_WARNING(warningNumber) __pragma(warning(disable : warningNumber))
#else
#define FOLLY_PUSH_WARNING
#define FOLLY_POP_WARNING
#define FOLLY_GNU_DISABLE_WARNING(warningName)
#define FOLLY_GCC_DISABLE_WARNING(warningName)
#define FOLLY_CLANG_DISABLE_WARNING(warningName)
#define FOLLY_MSVC_DISABLE_WARNING(warningNumber)
#endif

/**
 * FB_ANONYMOUS_VARIABLE(str) introduces an identifier starting with
 * str and ending with a number that varies with the line.
 */
#ifndef FB_ANONYMOUS_VARIABLE
#define FB_CONCATENATE_IMPL(s1, s2) s1##s2
#define FB_CONCATENATE(s1, s2) FB_CONCATENATE_IMPL(s1, s2)
#ifdef __COUNTER__
// Modular builds build each module with its own preprocessor state, meaning
// `__COUNTER__` no longer provides a unique number across a TU.  Instead of
// calling back to just `__LINE__`, use a mix of `__COUNTER__` and `__LINE__`
// to try provide as much uniqueness as possible.
#if FOLLY_HAS_FEATURE(modules)
#define FB_ANONYMOUS_VARIABLE(str) \
  FB_CONCATENATE(FB_CONCATENATE(FB_CONCATENATE(str, __COUNTER__), _), __LINE__)
#else
#define FB_ANONYMOUS_VARIABLE(str) FB_CONCATENATE(str, __COUNTER__)
#endif
#else
#define FB_ANONYMOUS_VARIABLE(str) FB_CONCATENATE(str, __LINE__)
#endif
#endif

#endif // IGL_CMAKE_BUILD

#define IGL_NULLABLE FOLLY_NULLABLE
#define IGL_NONNULL FOLLY_NONNULL
