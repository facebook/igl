/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define IGL_COMMON_SKIP_CHECK
#include <igl/Assert.h>

// ----------------------------------------------------------------------------

namespace {
IGLErrorHandlerFunc& getDebugAbortListener() {
  static IGLErrorHandlerFunc sListener = nullptr;
  return sListener;
}
} // namespace

IGL_API void iglSetDebugAbortListener(IGLErrorHandlerFunc listener) {
  getDebugAbortListener() = listener;
}

IGL_API IGLErrorHandlerFunc iglGetDebugAbortListener(void) {
  return getDebugAbortListener();
}

namespace igl {

// Toggle debug break on/off at runtime
#if IGL_DEBUG
static bool debugBreakEnabled = true;
#else
static bool debugBreakEnabled = false;
#endif // IGL_DEBUG

bool isDebugBreakEnabled() {
  return debugBreakEnabled;
}

void setDebugBreakEnabled(bool enabled) {
  debugBreakEnabled = enabled;
}

} // namespace igl

#if IGL_PLATFORM_APPLE || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
#define IGL_DEBUGGER_SIGTRAP 1
#include <csignal>
#elif IGL_PLATFORM_WINDOWS
#include <windows.h>
#include <igl/Log.h>
#endif

void iglDebugBreak() {
#if IGL_DEBUG_BREAK_ENABLED
  if (igl::isDebugBreakEnabled()) {
#ifdef IGL_DEBUGGER_SIGTRAP
    raise(SIGTRAP);
#elif IGL_PLATFORM_WINDOWS
    if (!IsDebuggerPresent()) {
      IGLLog(IGLLogError, "[IGL] Skipping debug break - debugger not present");
      return;
    }
    __debugbreak();
#else
#warning "IGLDebugBreak() not implemented on this platform"
#endif
  }
#endif // IGL_DEBUG_BREAK_ENABLED
}

// ----------------------------------------------------------------------------

namespace {
IGLErrorHandlerFunc& getSoftErrorHandler() {
  static IGLErrorHandlerFunc sHandler = nullptr;
  return sHandler;
}
} // namespace

IGL_API void iglSetSoftErrorHandler(IGLErrorHandlerFunc handler) {
  getSoftErrorHandler() = handler;
}

IGL_API IGLErrorHandlerFunc iglGetSoftErrorHandler(void) {
  return getSoftErrorHandler();
}
