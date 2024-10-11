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

#include <igl/Macros.h>

///--------------------------------------
/// MARK: - Assert

// When a runtime assert fails, it will:
//
// * trap in the debugger
// * logs to console and/or debugger console: failing expression, function signature, file/line
// * allows you to continue execution during debugging after a failing assert, instead of exiting
//
// ## IGL_DEBUG_ASSERT vs IGL_DEBUG_VERIFY/IGL_DEBUG_VERIFY_NOT
//
// Use IGL_DEBUG_ASSERT for debug-only assertions. On release builds, the expressions
// expand to no-op's, so no perf penalty. IGL_DEBUG_ASSERT logs failed the  expression to
// console. To customize, provide a format argment with printf semantics:
//
//   int i = 42;
//   auto p = std::make_shared<int>(i);
//   IGL_DEBUG_ASSERT(p);
//   IGL_DEBUG_ASSERT(*p == i, "*p is wrong value. Got %d. Expected %d.", *p, i);
//
// Use IGL_DEBUG_VERIFY and IGL_DEBUG_VERIFY_NOT to evaluate expressions and catch asserts on debug
// builds. Typically, you'd wrap an expressions inside an `if` statement with IGL_DEBUG_VERIFY.
// IGL_DEBUG_VERIFY_NOT is for `if` statements that check if an error condition is true. That way,
// you can catch assertions on debug builds. On release builds, there's no overhead; they simply
// expand to the original expression:
//
//   FILE* fp = std::fopen("test.txt", "r");
//   if (IGL_DEBUG_VERIFY(fp)) {
//     printf("Success!\n");
//   } else {
//     printf("Failure!\n");
//   }
//
//   void Foo::initialize() {
//     if (IGL_DEBUG_VERIFY_NOT(initialized_)) {
//       // Initialize should only be called once!
//       return;
//     }
//   }
//

#include <igl/Log.h>

#if IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS)
#define IGL_VERIFY_ENABLED 1
#else
#define IGL_VERIFY_ENABLED 0
#endif

#if IGL_DEBUG
#define IGL_DEBUG_BREAK_ENABLED 1
#else
#define IGL_DEBUG_BREAK_ENABLED 0
#endif

IGL_API void _IGLDebugBreak();

namespace igl {
bool isDebugBreakEnabled();
void setDebugBreakEnabled(bool enabled);

template<typename T>
[[nodiscard]] static inline const T& _IGLVerify(const T& cond,
                                                [[maybe_unused]] const char* reason,
                                                [[maybe_unused]] const char* func,
                                                [[maybe_unused]] const char* file,
                                                [[maybe_unused]] int line,
                                                [[maybe_unused]] const char* format,
                                                ...) {
#if IGL_VERIFY_ENABLED
  if (!cond) {
    IGLLog(IGLLogError, "[IGL] %s in '%s' (%s:%d): ", reason, func, file, line);
    va_list ap;
    va_start(ap, format);
    IGLLogV(IGLLogError, format, ap);
    va_end(ap);
    IGLLog(IGLLogError, IGL_NEWLINE);
    _IGLDebugBreak();
  }
#endif // IGL_VERIFY_ENABLED
  return cond;
}
} // namespace igl

#if IGL_VERIFY_ENABLED

