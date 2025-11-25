/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12PipelineCache.h>
#include <igl/d3d12/D3D12RootSignatureKey.h>
#include <igl/d3d12/D3D12Context.h>
#include <vector>

namespace igl::d3d12 {

ComPtr<ID3D12RootSignature> D3D12PipelineCache::createRootSignatureFromKey(
    ID3D12Device* d3dDevice,
    const D3D12RootSignatureKey& key,
    D3D12_RESOURCE_BINDING_TIER bindingTier,
    Result* IGL_NULLABLE outResult) const {

  if (!d3dDevice) {
    Result::setResult(outResult, Result::Code::InvalidOperation, "D3D12 device is null");
    return nullptr;
  }

  IGL_D3D12_LOG_VERBOSE("Creating root signature from reflection key:\n");
  if (key.hasPushConstants) {
    IGL_D3D12_LOG_VERBOSE("  Push constants: b%u (%u DWORDs)\n",
                 key.pushConstantSlot, key.pushConstantSize);
  }
  IGL_D3D12_LOG_VERBOSE("  CBV slots: %zu, SRV slots: %zu, UAV slots: %zu, Sampler slots: %zu\n",
               key.usedCBVSlots.size(), key.usedSRVSlots.size(),
               key.usedUAVSlots.size(), key.usedSamplerSlots.size());

  // Determine if we need bounded ranges (Tier 1 hardware)
  const bool needsBoundedRanges = (bindingTier == D3D12_RESOURCE_BINDING_TIER_1);
  const UINT srvBound = needsBoundedRanges ? 128 : UINT_MAX;
  const UINT samplerBound = needsBoundedRanges ? 32 : UINT_MAX;
  const UINT uavBound = needsBoundedRanges ? 8 : UINT_MAX;

  // Build root parameters dynamically based on shader requirements
  std::vector<D3D12_ROOT_PARAMETER> rootParams;
  std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;

  // Parameter 0: Push constants (if shader uses them)
  if (key.hasPushConstants) {
    D3D12_ROOT_PARAMETER pushConstParam = {};
    pushConstParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    pushConstParam.Constants.ShaderRegister = key.pushConstantSlot;
    pushConstParam.Constants.RegisterSpace = 0;
    pushConstParam.Constants.Num32BitValues = key.pushConstantSize;
    pushConstParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams.push_back(pushConstParam);
  }

  // Add root CBVs for legacy bindBuffer support (b0, b1)
  // Only add if not conflicting with push constants
  if (key.pushConstantSlot != 0) {
    D3D12_ROOT_PARAMETER cbv0Param = {};
    cbv0Param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    cbv0Param.Descriptor.ShaderRegister = 0;
    cbv0Param.Descriptor.RegisterSpace = 0;
    cbv0Param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams.push_back(cbv0Param);
  }

  if (key.pushConstantSlot != 1) {
    D3D12_ROOT_PARAMETER cbv1Param = {};
    cbv1Param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    cbv1Param.Descriptor.ShaderRegister = 1;
    cbv1Param.Descriptor.RegisterSpace = 0;
    cbv1Param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams.push_back(cbv1Param);
  }

  // CBV descriptor table for b3-b15 (bind groups)
  D3D12_DESCRIPTOR_RANGE cbvRange = {};
  cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
  cbvRange.NumDescriptors = 13;  // b3 through b15
  cbvRange.BaseShaderRegister = 3;
  cbvRange.RegisterSpace = 0;
  cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  descriptorRanges.push_back(cbvRange);

  D3D12_ROOT_PARAMETER cbvTableParam = {};
  cbvTableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  cbvTableParam.DescriptorTable.NumDescriptorRanges = 1;
  cbvTableParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[descriptorRanges.size() - 1];
  cbvTableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rootParams.push_back(cbvTableParam);

  // SRV descriptor table (textures)
  D3D12_DESCRIPTOR_RANGE srvRange = {};
  srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  srvRange.NumDescriptors = srvBound;
  srvRange.BaseShaderRegister = 0;
  srvRange.RegisterSpace = 0;
  srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  descriptorRanges.push_back(srvRange);

  D3D12_ROOT_PARAMETER srvTableParam = {};
  srvTableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  srvTableParam.DescriptorTable.NumDescriptorRanges = 1;
  srvTableParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[descriptorRanges.size() - 1];
  srvTableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rootParams.push_back(srvTableParam);

  // Sampler descriptor table
  D3D12_DESCRIPTOR_RANGE samplerRange = {};
  samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  samplerRange.NumDescriptors = samplerBound;
  samplerRange.BaseShaderRegister = 0;
  samplerRange.RegisterSpace = 0;
  samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  descriptorRanges.push_back(samplerRange);

  D3D12_ROOT_PARAMETER samplerTableParam = {};
  samplerTableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  samplerTableParam.DescriptorTable.NumDescriptorRanges = 1;
  samplerTableParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[descriptorRanges.size() - 1];
  samplerTableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rootParams.push_back(samplerTableParam);

  // UAV descriptor table (storage buffers)
  D3D12_DESCRIPTOR_RANGE uavRange = {};
  uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
  uavRange.NumDescriptors = uavBound;
  uavRange.BaseShaderRegister = 0;
  uavRange.RegisterSpace = 0;
  uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  descriptorRanges.push_back(uavRange);

  D3D12_ROOT_PARAMETER uavTableParam = {};
  uavTableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  uavTableParam.DescriptorTable.NumDescriptorRanges = 1;
  uavTableParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[descriptorRanges.size() - 1];
  uavTableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rootParams.push_back(uavTableParam);

  // Create root signature desc
  D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
  rootSigDesc.NumParameters = static_cast<UINT>(rootParams.size());
  rootSigDesc.pParameters = rootParams.data();
  rootSigDesc.NumStaticSamplers = 0;
  rootSigDesc.pStaticSamplers = nullptr;
  rootSigDesc.Flags = key.flags;

  IGL_D3D12_LOG_VERBOSE("  Root signature has %u parameters\n", rootSigDesc.NumParameters);

  // Use existing caching infrastructure
  return getOrCreateRootSignature(d3dDevice, rootSigDesc, outResult);
}

} // namespace igl::d3d12
