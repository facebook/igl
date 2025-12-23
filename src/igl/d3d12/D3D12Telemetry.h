/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace igl::d3d12 {

class D3D12Telemetry {
 public:
  void incrementDrawCount(size_t count) noexcept {
    currentDrawCount_.fetch_add(count, std::memory_order_relaxed);
  }

  void incrementShaderCompilationCount() noexcept {
    shaderCompilationCount_.fetch_add(1, std::memory_order_relaxed);
  }

  [[nodiscard]] size_t getDrawCount() const noexcept {
    return currentDrawCount_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] size_t getShaderCompilationCount() const noexcept {
    return shaderCompilationCount_.load(std::memory_order_relaxed);
  }

 private:
  std::atomic<size_t> currentDrawCount_{0};
  std::atomic<size_t> shaderCompilationCount_{0};
};

} // namespace igl::d3d12
