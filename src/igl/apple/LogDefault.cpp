/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/apple/LogDefault.h>

#if !IGL_PLATFORM_APPLE
#error This file should only be compiled on Apple targets
#endif // !IGL_PLATFORM_APPLE

#include <cstdio>
#include <os/log.h>

IGL_API int IGLAppleLogDefaultHandler(IGLLogLevel logLevel,
                                      const char* IGL_RESTRICT format,
                                      va_list ap) {
  // Format the message into a buffer for os_log
  char buf[4096];
  int result = vsnprintf(buf, sizeof(buf), format, ap);

  switch (logLevel) {
  case IGLLogError:
    os_log_error(OS_LOG_DEFAULT, "[IGL] %{public}s", buf);
    break;
  case IGLLogWarning:
    os_log(OS_LOG_DEFAULT, "[IGL] %{public}s", buf);
    break;
  case IGLLogInfo:
    os_log_info(OS_LOG_DEFAULT, "[IGL] %{public}s", buf);
    break;
  }

  return result;
}
