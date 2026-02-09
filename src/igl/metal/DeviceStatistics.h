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

namespace igl::metal {

class DeviceStatistics {
 public:
  [[nodiscard]] size_t getDrawCount() const noexcept;
  [[nodiscard]] size_t getShaderCompilationCount() const noexcept;

 private:
  friend class CommandQueue;
  friend class Device;
  void incrementDrawCount(uint32_t newDrawCount) noexcept;
  void incrementShaderCompilationCount() noexcept;

  std::atomic<size_t> currentDrawCount_{0};
  std::atomic<size_t> shaderCompilationCount_{0};
};

} // namespace igl::metal
