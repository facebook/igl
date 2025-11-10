/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Shader.h>
#include <igl/d3d12/Common.h>
#include <igl/Log.h>
#include <d3d12shader.h>
#include <vector>
#include <string>

namespace igl::d3d12 {

class ShaderModule final : public IShaderModule {
 public:
  // Resource binding information extracted from shader reflection
  struct ResourceBinding {
    std::string name;
    D3D_SHADER_INPUT_TYPE type;  // CBV, SRV, UAV, Sampler
    UINT bindPoint;
    UINT bindCount;
    UINT space;
  };

  // Constant buffer information from reflection
  struct ConstantBufferInfo {
    std::string name;
    UINT size;
    UINT numVariables;
  };

  ShaderModule(ShaderModuleInfo info, std::vector<uint8_t> bytecode)
      : IShaderModule(info), bytecode_(std::move(bytecode)) {
    if (!validateBytecode()) {
      IGL_LOG_ERROR("ShaderModule: Created with invalid bytecode (validation failed)\n");
    }
  }
  ~ShaderModule() override = default;

  const std::vector<uint8_t>& getBytecode() const { return bytecode_; }

  // Shader reflection API
  void setReflection(Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection);
  const std::vector<ResourceBinding>& getResourceBindings() const { return resourceBindings_; }
  const std::vector<ConstantBufferInfo>& getConstantBuffers() const { return constantBuffers_; }

  bool hasResource(const std::string& name) const;
  UINT getResourceBindPoint(const std::string& name) const;
  size_t getConstantBufferSize(const std::string& name) const;

  // Bytecode validation
  bool validateBytecode() const;

 private:
  std::vector<uint8_t> bytecode_; // DXIL bytecode
  Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection_;
  std::vector<ResourceBinding> resourceBindings_;
  std::vector<ConstantBufferInfo> constantBuffers_;

  void extractShaderMetadata();
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
