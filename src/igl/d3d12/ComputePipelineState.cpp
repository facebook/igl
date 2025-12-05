/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/ComputePipelineState.h>
#include <igl/d3d12/ShaderModule.h>
#include <igl/d3d12/D3D12ReflectionUtils.h>
#include <igl/RenderPipelineReflection.h>
#include <igl/NameHandle.h>
#include <d3dcompiler.h>

namespace igl::d3d12 {

ComputePipelineState::ComputePipelineState(const ComputePipelineDesc& desc,
                                           igl::d3d12::ComPtr<ID3D12PipelineState> pipelineState,
                                           igl::d3d12::ComPtr<ID3D12RootSignature> rootSignature)
    : desc_(desc),
      pipelineState_(std::move(pipelineState)),
      rootSignature_(std::move(rootSignature)) {
  // Set D3D12 object names for PIX debugging
  const std::string& debugName = desc_.debugName;
  if (pipelineState_.Get() && !debugName.empty()) {
    std::wstring wideName(debugName.begin(), debugName.end());
    pipelineState_->SetName((L"ComputePSO_" + wideName).c_str());
    IGL_D3D12_LOG_VERBOSE("ComputePipelineState: Set PIX debug name 'ComputePSO_%s'\n", debugName.c_str());
  }
  if (rootSignature_.Get() && !debugName.empty()) {
    std::wstring wideName(debugName.begin(), debugName.end());
    rootSignature_->SetName((L"ComputeRootSig_" + wideName).c_str());
    IGL_D3D12_LOG_VERBOSE("ComputePipelineState: Set PIX root signature name 'ComputeRootSig_%s'\n", debugName.c_str());
  }
}

std::shared_ptr<IComputePipelineState::IComputePipelineReflection>
ComputePipelineState::computePipelineReflection() {
  // Return cached reflection if already created
  if (reflection_) {
    return reflection_;
  }

  // Reflection implementation following the pattern from RenderPipelineState
  struct ReflectionImpl final : public IComputePipelineReflection {
    std::vector<BufferArgDesc> ubs;
    std::vector<SamplerArgDesc> samplers;
    std::vector<TextureArgDesc> textures;
    const std::vector<BufferArgDesc>& allUniformBuffers() const override { return ubs; }
    const std::vector<SamplerArgDesc>& allSamplers() const override { return samplers; }
    const std::vector<TextureArgDesc>& allTextures() const override { return textures; }
  };

  auto out = std::make_shared<ReflectionImpl>();

  // Get compute shader module and reflect it
  if (!desc_.shaderStages) {
    return out;
  }

  auto computeModule = desc_.shaderStages->getComputeModule();
  if (!computeModule) {
    return out;
  }

  auto* d3dMod = dynamic_cast<const igl::d3d12::ShaderModule*>(computeModule.get());
  if (!d3dMod) {
    return out;
  }

  const auto& bc = d3dMod->getBytecode();
  if (bc.empty()) {
    return out;
  }

  // Create shader reflection interface using D3DReflect
  igl::d3d12::ComPtr<ID3D12ShaderReflection> refl;
  if (FAILED(D3DReflect(bc.data(), bc.size(), IID_PPV_ARGS(refl.GetAddressOf())))) {
    return out;
  }

  D3D12_SHADER_DESC sd{};
  if (FAILED(refl->GetDesc(&sd))) {
    return out;
  }

  // Extract constant buffer information
  for (UINT i = 0; i < sd.ConstantBuffers; ++i) {
    auto* cb = refl->GetConstantBufferByIndex(i);
    D3D12_SHADER_BUFFER_DESC cbd{};
    if (FAILED(cb->GetDesc(&cbd))) {
      continue;
    }

    // Find the bind point for this constant buffer
    int bufferIndex = -1;
    for (UINT r = 0; r < sd.BoundResources; ++r) {
      D3D12_SHADER_INPUT_BIND_DESC bind{};
      if (SUCCEEDED(refl->GetResourceBindingDesc(r, &bind))) {
        if (bind.Type == D3D_SIT_CBUFFER &&
            std::string(bind.Name) == std::string(cbd.Name)) {
          bufferIndex = static_cast<int>(bind.BindPoint);
          break;
        }
      }
    }

    BufferArgDesc ub;
    ub.name = igl::genNameHandle(cbd.Name ? cbd.Name : "");
    ub.bufferAlignment = 256; // D3D12 constant buffer alignment
    ub.bufferDataSize = cbd.Size;
    ub.bufferIndex = bufferIndex;
    ub.shaderStage = ShaderStage::Compute;
    ub.isUniformBlock = true;

    // Extract member variables from constant buffer
    for (UINT v = 0; v < cbd.Variables; ++v) {
      auto* var = cb->GetVariableByIndex(v);
      D3D12_SHADER_VARIABLE_DESC vd{};
      if (FAILED(var->GetDesc(&vd))) {
        continue;
      }

      auto* t = var->GetType();
      if (!t) {
        continue;
      }

      D3D12_SHADER_TYPE_DESC td{};
      if (FAILED(t->GetDesc(&td))) {
        continue;
      }

      BufferArgDesc::BufferMemberDesc m;
      m.name = igl::genNameHandle(vd.Name ? vd.Name : "");
      m.type = ReflectionUtils::mapUniformType(td);
      m.offset = vd.StartOffset;
      m.arrayLength = td.Elements ? td.Elements : 1;
      ub.members.push_back(std::move(m));
    }

    out->ubs.push_back(std::move(ub));
  }

  // Extract texture and sampler bindings
  for (UINT r = 0; r < sd.BoundResources; ++r) {
    D3D12_SHADER_INPUT_BIND_DESC bind{};
    if (FAILED(refl->GetResourceBindingDesc(r, &bind))) {
      continue;
    }

    if (bind.Type == D3D_SIT_TEXTURE) {
      TextureArgDesc t;
      t.name = bind.Name ? bind.Name : "";
      t.type = TextureType::TwoD;
      t.textureIndex = bind.BindPoint;
      t.shaderStage = ShaderStage::Compute;
      out->textures.push_back(std::move(t));
    } else if (bind.Type == D3D_SIT_SAMPLER) {
      SamplerArgDesc s;
      s.name = bind.Name ? bind.Name : "";
      s.samplerIndex = bind.BindPoint;
      s.shaderStage = ShaderStage::Compute;
      out->samplers.push_back(std::move(s));
    }
  }

  // Cache the reflection for future calls
  reflection_ = out;
  return reflection_;
}

} // namespace igl::d3d12
