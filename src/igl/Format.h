/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// On iOS for Xcode 16.3, std::to_chars is not available. To avoid an error, we should not include
// std::format, and switch to using fmt/format instead. This define is used in conjunction with
// others below, as this is not the only reason not to include std::format.
//
// Note: the _LIBCPP_AVAILABILITY_HAS_TO_CHARS_FLOATING_POINT is defined in libc++'s <__config>
// header, which is includes by all other headers. We include <cassert> and <utility> above
#define IGL_INCLUDE_FORMAT (!IGL_PLATFORM_APPLE || _LIBCPP_AVAILABILITY_HAS_TO_CHARS_FLOATING_POINT)

// libc++'s implementation of std::format has a large binary size impact
// (https://github.com/llvm/llvm-project/issues/64180), so avoid it on Android.
#if defined(__cpp_lib_format) && (!defined(__ANDROID__) && IGL_INCLUDE_FORMAT)
#include <format>
#define IGL_FORMAT std::format
#else
#include <fmt/format.h>
#define IGL_FORMAT fmt::format
#endif // __cpp_lib_format
