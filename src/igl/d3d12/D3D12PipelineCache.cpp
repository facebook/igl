/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12PipelineCache.h>
#include <igl/d3d12/D3D12RootSignatureKey.h>
#include <igl/d3d12/D3D12Context.h>
#include <algorithm>
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

  // Build descriptor ranges dynamically - only create ranges for resource types the shader uses
  // The ranges must remain stable (no reallocation) since root parameters will point to them
  std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;
  descriptorRanges.reserve(4);  // Maximum: CBV, SRV, Sampler, UAV

  // Track which descriptor range index corresponds to each resource type
  size_t cbvRangeIndex = SIZE_MAX;
  size_t srvRangeIndex = SIZE_MAX;
  size_t samplerRangeIndex = SIZE_MAX;
  size_t uavRangeIndex = SIZE_MAX;

  // CBV descriptor table (only if shader uses CBVs)
  if (!key.usedCBVSlots.empty()) {
    cbvRangeIndex = descriptorRanges.size();

    // D3D12 descriptor tables must start at register 0
    // Calculate range from 0 to max slot (includes unused slots)
    UINT maxCBVSlot = key.maxCBVSlot;
    UINT numCBVs = maxCBVSlot + 1;

    D3D12_DESCRIPTOR_RANGE cbvRange = {};
    cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvRange.NumDescriptors = numCBVs;
    cbvRange.BaseShaderRegister = 0;
    cbvRange.RegisterSpace = 0;
    cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descriptorRanges.push_back(cbvRange);
  }

  // SRV descriptor table (only if shader uses SRVs)
  if (!key.usedSRVSlots.empty()) {
    srvRangeIndex = descriptorRanges.size();

    // D3D12 descriptor tables must start at register 0
    // Calculate range from 0 to max slot (includes unused slots)
    UINT maxSRVSlot = key.maxSRVSlot;
    UINT numSRVs = maxSRVSlot + 1;

    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = numSRVs;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descriptorRanges.push_back(srvRange);
  }

  // Sampler descriptor table (only if shader uses samplers)
  if (!key.usedSamplerSlots.empty()) {
    samplerRangeIndex = descriptorRanges.size();

    // D3D12 descriptor tables must start at register 0
    // Calculate range from 0 to max slot (includes unused slots)
    UINT maxSamplerSlot = key.maxSamplerSlot;
    UINT numSamplers = maxSamplerSlot + 1;

    D3D12_DESCRIPTOR_RANGE samplerRange = {};
    samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerRange.NumDescriptors = numSamplers;
    samplerRange.BaseShaderRegister = 0;
    samplerRange.RegisterSpace = 0;
    samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descriptorRanges.push_back(samplerRange);
  }

  // UAV descriptor table (only if shader uses UAVs)
  if (!key.usedUAVSlots.empty()) {
    uavRangeIndex = descriptorRanges.size();

    // D3D12 descriptor tables must start at register 0
    // Calculate range from 0 to max slot (includes unused slots)
    UINT maxUAVSlot = key.maxUAVSlot;
    UINT numUAVs = maxUAVSlot + 1;

    D3D12_DESCRIPTOR_RANGE uavRange = {};
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = numUAVs;
    uavRange.BaseShaderRegister = 0;
    uavRange.RegisterSpace = 0;
    uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descriptorRanges.push_back(uavRange);
  }

  // Build root parameters dynamically based on shader reflection (Vulkan approach)
  // Only include what the shader actually declares - no hardcoded assumptions
  // Order: Push constants, CBV table, SRV table, Sampler table, UAV table
  std::vector<D3D12_ROOT_PARAMETER> rootParams;

  // Track which root parameter index corresponds to each resource type
  UINT pushConstantRootParamIndex = UINT_MAX;
  UINT cbvTableRootParamIndex = UINT_MAX;
  UINT srvTableRootParamIndex = UINT_MAX;
  UINT samplerTableRootParamIndex = UINT_MAX;
  UINT uavTableRootParamIndex = UINT_MAX;

  // Add push constants if shader uses them (always first if present)
  if (key.hasPushConstants) {
    pushConstantRootParamIndex = static_cast<UINT>(rootParams.size());

    D3D12_ROOT_PARAMETER pushConstParam = {};
    pushConstParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    pushConstParam.Constants.ShaderRegister = key.pushConstantSlot;
    pushConstParam.Constants.RegisterSpace = 0;
    pushConstParam.Constants.Num32BitValues = key.pushConstantSize;
    pushConstParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams.push_back(pushConstParam);
  }

  // Add CBV descriptor table if shader uses any CBV slots
  if (!key.usedCBVSlots.empty() && cbvRangeIndex != SIZE_MAX) {
    cbvTableRootParamIndex = static_cast<UINT>(rootParams.size());

    D3D12_ROOT_PARAMETER cbvTableParam = {};
    cbvTableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    cbvTableParam.DescriptorTable.NumDescriptorRanges = 1;
    cbvTableParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[cbvRangeIndex];
    cbvTableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams.push_back(cbvTableParam);
  }

  // Add SRV descriptor table if shader uses any SRV slots
  if (!key.usedSRVSlots.empty() && srvRangeIndex != SIZE_MAX) {
    srvTableRootParamIndex = static_cast<UINT>(rootParams.size());

    D3D12_ROOT_PARAMETER srvTableParam = {};
    srvTableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    srvTableParam.DescriptorTable.NumDescriptorRanges = 1;
    srvTableParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[srvRangeIndex];
    srvTableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams.push_back(srvTableParam);
  }

  // Add Sampler descriptor table if shader uses any sampler slots
  if (!key.usedSamplerSlots.empty() && samplerRangeIndex != SIZE_MAX) {
    samplerTableRootParamIndex = static_cast<UINT>(rootParams.size());

    D3D12_ROOT_PARAMETER samplerTableParam = {};
    samplerTableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    samplerTableParam.DescriptorTable.NumDescriptorRanges = 1;
    samplerTableParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[samplerRangeIndex];
    samplerTableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams.push_back(samplerTableParam);
  }

  // Add UAV descriptor table if shader uses any UAV slots
  if (!key.usedUAVSlots.empty() && uavRangeIndex != SIZE_MAX) {
    uavTableRootParamIndex = static_cast<UINT>(rootParams.size());

    D3D12_ROOT_PARAMETER uavTableParam = {};
    uavTableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    uavTableParam.DescriptorTable.NumDescriptorRanges = 1;
    uavTableParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[uavRangeIndex];
    uavTableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParams.push_back(uavTableParam);
  }

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
