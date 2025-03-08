/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdarg>
#include <igl/Core.h>

namespace igl::tests::util {
/// Sets an IGL handler that will cause gtest to fail when IGLReportErrorHandler is called
class TestErrorGuard final {
 public:
  TestErrorGuard();

  virtual ~TestErrorGuard();

  static void ReportErrorHandler(const char* category,
                                 const char* reason,
                                 const char* file,
                                 const char* func,
                                 int line,
                                 const char* format,
                                 va_list ap);

 private:
#if IGL_SOFT_ERROR_ENABLED
  IGLErrorHandlerFunc savedErrorHandler_;
#endif
};
} // namespace igl::tests::util