#define _IGL_DEBUG_ABORT(cond, format, ...) \
  (void)::igl::_IGLVerify(                  \
      cond, "Abort requested", IGL_FUNCTION, __FILE__, __LINE__, (format), ##__VA_ARGS__)
#define _IGL_DEBUG_ASSERT(cond, format, ...) \
  (void)::igl::_IGLVerify(                   \
      cond, "Assert failed", IGL_FUNCTION, __FILE__, __LINE__, (format), ##__VA_ARGS__)

#define _IGL_DEBUG_VERIFY(cond, format, ...) \
  ::igl::_IGLVerify(                         \
      (cond), "Verify failed", IGL_FUNCTION, __FILE__, __LINE__, (format), ##__VA_ARGS__)
#define _IGL_DEBUG_VERIFY_NOT(cond, format, ...) \
  (!::igl::_IGLVerify(                           \
      0 == !!(cond), "Verify failed", IGL_FUNCTION, __FILE__, __LINE__, (format), ##__VA_ARGS__))

#else

#define _IGL_DEBUG_ABORT(cond, format, ...) static_cast<void>(0)
#define _IGL_DEBUG_ASSERT(cond, format, ...) static_cast<void>(0)
#define _IGL_DEBUG_VERIFY(cond, format, ...) (cond)
#define _IGL_DEBUG_VERIFY_NOT(cond, format, ...) (cond)

#endif // IGL_VERIFY_ENABLED

#define IGL_DEBUG_ABORT(format, ...) _IGL_DEBUG_ABORT(false, (format), ##__VA_ARGS__)

#define _IGL_DEBUG_ASSERT_0(cond) _IGL_DEBUG_ASSERT(cond, #cond)
#define _IGL_DEBUG_ASSERT_1(cond, format, ...) _IGL_DEBUG_ASSERT(cond, (format), ##__VA_ARGS__)
// Supported variations:
// IGL_DEBUG_ASSERT(cond)
// IGL_DEBUG_ASSERT(cond, format, ...)
#define IGL_DEBUG_ASSERT(...) \
  _IGL_CALL(IGL_CONCAT(_IGL_DEBUG_ASSERT_, _IGL_HAS_COMMA(__VA_ARGS__)), _IGL_ECHO((__VA_ARGS__)))

#define _IGL_DEBUG_VERIFY_0(cond) _IGL_DEBUG_VERIFY(cond, #cond)
#define _IGL_DEBUG_VERIFY_1(cond, format, ...) _IGL_DEBUG_VERIFY(cond, (format), ##__VA_ARGS__)
// Supported variations:
// IGL_DEBUG_VERIFY(cond)
// IGL_DEBUG_VERIFY(cond, format, ...)
#define IGL_DEBUG_VERIFY(...) \
  _IGL_CALL(IGL_CONCAT(_IGL_DEBUG_VERIFY_, _IGL_HAS_COMMA(__VA_ARGS__)), _IGL_ECHO((__VA_ARGS__)))

#define _IGL_DEBUG_VERIFY_NOT_0(cond) _IGL_DEBUG_VERIFY_NOT(cond, "!(" #cond ")")
#define _IGL_DEBUG_VERIFY_NOT_1(cond, format, ...) \
  _IGL_DEBUG_VERIFY_NOT(cond, (format), ##__VA_ARGS__)
// Supported variations:
// IGL_DEBUG_VERIFY_NOT(cond)
// IGL_DEBUG_VERIFY_NOT(cond, format, ...)
#define IGL_DEBUG_VERIFY_NOT(...)                                            \
  _IGL_CALL(IGL_CONCAT(_IGL_DEBUG_VERIFY_NOT_, _IGL_HAS_COMMA(__VA_ARGS__)), \
            _IGL_ECHO((__VA_ARGS__)))

#define IGL_DEBUG_ASSERT_NOT_REACHED() IGL_DEBUG_ABORT("Code should NOT be reached")
#define IGL_DEBUG_ASSERT_NOT_IMPLEMENTED() IGL_DEBUG_ABORT("Code NOT implemented")

///--------------------------------------
/// MARK: - Custom

#define IGL_ERROR_CATEGORY "IGL"

using IGLSoftErrorFunc = void (*)(const char* file,
                                  const char* func,
                                  int line,
                                  const char* category,
                                  const char* format,
                                  ...);
IGL_API void IGLSetSoftErrorHandler(IGLSoftErrorFunc handler);
IGL_API IGLSoftErrorFunc IGLGetSoftErrorHandler(void);
IGL_API void IGLSoftError(const char* file,
                          const char* func,
                          int line,
                          const char* category,
                          const char* format,
                          ...);
namespace igl {
template<typename T, typename... Args>
[[nodiscard]] static inline const T& _IGLSoftError(const T& cond,
                                                   [[maybe_unused]] const char* reason,
                                                   [[maybe_unused]] const char* func,
                                                   [[maybe_unused]] const char* file,
                                                   [[maybe_unused]] int line,
                                                   [[maybe_unused]] const char* format,
                                                   [[maybe_unused]] const Args&... args) {
#if IGL_VERIFY_ENABLED
  const auto& verifiedCond = _IGLVerify(cond, reason, func, file, line, format, args...);
#else
  const auto& verifiedCond = cond;
#endif // IGL_VERIFY_ENABLED

#if IGL_SOFT_ERROR_ENABLED
  if (!verifiedCond) {
    IGLGetSoftErrorHandler()(file, func, line, IGL_ERROR_CATEGORY, format, args...);
  }
#endif // IGL_SOFT_ERROR_ENABLED

  return verifiedCond;
}
} // namespace igl

#if IGL_SOFT_ERROR_ENABLED

#define _IGL_SOFT_ERROR(cond, format, ...) \
  (void)::igl::_IGLSoftError(              \
      cond, "Soft error", IGL_FUNCTION, __FILE__, __LINE__, (format), ##__VA_ARGS__)
#define _IGL_SOFT_ASSERT(cond, format, ...) \
  (void)::igl::_IGLSoftError(               \
      cond, "Soft assert failed", IGL_FUNCTION, __FILE__, __LINE__, (format), ##__VA_ARGS__)

#define _IGL_SOFT_VERIFY(cond, format, ...) \
  ::igl::_IGLSoftError(                     \
      (cond), "Soft verify failed", IGL_FUNCTION, __FILE__, __LINE__, (format), ##__VA_ARGS__)
#define _IGL_SOFT_VERIFY_NOT(cond, format, ...) \
  (!::igl::_IGLSoftError(0 == !!(cond),         \
                         "Soft verify failed",  \
                         IGL_FUNCTION,          \
                         __FILE__,              \
                         __LINE__,              \
                         (format),              \
                         ##__VA_ARGS__))

#else

#define _IGL_SOFT_ERROR(cond, format, ...) static_cast<void>(0)
#define _IGL_SOFT_ASSERT(cond, format, ...) static_cast<void>(0)
#define _IGL_SOFT_VERIFY(cond, format, ...) (cond)
#define _IGL_SOFT_VERIFY_NOT(cond, format, ...) (cond)

#endif // IGL_SOFT_ERROR_ENABLED

#define IGL_SOFT_ERROR(format, ...) _IGL_SOFT_ERROR(false, (format), ##__VA_ARGS__)

#define _IGL_SOFT_ASSERT_0(cond) _IGL_SOFT_ASSERT(cond, #cond)
#define _IGL_SOFT_ASSERT_1(cond, format, ...) _IGL_SOFT_ASSERT(cond, (format), ##__VA_ARGS__)
// Supported variations:
// IGL_SOFT_ASSERT(cond)
// IGL_SOFT_ASSERT(cond, format, ...)
#define IGL_SOFT_ASSERT(...) \
  _IGL_CALL(IGL_CONCAT(_IGL_SOFT_ASSERT_, _IGL_HAS_COMMA(__VA_ARGS__)), _IGL_ECHO((__VA_ARGS__)))

#define _IGL_SOFT_VERIFY_0(cond) _IGL_SOFT_VERIFY(cond, #cond)
#define _IGL_SOFT_VERIFY_1(cond, format, ...) _IGL_SOFT_VERIFY(cond, (format), ##__VA_ARGS__)
// Supported variations:
// IGL_SOFT_VERIFY(cond)
// IGL_SOFT_VERIFY(cond, format, ...)
#define IGL_SOFT_VERIFY(...) \
  _IGL_CALL(IGL_CONCAT(_IGL_SOFT_VERIFY_, _IGL_HAS_COMMA(__VA_ARGS__)), _IGL_ECHO((__VA_ARGS__)))

#define _IGL_SOFT_VERIFY_NOT_0(cond) _IGL_SOFT_VERIFY_NOT(cond, "!(" #cond ")")
#define _IGL_SOFT_VERIFY_NOT_1(cond, format, ...) \
  _IGL_SOFT_VERIFY_NOT(cond, (format), ##__VA_ARGS__)
// Supported variations:
// IGL_SOFT_VERIFY_NOT(cond)
// IGL_SOFT_VERIFY_NOT(cond, format, ...)
#define IGL_SOFT_VERIFY_NOT(...)                                            \
  _IGL_CALL(IGL_CONCAT(_IGL_SOFT_VERIFY_NOT_, _IGL_HAS_COMMA(__VA_ARGS__)), \
            _IGL_ECHO((__VA_ARGS__)))
