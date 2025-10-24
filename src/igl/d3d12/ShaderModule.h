/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Shader.h>
#include <igl/d3d12/Common.h>
#include <vector>

namespace igl::d3d12 {

class ShaderModule final : public IShaderModule {
 public:
  ShaderModule(ShaderModuleInfo info, std::vector<uint8_t> bytecode)
      : IShaderModule(info), bytecode_(std::move(bytecode)) {}
  ~ShaderModule() override = default;

  const std::vector<uint8_t>& getBytecode() const { return bytecode_; }

 private:
  std::vector<uint8_t> bytecode_; // DXIL bytecode
};

class ShaderStages final : public IShaderStages {
 public:
  ShaderStages(ShaderStagesDesc desc) : IShaderStages(desc) {}
  ~ShaderStages() override = default;
};

class ShaderLibrary final : public IShaderLibrary {
 public:
  explicit ShaderLibrary(std::vector<std::shared_ptr<IShaderModule>> modules)
      : IShaderLibrary(std::move(modules)) {}
  ~ShaderLibrary() override = default;
};

} // namespace igl::d3d12
