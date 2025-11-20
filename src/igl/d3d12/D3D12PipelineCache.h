/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <igl/Common.h>
#include <igl/RenderPipelineState.h>
#include <igl/ComputePipelineState.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/ShaderModule.h>
#include <igl/d3d12/VertexInputState.h>

namespace igl::d3d12 {

class D3D12PipelineCache {
 public:
  D3D12PipelineCache() = default;

  void clear();

 private:
  size_t hashRootSignature(const D3D12_ROOT_SIGNATURE_DESC& desc) const;
  ComPtr<ID3D12RootSignature> getOrCreateRootSignature(
      ID3D12Device* d3dDevice,
      const D3D12_ROOT_SIGNATURE_DESC& desc,
      Result* IGL_NULLABLE outResult) const;

  size_t hashRenderPipelineDesc(const RenderPipelineDesc& desc) const;
  size_t hashComputePipelineDesc(const ComputePipelineDesc& desc) const;

  mutable std::unordered_map<size_t, ComPtr<ID3D12PipelineState>> graphicsPSOCache_;
  mutable std::unordered_map<size_t, ComPtr<ID3D12PipelineState>> computePSOCache_;
  mutable std::mutex psoCacheMutex_;
  mutable size_t graphicsPSOCacheHits_ = 0;
  mutable size_t graphicsPSOCacheMisses_ = 0;
  mutable size_t computePSOCacheHits_ = 0;
  mutable size_t computePSOCacheMisses_ = 0;

  mutable std::unordered_map<size_t, ComPtr<ID3D12RootSignature>> rootSignatureCache_;
  mutable std::mutex rootSignatureCacheMutex_;
  mutable size_t rootSignatureCacheHits_ = 0;
  mutable size_t rootSignatureCacheMisses_ = 0;

  std::vector<uint8_t> mipmapVSBytecode_;
  std::vector<uint8_t> mipmapPSBytecode_;
  ComPtr<ID3D12RootSignature> mipmapRootSignature_;
  bool mipmapShadersAvailable_ = false;

  friend class Device;
};

inline void D3D12PipelineCache::clear() {
  {
    std::lock_guard<std::mutex> lock(psoCacheMutex_);
    graphicsPSOCache_.clear();
    computePSOCache_.clear();
    graphicsPSOCacheHits_ = 0;
    graphicsPSOCacheMisses_ = 0;
    computePSOCacheHits_ = 0;
    computePSOCacheMisses_ = 0;
  }
  {
    std::lock_guard<std::mutex> lock(rootSignatureCacheMutex_);
    rootSignatureCache_.clear();
    rootSignatureCacheHits_ = 0;
    rootSignatureCacheMisses_ = 0;
  }
  mipmapVSBytecode_.clear();
  mipmapPSBytecode_.clear();
  mipmapRootSignature_.Reset();
  mipmapShadersAvailable_ = false;
}

inline size_t D3D12PipelineCache::hashRootSignature(
    const D3D12_ROOT_SIGNATURE_DESC& desc) const {
  size_t hash = 0;

  hashCombine(hash, static_cast<size_t>(desc.Flags));
  hashCombine(hash, static_cast<size_t>(desc.NumParameters));

  for (UINT i = 0; i < desc.NumParameters; ++i) {
    const auto& param = desc.pParameters[i];

    hashCombine(hash, static_cast<size_t>(param.ParameterType));
    hashCombine(hash, static_cast<size_t>(param.ShaderVisibility));

    switch (param.ParameterType) {
    case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
      hashCombine(hash,
                  static_cast<size_t>(param.DescriptorTable.NumDescriptorRanges));

      for (UINT j = 0; j < param.DescriptorTable.NumDescriptorRanges; ++j) {
        const auto& range = param.DescriptorTable.pDescriptorRanges[j];
        hashCombine(hash, static_cast<size_t>(range.RangeType));
        hashCombine(hash, static_cast<size_t>(range.NumDescriptors));
        hashCombine(hash, static_cast<size_t>(range.BaseShaderRegister));
        hashCombine(hash, static_cast<size_t>(range.RegisterSpace));
        hashCombine(
            hash,
            static_cast<size_t>(range.OffsetInDescriptorsFromTableStart));
      }
      break;
    }
    case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: {
      hashCombine(hash, static_cast<size_t>(param.Constants.ShaderRegister));
      hashCombine(hash, static_cast<size_t>(param.Constants.RegisterSpace));
      hashCombine(hash, static_cast<size_t>(param.Constants.Num32BitValues));
      break;
    }
    case D3D12_ROOT_PARAMETER_TYPE_CBV:
    case D3D12_ROOT_PARAMETER_TYPE_SRV:
    case D3D12_ROOT_PARAMETER_TYPE_UAV: {
      hashCombine(hash, static_cast<size_t>(param.Descriptor.ShaderRegister));
      hashCombine(hash, static_cast<size_t>(param.Descriptor.RegisterSpace));
      break;
    }
    }
  }

