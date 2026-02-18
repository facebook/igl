/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdarg>
#include <igl/Core.h>

// At startup, install an Apple-specific log handler, so logging shows up in Console.app / os_log:
// ```
// IGLLogSetHandler(IGLAppleLogDefaultHandler);
// ```

extern "C" {

// NOLINTNEXTLINE(readability-identifier-naming)
IGL_API int IGLAppleLogDefaultHandler(IGLLogLevel logLevel,
                                      const char* IGL_RESTRICT format,
                                      va_list ap);
}
