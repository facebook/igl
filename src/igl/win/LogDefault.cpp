/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/win/LogDefault.h>

#if !IGL_PLATFORM_WINDOWS
#error This file should only be compiled on Windows targets
#endif // !IGL_PLATFORM_WINDOWS

#include <stdio.h>
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

IGL_API int IGLWinLogDefaultHandler(IGLLogLevel logLevel,
                                    const char* IGL_RESTRICT format,
                                    va_list ap) {
  if (IsDebuggerPresent()) {
    char str[10240];
    const size_t kStrSize = sizeof(str) / sizeof(str[0]);
    int result = vsnprintf(str, kStrSize, format, ap);

    OutputDebugStringA(str);

    if (result >= kStrSize) {
      OutputDebugStringA("(...message truncated.)" IGL_NEWLINE);
    }
  }

  // Log to non-debugger console
  return IGLLogDefaultHandler(logLevel, format, ap);
}
