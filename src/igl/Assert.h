/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <assert.h>
#include <cstdarg>

#include <minilog/minilog.h>

#include <igl/Macros.h>

namespace igl {

template<typename T>
static inline const T& _IGLVerify(const T& cond,
                                  const char* func,
                                  const char* file,
                                  int line,
                                  const char* format,
                                  ...) {
  if (!cond) {
    LLOGW("[IGL] Assert failed in '%s' (%s:%d): ", func, file, line);
    va_list ap;
    va_start(ap, format);
    LLOGW(format, ap);
    va_end(ap);
    assert(false);
  }
  return cond;
}
} // namespace igl

#if IGL_DEBUG

#define IGL_UNEXPECTED(cond) (!_IGLVerify(0 == !!(cond), IGL_FUNCTION, __FILE__, __LINE__, #cond))
#define IGL_VERIFY(cond) ::igl::_IGLVerify((cond), IGL_FUNCTION, __FILE__, __LINE__, #cond)
#define IGL_ASSERT(cond) (void)IGL_VERIFY(cond)
#define IGL_ASSERT_MSG(cond, format, ...) \
  (void)::igl::_IGLVerify((cond), IGL_FUNCTION, __FILE__, __LINE__, (format), ##__VA_ARGS__)

#else

#define IGL_UNEXPECTED(cond) (cond)
#define IGL_VERIFY(cond) (cond)
#define IGL_ASSERT(cond) static_cast<void>(0)
#define IGL_ASSERT_MSG(cond, format, ...) static_cast<void>(0)

#endif // IGL_DEBUG

#define IGL_ASSERT_NOT_REACHED() IGL_ASSERT_MSG(0, "Code should NOT be reached")
