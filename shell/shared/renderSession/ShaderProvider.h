/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Device.h>
#include <igl/Shader.h>

#include <string>
#include <unordered_set>
#include <vector>

namespace igl::shell {

class IShaderProvider {
 public:
  [[nodiscard]] virtual ShaderStage getStage() const = 0;
  [[nodiscard]] virtual std::string getShaderText(const IDevice& device) const = 0;
  [[nodiscard]] virtual std::vector<uint32_t> getShaderBinary(const IDevice& device) const = 0;
  virtual ~IShaderProvider() = default;
};

} // namespace igl::shell
