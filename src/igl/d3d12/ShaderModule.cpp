/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/ShaderModule.h>

namespace igl::d3d12 {

void ShaderModule::setReflection(Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection) {
  reflection_ = reflection;
  if (reflection_.Get()) {
    extractShaderMetadata();
  }
}

void ShaderModule::extractShaderMetadata() {
  if (!reflection_.Get()) {
    return;
  }

  D3D12_SHADER_DESC shaderDesc = {};
  HRESULT hr = reflection_->GetDesc(&shaderDesc);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("ShaderModule::extractShaderMetadata: Failed to get shader desc: 0x%08X\n", hr);
    return;
  }

  IGL_LOG_INFO("ShaderModule: Reflection extracted - %u constant buffers, %u bound resources, %u input params, %u output params\n",
               shaderDesc.ConstantBuffers,
               shaderDesc.BoundResources,
               shaderDesc.InputParameters,
               shaderDesc.OutputParameters);

  // Extract resource bindings (textures, buffers, samplers, UAVs)
  resourceBindings_.clear();
  for (UINT i = 0; i < shaderDesc.BoundResources; i++) {
    D3D12_SHADER_INPUT_BIND_DESC bindDesc = {};
    hr = reflection_->GetResourceBindingDesc(i, &bindDesc);
    if (FAILED(hr)) {
      IGL_LOG_ERROR("ShaderModule::extractShaderMetadata: Failed to get resource binding %u: 0x%08X\n", i, hr);
      continue;
    }

    ResourceBinding binding;
    binding.name = bindDesc.Name;
    binding.type = bindDesc.Type;
    binding.bindPoint = bindDesc.BindPoint;
    binding.bindCount = bindDesc.BindCount;
    binding.space = bindDesc.Space;

    resourceBindings_.push_back(binding);

    const char* typeStr = "Unknown";
    switch (bindDesc.Type) {
      case D3D_SIT_CBUFFER: typeStr = "CBV (Constant Buffer)"; break;
      case D3D_SIT_TBUFFER: typeStr = "TBuffer"; break;
      case D3D_SIT_TEXTURE: typeStr = "SRV (Texture)"; break;
      case D3D_SIT_SAMPLER: typeStr = "Sampler"; break;
      case D3D_SIT_UAV_RWTYPED: typeStr = "UAV (RW Typed)"; break;
      case D3D_SIT_STRUCTURED: typeStr = "SRV (StructuredBuffer)"; break;
      case D3D_SIT_UAV_RWSTRUCTURED: typeStr = "UAV (RWStructuredBuffer)"; break;
      case D3D_SIT_BYTEADDRESS: typeStr = "SRV (ByteAddressBuffer)"; break;
      case D3D_SIT_UAV_RWBYTEADDRESS: typeStr = "UAV (RWByteAddressBuffer)"; break;
      case D3D_SIT_UAV_APPEND_STRUCTURED: typeStr = "UAV (AppendStructuredBuffer)"; break;
      case D3D_SIT_UAV_CONSUME_STRUCTURED: typeStr = "UAV (ConsumeStructuredBuffer)"; break;
      case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER: typeStr = "UAV (RWStructuredBuffer with counter)"; break;
      default: break;
    }

    IGL_LOG_DEBUG("  Resource [%u]: '%s' | Type: %s | Slot: t%u/b%u/s%u/u%u | Space: %u | Count: %u\n",
                  i,
                  bindDesc.Name,
                  typeStr,
                  bindDesc.Type == D3D_SIT_TEXTURE ? bindDesc.BindPoint : 0,
                  bindDesc.Type == D3D_SIT_CBUFFER ? bindDesc.BindPoint : 0,
                  bindDesc.Type == D3D_SIT_SAMPLER ? bindDesc.BindPoint : 0,
                  (bindDesc.Type == D3D_SIT_UAV_RWTYPED || bindDesc.Type == D3D_SIT_UAV_RWSTRUCTURED) ? bindDesc.BindPoint : 0,
                  bindDesc.Space,
                  bindDesc.BindCount);
  }

  // Extract constant buffer information
  constantBuffers_.clear();
  for (UINT i = 0; i < shaderDesc.ConstantBuffers; i++) {
    ID3D12ShaderReflectionConstantBuffer* cb = reflection_->GetConstantBufferByIndex(i);
    if (!cb) {
      IGL_LOG_ERROR("ShaderModule::extractShaderMetadata: Failed to get constant buffer %u\n", i);
      continue;
    }

    D3D12_SHADER_BUFFER_DESC bufferDesc = {};
    hr = cb->GetDesc(&bufferDesc);
    if (FAILED(hr)) {
      IGL_LOG_ERROR("ShaderModule::extractShaderMetadata: Failed to get CB desc %u: 0x%08X\n", i, hr);
      continue;
    }

    ConstantBufferInfo cbInfo;
    cbInfo.name = bufferDesc.Name;
    cbInfo.size = bufferDesc.Size;
    cbInfo.numVariables = bufferDesc.Variables;

    constantBuffers_.push_back(cbInfo);

    IGL_LOG_DEBUG("  Constant Buffer [%u]: '%s' | Size: %u bytes | Variables: %u\n",
                  i,
                  bufferDesc.Name,
                  bufferDesc.Size,
                  bufferDesc.Variables);

    // Optionally log variable details for debugging
    for (UINT v = 0; v < bufferDesc.Variables; v++) {
      ID3D12ShaderReflectionVariable* var = cb->GetVariableByIndex(v);
      if (var) {
        D3D12_SHADER_VARIABLE_DESC varDesc = {};
        if (SUCCEEDED(var->GetDesc(&varDesc))) {
          IGL_LOG_DEBUG("    Variable [%u]: '%s' | Offset: %u | Size: %u bytes\n",
                        v,
                        varDesc.Name,
                        varDesc.StartOffset,
                        varDesc.Size);
        }
      }
    }
  }
}

bool ShaderModule::hasResource(const std::string& name) const {
  for (const auto& binding : resourceBindings_) {
    if (binding.name == name) {
      return true;
    }
  }
  return false;
}

UINT ShaderModule::getResourceBindPoint(const std::string& name) const {
  for (const auto& binding : resourceBindings_) {
    if (binding.name == name) {
      return binding.bindPoint;
    }
  }
  return UINT_MAX; // Not found
}

size_t ShaderModule::getConstantBufferSize(const std::string& name) const {
  for (const auto& cb : constantBuffers_) {
    if (cb.name == name) {
      return cb.size;
    }
  }
  return 0; // Not found
}

} // namespace igl::d3d12
