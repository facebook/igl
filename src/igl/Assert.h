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
// ## IGL_ASSERT/IGL_ASSERT_MSG vs IGL_VERIFY/IGL_UNEXPECTED
//
// Use IGL_ASSERT for debug-only assertions. On release builds, the expressions
// expand to no-op's, so no perf penalty. IGL_ASSERT logs failed expression to
// console. To customize, use IGL_ASSERT_MSG which supports printf semantics:
//
//   int i = 42;
//   auto p = std::make_shared<int>(i);
//   IGL_ASSERT(p);
//   IGL_ASSERT_MSG(*p == i, "*p is wrong value. Got %d. Expected %d.", *p, i);
//
// Use IGL_VERIFY and IGL_UNEXPECTED to evaluate expressions and catch asserts on debug builds.
// Typically, you'd wrap an expressions inside an `if` statement with IGL_VERIFY. IGL_UNEXPECTED
// is for `if` statements that check if an error condition is true. That way, you can catch
// assertions on debug builds. On release builds, there's no overhead; they simply expand to the
// original expression:
//
//   FILE* fp = std::fopen("test.txt", "r");
//   if (IGL_VERIFY(fp)) {
//     printf("Success!\n");
//   } else {
//     printf("Failure!\n");
//   }
//
//   void Foo::initialize() {
//     if (IGL_UNEXPECTED(initialized_)) {
//       // Initialize should only be called once!
//       return;
//     }
//   }
//

#include <igl/Log.h>

IGL_API void _IGLDebugBreak();

namespace igl {
bool isDebugBreakEnabled();
void setDebugBreakEnabled(bool enabled);

template<typename T>
static inline const T& _IGLVerify(const T& cond,
                                  const char* func,
                                  const char* file,
                                  int line,
                                  const char* format,
                                  ...) {
  if (!cond) {
    IGLLog(IGLLogLevel::LOG_ERROR, "[IGL] Assert failed in '%s' (%s:%d): ", func, file, line);
    va_list ap;
    va_start(ap, format);
    IGLLogV(IGLLogLevel::LOG_ERROR, format, ap);
    va_end(ap);
    IGLLog(IGLLogLevel::LOG_ERROR, IGL_NEWLINE);
    if (igl::isDebugBreakEnabled()) {
      _IGLDebugBreak();
    }
  }
  return cond;
}
} // namespace igl

#if IGL_DEBUG

#define IGL_UNEXPECTED(cond) \
  (!::igl::_IGLVerify(0 == !!(cond), IGL_FUNCTION, __FILE__, __LINE__, #cond))
#define IGL_UNEXPECTED_MSG(cond, format, ...) \
  (!::igl::_IGLVerify(0 == !!(cond), IGL_FUNCTION, __FILE__, __LINE__, (format), ##__VA_ARGS__))

#define IGL_VERIFY(cond) ::igl::_IGLVerify((cond), IGL_FUNCTION, __FILE__, __LINE__, #cond)
#define IGL_ASSERT(cond) (void)IGL_VERIFY(cond)
#define IGL_ASSERT_MSG(cond, format, ...) \
  (void)::igl::_IGLVerify((cond), IGL_FUNCTION, __FILE__, __LINE__, (format), ##__VA_ARGS__)

#else

#define IGL_UNEXPECTED(cond) (cond)
#define IGL_UNEXPECTED_MSG(cond, format, ...) (cond)
#define IGL_VERIFY(cond) (cond)
#define IGL_ASSERT(cond) static_cast<void>(0)
#define IGL_ASSERT_MSG(cond, format, ...) static_cast<void>(0)

#endif // IGL_DEBUG

#define IGL_ASSERT_NOT_REACHED() IGL_ASSERT_MSG(0, "Code should NOT be reached")
#define IGL_ASSERT_NOT_IMPLEMENTED() IGL_ASSERT_MSG(0, "Code NOT implemented")

///--------------------------------------
/// MARK: - Custom

#if IGL_REPORT_ERROR_ENABLED

#define IGL_ERROR_CATEGORY "IGL"

using IGLReportErrorFunc = void (*)(const char* file,
                                    const char* func,
                                    int line,
                                    const char* category,
                                    const char* format,
                                    ...);
IGL_API void IGLReportErrorSetHandler(IGLReportErrorFunc handler);
IGL_API IGLReportErrorFunc IGLReportErrorGetHandler(void);

#define IGL_REPORT_ERROR(cond)                                                                 \
  do {                                                                                         \
    if (!IGL_VERIFY(cond)) {                                                                   \
      IGLReportErrorGetHandler()(__FILE__, IGL_FUNCTION, __LINE__, IGL_ERROR_CATEGORY, #cond); \
    }                                                                                          \
  } while (0)

#define IGL_REPORT_ERROR_MSG(cond, format, ...)                                           \
  do {                                                                                    \
    bool cachedCond = (cond);                                                             \
    IGL_ASSERT_MSG(cachedCond, format, ##__VA_ARGS__);                                    \
    if (!cachedCond) {                                                                    \
      IGLReportErrorGetHandler()(                                                         \
          __FILE__, IGL_FUNCTION, __LINE__, IGL_ERROR_CATEGORY, (format), ##__VA_ARGS__); \
    }                                                                                     \
  } while (0)

#else

#define IGL_REPORT_ERROR(condition) static_cast<void>(0)
#define IGL_REPORT_ERROR_MSG(condition, format, ...) static_cast<void>(0)

#endif // IGL_REPORT_ERROR_ENABLED
