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
IGLErrorHandlerFunc& GetDebugAbortListener() {
  static IGLErrorHandlerFunc sListener = nullptr;
  return sListener;
}
} // namespace

IGL_API void IGLSetDebugAbortListener(IGLErrorHandlerFunc listener) {
  GetDebugAbortListener() = listener;
}

IGL_API IGLErrorHandlerFunc IGLGetDebugAbortListener(void) {
  return GetDebugAbortListener();
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
#include <igl/Log.h>
#include <windows.h>
#endif

void _IGLDebugBreak() {
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
IGLErrorHandlerFunc& GetSoftErrorHandler() {
  static IGLErrorHandlerFunc sHandler = nullptr;
  return sHandler;
}
} // namespace

IGL_API void IGLSetSoftErrorHandler(IGLErrorHandlerFunc handler) {
  GetSoftErrorHandler() = handler;
}

IGL_API IGLErrorHandlerFunc IGLGetSoftErrorHandler(void) {
  return GetSoftErrorHandler();
}