  hashCombine(hash, static_cast<size_t>(desc.NumStaticSamplers));
  for (UINT i = 0; i < desc.NumStaticSamplers; ++i) {
    const auto& sampler = desc.pStaticSamplers[i];
    hashCombine(hash, static_cast<size_t>(sampler.Filter));
    hashCombine(hash, static_cast<size_t>(sampler.AddressU));
    hashCombine(hash, static_cast<size_t>(sampler.AddressV));
    hashCombine(hash, static_cast<size_t>(sampler.AddressW));
    hashCombine(hash, static_cast<size_t>(sampler.ComparisonFunc));
    hashCombine(hash, static_cast<size_t>(sampler.ShaderRegister));
    hashCombine(hash, static_cast<size_t>(sampler.RegisterSpace));
    hashCombine(hash, static_cast<size_t>(sampler.ShaderVisibility));
  }

  return hash;
}

inline ComPtr<ID3D12RootSignature> D3D12PipelineCache::getOrCreateRootSignature(
    ID3D12Device* d3dDevice,
    const D3D12_ROOT_SIGNATURE_DESC& desc,
    Result* IGL_NULLABLE outResult) const {
  const size_t hash = hashRootSignature(desc);

  {
    std::lock_guard<std::mutex> lock(rootSignatureCacheMutex_);
    auto it = rootSignatureCache_.find(hash);
    if (it != rootSignatureCache_.end()) {
      rootSignatureCacheHits_++;
      IGL_D3D12_LOG_VERBOSE(
          "  Root signature cache HIT (hash=0x%zx, hits=%zu, misses=%zu)\n",
          hash,
          rootSignatureCacheHits_,
          rootSignatureCacheMisses_);
      return it->second;
    }
  }

  rootSignatureCacheMisses_++;
  IGL_D3D12_LOG_VERBOSE(
      "  Root signature cache MISS (hash=0x%zx, hits=%zu, misses=%zu)\n",
      hash,
      rootSignatureCacheHits_,
      rootSignatureCacheMisses_);

  if (!d3dDevice) {
    Result::setResult(outResult,
                      Result::Code::InvalidOperation,
                      "D3D12 device is null");
    return nullptr;
  }

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;

  IGL_D3D12_LOG_VERBOSE("  Serializing root signature (version 1.0)...\n");
  HRESULT hr = D3D12SerializeRootSignature(
      &desc,
      D3D_ROOT_SIGNATURE_VERSION_1,
      signature.GetAddressOf(),
      error.GetAddressOf());
  if (FAILED(hr)) {
    if (error.Get()) {
      const char* errorMsg =
          static_cast<const char*>(error->GetBufferPointer());
      IGL_LOG_ERROR("Root signature serialization error: %s\n", errorMsg);
    }
    Result::setResult(outResult,
                      Result::Code::RuntimeError,
                      "Failed to serialize root signature");
    return nullptr;
  }

  ComPtr<ID3D12RootSignature> rootSignature;
  hr = d3dDevice->CreateRootSignature(0,
                                      signature->GetBufferPointer(),
                                      signature->GetBufferSize(),
                                      IID_PPV_ARGS(rootSignature.GetAddressOf()));
  if (FAILED(hr)) {
    IGL_LOG_ERROR(
        "  CreateRootSignature FAILED: 0x%08X\n",
        static_cast<unsigned>(hr));
    Result::setResult(outResult,
                      Result::Code::RuntimeError,
                      "Failed to create root signature");
    return nullptr;
  }

  IGL_D3D12_LOG_VERBOSE("  Root signature created successfully\n");

  {
    std::lock_guard<std::mutex> lock(rootSignatureCacheMutex_);
    rootSignatureCache_[hash] = rootSignature;
  }

  return rootSignature;
}

