/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define IGL_COMMON_SKIP_CHECK

#include <array>
#include <cstdarg>
#include <cstdio>
#include <igl/Core.h>
#include <mutex>
#include <string>
#include <unordered_set>

#if IGL_PLATFORM_ANDROID
#include <igl/android/LogDefault.h>
#elif IGL_PLATFORM_WINDOWS
#include <igl/win/LogDefault.h>
#endif

// Returns a "handle" (i.e. ptr to ptr) to func
static IGLLogHandlerFunc* GetHandle() {
#if IGL_PLATFORM_ANDROID
  static IGLLogHandlerFunc sHandler = IGLAndroidLogDefaultHandler;
#elif IGL_PLATFORM_WINDOWS
  static IGLLogHandlerFunc sHandler = IGLWinLogDefaultHandler;
#else
  static IGLLogHandlerFunc sHandler = IGLLogDefaultHandler;
#endif
  return &sHandler;
}

IGL_API int IGLLog(IGLLogLevel logLevel, const char* IGL_RESTRICT format, ...) {
  va_list ap;
  va_start(ap, format);
  const int result = IGLLogV(logLevel, format, ap);
  va_end(ap);
  return result;
}

IGL_API int IGLLogOnce(IGLLogLevel logLevel, const char* IGL_RESTRICT format, ...) {
  static std::mutex s_loggedMessagesMutex;
  static std::unordered_set<std::string> s_loggedMessages;

  va_list ap, apCopy;
  va_start(ap, format);
  va_copy(apCopy, ap); // make a copy for later passing to IGLLogV()

  constexpr size_t bufferLength = 256;
  std::array<char, bufferLength> buffer{};
  FOLLY_PUSH_WARNING
  FOLLY_GNU_DISABLE_WARNING("-Wformat-nonliteral")
  int result = vsnprintf(buffer.data(), bufferLength, format, ap);
  FOLLY_POP_WARNING
  va_end(ap);

  const std::string msg(buffer.data());
  {
    const std::lock_guard<std::mutex> guard(s_loggedMessagesMutex);
    if (s_loggedMessages.count(msg) == 0) {
      result = IGLLogV(logLevel, format, apCopy);
      s_loggedMessages.insert(msg);
    }
  }
  va_end(apCopy);

  return result;
}

IGL_API int IGLLogV(IGLLogLevel logLevel, const char* IGL_RESTRICT format, va_list ap) {
  return (*GetHandle())(logLevel, format, ap);
}

IGL_API int IGLLogDefaultHandler(IGLLogLevel /*logLevel*/,
                                 const char* IGL_RESTRICT format,
                                 va_list ap) {
  FOLLY_PUSH_WARNING
  FOLLY_GNU_DISABLE_WARNING("-Wformat-nonliteral")
  return vfprintf(stderr, format, ap);
  FOLLY_POP_WARNING
}

IGL_API void IGLLogSetHandler(IGLLogHandlerFunc handler) {
  *GetHandle() = handler;
}

IGL_API IGLLogHandlerFunc IGLLogGetHandler() {
  return *GetHandle();
}
