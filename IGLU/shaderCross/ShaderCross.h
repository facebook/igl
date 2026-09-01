/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <igl/IGL.h>

namespace iglu {

struct ShaderCrossOptions {
  /// Emit SIMD-group intrinsics instead of quadgroup ones for subgroup operations on iOS.
  /// Subgroup arithmetic (prefix sums, reductions) has no quadgroup equivalent and fails to
  /// cross-compile without this. Enabling it also widens broadcast/ballot/shuffle from 4 lanes
  /// to 32, so shaders must be authored for the wider group. Not every iOS GPU implements the
  /// SIMD-group functions.
  bool iosUseSimdgroupFunctions = false;
};

/// Wrapper for SPIR-V cross compiler to generate IGL-compatible shader sources for different
/// backends.
class ShaderCross final {
 public:
  explicit ShaderCross(igl::IDevice& device, ShaderCrossOptions options = {}) noexcept;
  ~ShaderCross() noexcept;
  ShaderCross(const ShaderCross&) = delete;
  ShaderCross& operator=(const ShaderCross&) = delete;
  ShaderCross(ShaderCross&&) = delete;
  ShaderCross& operator=(ShaderCross&&) = delete;

  [[nodiscard]] std::string entryPointName(igl::ShaderStage stage) const noexcept;

  [[nodiscard]] std::string crossCompileFromVulkanSource(const char* source,
                                                         igl::ShaderStage stage,
                                                         igl::Result* IGL_NULLABLE
                                                             outResult) const noexcept;

 private:
  igl::IDevice& device_;
  ShaderCrossOptions options_;
};
} // namespace iglu
