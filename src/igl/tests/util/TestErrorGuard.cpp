/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "TestErrorGuard.h"

#include <cstdarg>
#include <cstdio>
#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <string>

igl::tests::util::TestErrorGuard::TestErrorGuard() {
  savedErrorHandler_ = IGLReportErrorGetHandler();
  IGLReportErrorSetHandler(ReportErrorHandler);
}

igl::tests::util::TestErrorGuard::~TestErrorGuard() {
  IGLReportErrorSetHandler(savedErrorHandler_);
}

void igl::tests::util::TestErrorGuard::ReportErrorHandler(const char* file,
                                                          const char* /*func*/,
                                                          int line,
                                                          const char* category,
                                                          const char* format,
                                                          ...) {
  va_list ap;
  va_start(ap, format);

  va_list apCopy;
  va_copy(apCopy, ap);
  const auto len = std::vsnprintf(nullptr, 0, format, apCopy);
  va_end(apCopy);

  std::string fmtString;
  fmtString.resize(len + 1);
  std::vsnprintf(&fmtString.front(), len + 1, format, ap);
  fmtString.resize(len);
  va_end(ap);

  ADD_FAILURE() << "IGL error encountered in " << file << ":" << line << " category=" << category
                << " " << fmtString;
}
