/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/DeviceStatistics.h>

namespace igl::metal {

void DeviceStatistics::incrementDrawCount(uint32_t newDrawCount) noexcept {
  currentDrawCount_.fetch_add(newDrawCount, std::memory_order_relaxed);
}

size_t DeviceStatistics::getDrawCount() const noexcept {
  return currentDrawCount_.load(std::memory_order_relaxed);
}

void DeviceStatistics::incrementShaderCompilationCount() noexcept {
  shaderCompilationCount_.fetch_add(1, std::memory_order_relaxed);
}

size_t DeviceStatistics::getShaderCompilationCount() const noexcept {
  return shaderCompilationCount_.load(std::memory_order_relaxed);
}

} // namespace igl::metal
