/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdarg>
#include <igl/Common.h>

// At startup, install an Android-specific log handler, so logging shows up in `adb logcat`:
// ```
// IGLLogSetHandler(IGLAndroidLogDefaultHandler);
// ```

extern "C" {

// NOLINTNEXTLINE(readability-identifier-naming)
IGL_API int IGLAndroidLogDefaultHandler(IGLLogLevel logLevel,
                                        const char* IGL_RESTRICT format,
                                        va_list ap);
}
