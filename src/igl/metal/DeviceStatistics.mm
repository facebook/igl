/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/DeviceStatistics.h>

namespace igl::metal {

void DeviceStatistics::incrementDrawCount(uint32_t newDrawCount) noexcept {
  currentDrawCount_ += newDrawCount;
}

size_t DeviceStatistics::getDrawCount() const noexcept {
  return currentDrawCount_;
}

void DeviceStatistics::incrementShaderCompilationCount() noexcept {
  shaderCompilationCount_++;
}

size_t DeviceStatistics::getShaderCompilationCount() const noexcept {
  return shaderCompilationCount_;
}

} // namespace igl::metal
