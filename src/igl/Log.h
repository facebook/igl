/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#if !defined(IGL_COMMON_H) && !defined(IGL_COMMON_SKIP_CHECK)
#error "Please, include <igl/Common.h> instead"
#endif

#include <cstdarg>
#include <igl/Macros.h>

enum IGLLogLevel {
  // Was previously LOG_ERROR.
  IGLLogError = 1,

  // Was previously LOG_WARNING.
  IGLLogWarning,

  // Was previously LOG_INFO.
  IGLLogInfo,
};

///--------------------------------------
/// MARK: - Logging

IGL_API int IGLLog(IGLLogLevel logLevel, const char* IGL_RESTRICT format, ...);
IGL_API int IGLLogOnce(IGLLogLevel logLevel, const char* IGL_RESTRICT format, ...);
IGL_API int IGLLogV(IGLLogLevel logLevel, const char* IGL_RESTRICT format, va_list arguments);

IGL_API int IGLLogDefaultHandler(IGLLogLevel logLevel, const char* IGL_RESTRICT format, va_list ap);

///--------------------------------------
/// MARK: - Custom log handler

using IGLLogHandlerFunc = int (*)(IGLLogLevel logLevel,
                                  const char* IGL_RESTRICT format,
                                  va_list ap);
IGL_API void IGLLogSetHandler(IGLLogHandlerFunc handler);
IGL_API IGLLogHandlerFunc IGLLogGetHandler(void);

///--------------------------------------
/// MARK: - Macros

// Debug logging
#if IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS)
#define IGL_LOG_ERROR(format, ...)                             \
  IGLLog(IGLLogError, "[IGL] Error in (%s).\n", IGL_FUNCTION); \
  IGLLog(IGLLogError, (format), ##__VA_ARGS__)
#define IGL_LOG_ERROR_ONCE(format, ...) IGLLogOnce(IGLLogError, (format), ##__VA_ARGS__)

#define IGL_LOG_INFO(format, ...) IGLLog(IGLLogInfo, (format), ##__VA_ARGS__)
#define IGL_LOG_INFO_ONCE(format, ...) IGLLogOnce(IGLLogInfo, (format), ##__VA_ARGS__)
#define IGL_LOG_DEBUG(format, ...) IGLLog(IGLLogInfo, (format), ##__VA_ARGS__)
#else
#define IGL_LOG_ERROR(format, ...) static_cast<void>(0)
#define IGL_LOG_ERROR_ONCE(format, ...) static_cast<void>(0)
#define IGL_LOG_INFO(format, ...) static_cast<void>(0)
#define IGL_LOG_INFO_ONCE(format, ...) static_cast<void>(0)
#define IGL_LOG_DEBUG(format, ...) static_cast<void>(0)
#endif
