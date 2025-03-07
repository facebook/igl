/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "TestErrorGuard.h"

#include <gtest/gtest.h>

igl::tests::util::TestErrorGuard::TestErrorGuard() {
#if IGL_SOFT_ERROR_ENABLED
  savedErrorHandler_ = IGLGetSoftErrorHandler();
  IGLSetSoftErrorHandler(ReportErrorHandler);
#endif
}

igl::tests::util::TestErrorGuard::~TestErrorGuard() {
#if IGL_SOFT_ERROR_ENABLED
  IGLSetSoftErrorHandler(savedErrorHandler_);
#endif
}

void igl::tests::util::TestErrorGuard::ReportErrorHandler(const char* category,
                                                          const char* /*reason*/,
                                                          const char* file,
                                                          const char* /*func*/,
                                                          int line,
                                                          const char* format,
                                                          va_list ap) {
#if IGL_SOFT_ERROR_ENABLED
  va_list apCopy;
  va_copy(apCopy, ap);
  const auto len = std::vsnprintf(nullptr, 0, format, apCopy);
  va_end(apCopy);

  std::string fmtString;
  fmtString.resize(len + 1);
  std::vsnprintf(&fmtString.front(), len + 1, format, ap);
  fmtString.resize(len);

  ADD_FAILURE() << "IGL error encountered in " << file << ":" << line << " category=" << category
                << " " << fmtString;
#endif
}
