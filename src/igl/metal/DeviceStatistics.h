/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace igl::metal {

class DeviceStatistics {
 public:
  [[nodiscard]] size_t getDrawCount() const noexcept;

 private:
  friend class CommandQueue;
  void incrementDrawCount(uint32_t newDrawCount) noexcept;

  size_t currentDrawCount_ = 0;
};

} // namespace igl::metal
