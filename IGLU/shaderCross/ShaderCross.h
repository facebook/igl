/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/IGL.h>

#include <string>

namespace iglu {
/// Wrapper for SPIR-V cross compiler to generate IGL-compatible shader sources for different
/// backends.
class ShaderCross final {
 public:
  explicit ShaderCross(igl::IDevice& device) noexcept;
  ~ShaderCross() noexcept;

  [[nodiscard]] std::string entryPointName(igl::ShaderStage stage) const noexcept;

  [[nodiscard]] std::string crossCompileFromVulkanSource(const char* source,
                                                         igl::ShaderStage stage,
                                                         igl::Result* IGL_NULLABLE
                                                             outResult) const noexcept;

 private:
  igl::IDevice& device_;
};
} // namespace iglu
