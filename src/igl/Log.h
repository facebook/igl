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

#ifdef LOG_INFO
#undef LOG_INFO
#endif

#ifdef LOG_WARNING
#undef LOG_WARNING
#endif

#ifdef LOG_ERROR
#undef LOG_ERROR
#endif

enum class IGLLogLevel {
  LOG_ERROR = 1,
  LOG_WARNING,
  LOG_INFO,
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
#define IGL_LOG_ERROR(format, ...)                                        \
  IGLLog(IGLLogLevel::LOG_ERROR, "[IGL] Error in (%s).\n", IGL_FUNCTION); \
  IGLLog(IGLLogLevel::LOG_ERROR, (format), ##__VA_ARGS__)
#define IGL_LOG_ERROR_ONCE(format, ...) IGLLogOnce(IGLLogLevel::LOG_ERROR, (format), ##__VA_ARGS__)

#define IGL_LOG_INFO(format, ...) IGLLog(IGLLogLevel::LOG_INFO, (format), ##__VA_ARGS__)
#define IGL_LOG_INFO_ONCE(format, ...) IGLLogOnce(IGLLogLevel::LOG_INFO, (format), ##__VA_ARGS__)
#define IGL_DEBUG_LOG(format, ...) IGLLog(IGLLogLevel::LOG_INFO, (format), ##__VA_ARGS__)
#else
#define IGL_LOG_ERROR(format, ...) static_cast<void>(0)
#define IGL_LOG_ERROR_ONCE(format, ...) static_cast<void>(0)
#define IGL_LOG_INFO(format, ...) static_cast<void>(0)
#define IGL_LOG_INFO_ONCE(format, ...) static_cast<void>(0)
#define IGL_DEBUG_LOG(format, ...) static_cast<void>(0)
#endif
