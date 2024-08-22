/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/android/LogDefault.h>

#include <android/log.h>

namespace {

android_LogPriority logPriorityFromLogLevel(IGLLogLevel logLevel) noexcept {
  switch (logLevel) {
  case IGLLogError:
    return ANDROID_LOG_ERROR;
  case IGLLogWarning:
    return ANDROID_LOG_WARN;
  case IGLLogInfo:
    return ANDROID_LOG_INFO;
  }
}

} // namespace

IGL_API int IGLAndroidLogDefaultHandler(IGLLogLevel logLevel,
                                        const char* IGL_RESTRICT format,
                                        va_list ap) {
  return __android_log_vprint(logPriorityFromLogLevel(logLevel), "IGL", format, ap);
}
