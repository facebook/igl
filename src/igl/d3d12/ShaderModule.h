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
      : info_(std::move(info)), bytecode_(std::move(bytecode)) {}
  ~ShaderModule() override = default;

  ShaderModuleInfo shaderModuleInfo() const override { return info_; }

  const std::vector<uint8_t>& getBytecode() const { return bytecode_; }

 private:
  ShaderModuleInfo info_;
  std::vector<uint8_t> bytecode_; // DXIL bytecode
};

class ShaderStages final : public IShaderStages {
 public:
  ShaderStages(ShaderStagesDesc desc) : desc_(std::move(desc)) {}
  ~ShaderStages() override = default;

  const ShaderStagesDesc& getShaderStagesDesc() const { return desc_; }

 private:
  ShaderStagesDesc desc_;
};

} // namespace igl::d3d12
