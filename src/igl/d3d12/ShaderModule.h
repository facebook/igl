/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <d3d12shader.h>
#include <string>
#include <vector>
#include <igl/Common.h>
#include <igl/Shader.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class ShaderModule final : public IShaderModule {
 public:
  // Resource binding information extracted from shader reflection
  struct ResourceBinding {
    std::string name;
    D3D_SHADER_INPUT_TYPE type; // CBV, SRV, UAV, Sampler
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

  // Shader resource usage summary for root signature selection
  struct ShaderReflectionInfo {
    // Push constants (inline root constants)
    bool hasPushConstants = false;
    UINT pushConstantSlot = UINT_MAX; // Which b# register
    UINT pushConstantSize = 0; // Size in 32-bit values

    // Resource slot usage (for conflict detection)
    std::vector<UINT> usedCBVSlots; // Constant buffer slots (b#)
    std::vector<UINT> usedSRVSlots; // Shader resource view slots (t#)
    std::vector<UINT> usedUAVSlots; // Unordered access view slots (u#)
    std::vector<UINT> usedSamplerSlots; // Sampler slots (s#)

    // Maximum slot indices used (for root signature sizing)
    UINT maxCBVSlot = 0;
    UINT maxSRVSlot = 0;
    UINT maxUAVSlot = 0;
    UINT maxSamplerSlot = 0;
  };

  ShaderModule(ShaderModuleInfo info, std::vector<uint8_t> bytecode) :
    IShaderModule(info), bytecode_(std::move(bytecode)) {
    if (!validateBytecode()) {
      IGL_LOG_ERROR("ShaderModule: Created with invalid bytecode (validation failed)\n");
    }
  }
  ~ShaderModule() override = default;

  const std::vector<uint8_t>& getBytecode() const {
    return bytecode_;
  }

  // Shader reflection API
  void setReflection(igl::d3d12::ComPtr<ID3D12ShaderReflection> reflection);
  const std::vector<ResourceBinding>& getResourceBindings() const {
    return resourceBindings_;
  }
  const std::vector<ConstantBufferInfo>& getConstantBuffers() const {
    return constantBuffers_;
  }
  const ShaderReflectionInfo& getReflectionInfo() const {
    return reflectionInfo_;
  }

  bool hasResource(const std::string& name) const;
  UINT getResourceBindPoint(const std::string& name) const;
  size_t getConstantBufferSize(const std::string& name) const;

  // Bytecode validation
  bool validateBytecode() const;

 private:
  std::vector<uint8_t> bytecode_; // DXIL bytecode
  igl::d3d12::ComPtr<ID3D12ShaderReflection> reflection_;
  std::vector<ResourceBinding> resourceBindings_;
  std::vector<ConstantBufferInfo> constantBuffers_;
  ShaderReflectionInfo reflectionInfo_;

  void extractShaderMetadata();
};

class ShaderStages final : public IShaderStages {
 public:
  ShaderStages(ShaderStagesDesc desc) : IShaderStages(desc) {}
  ~ShaderStages() override = default;
};

class ShaderLibrary final : public IShaderLibrary {
 public:
  explicit ShaderLibrary(std::vector<std::shared_ptr<IShaderModule>> modules) :
    IShaderLibrary(std::move(modules)) {}
  ~ShaderLibrary() override = default;
};

} // namespace igl::d3d12