inline size_t D3D12PipelineCache::hashRenderPipelineDesc(
    const RenderPipelineDesc& desc) const {
  size_t hash = 0;

  if (desc.shaderStages) {
    auto* vertexModule =
        static_cast<const ShaderModule*>(desc.shaderStages->getVertexModule().get());
    auto* fragmentModule =
        static_cast<const ShaderModule*>(desc.shaderStages->getFragmentModule().get());

    if (vertexModule) {
      const auto& vsBytecode = vertexModule->getBytecode();
      hashCombine(hash, vsBytecode.size());
      size_t bytesToHash = std::min<size_t>(256, vsBytecode.size());
      for (size_t i = 0; i < bytesToHash; i += 8) {
        hashCombine(hash, static_cast<size_t>(vsBytecode[i]));
      }
    }

    if (fragmentModule) {
      const auto& psBytecode = fragmentModule->getBytecode();
      hashCombine(hash, psBytecode.size());
      size_t bytesToHash = std::min<size_t>(256, psBytecode.size());
      for (size_t i = 0; i < bytesToHash; i += 8) {
        hashCombine(hash, static_cast<size_t>(psBytecode[i]));
      }
    }
  }

  if (desc.vertexInputState) {
    auto* d3d12VertexInput =
        static_cast<const VertexInputState*>(desc.vertexInputState.get());
    const auto& vertexDesc = d3d12VertexInput->getDesc();
    hashCombine(hash, vertexDesc.numAttributes);
    for (size_t i = 0; i < vertexDesc.numAttributes; ++i) {
      hashCombine(hash,
                  static_cast<size_t>(vertexDesc.attributes[i].format));
      hashCombine(hash, vertexDesc.attributes[i].offset);
      hashCombine(hash, vertexDesc.attributes[i].bufferIndex);
      hashCombine(
          hash,
          std::hash<std::string>{}(vertexDesc.attributes[i].name));
    }
  }

  hashCombine(hash, desc.targetDesc.colorAttachments.size());
  for (const auto& att : desc.targetDesc.colorAttachments) {
    hashCombine(hash, static_cast<size_t>(att.textureFormat));
  }
  hashCombine(hash,
              static_cast<size_t>(desc.targetDesc.depthAttachmentFormat));
  hashCombine(hash,
              static_cast<size_t>(desc.targetDesc.stencilAttachmentFormat));

  for (const auto& att : desc.targetDesc.colorAttachments) {
    hashCombine(hash, att.blendEnabled ? 1 : 0);
    hashCombine(hash, static_cast<size_t>(att.srcRGBBlendFactor));
    hashCombine(hash, static_cast<size_t>(att.dstRGBBlendFactor));
    hashCombine(hash, static_cast<size_t>(att.rgbBlendOp));
    hashCombine(hash, static_cast<size_t>(att.srcAlphaBlendFactor));
    hashCombine(hash, static_cast<size_t>(att.dstAlphaBlendFactor));
    hashCombine(hash, static_cast<size_t>(att.alphaBlendOp));
    hashCombine(hash, static_cast<size_t>(att.colorWriteMask));
  }

  hashCombine(hash, static_cast<size_t>(desc.cullMode));
  hashCombine(hash, static_cast<size_t>(desc.frontFaceWinding));
  hashCombine(hash, static_cast<size_t>(desc.polygonFillMode));

  hashCombine(hash, static_cast<size_t>(desc.topology));

  hashCombine(hash, desc.sampleCount);

  return hash;
}

inline size_t D3D12PipelineCache::hashComputePipelineDesc(
    const ComputePipelineDesc& desc) const {
  size_t hash = 0;

  if (desc.shaderStages) {
    auto* computeModule =
        static_cast<const ShaderModule*>(desc.shaderStages->getComputeModule().get());

    if (computeModule) {
      const auto& csBytecode = computeModule->getBytecode();
      hashCombine(hash, csBytecode.size());
      size_t bytesToHash = std::min<size_t>(256, csBytecode.size());
      for (size_t i = 0; i < bytesToHash; i += 8) {
        hashCombine(hash, static_cast<size_t>(csBytecode[i]));
      }
    }
  }

  for (char c : desc.debugName) {
    hashCombine(hash, static_cast<size_t>(c));
  }

  return hash;
}

} // namespace igl::d3d12
