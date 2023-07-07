/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once
#include <string>

namespace igl::shell {
struct ScreenshotTestsParams {
  std::string outputPath_;
  int frameToCapture_ = 0;
  bool isScreenshotTestsEnabled() const {
    return !outputPath_.empty();
  }
};
} // namespace igl::shell
