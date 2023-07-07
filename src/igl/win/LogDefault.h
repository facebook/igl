/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdarg>
#include <igl/Common.h>

#ifdef __cplusplus
extern "C" {
#endif

// At startup, install a Win-specific log handler, so logging shows up in Visual Studio debugger:
// ```
// IGLLogSetHandler(IGLWinLogDefaultHandler);
// ```
IGL_API int IGLWinLogDefaultHandler(IGLLogLevel logLevel,
                                    const char* IGL_RESTRICT format,
                                    va_list ap);

#ifdef __cplusplus
}
#endif
