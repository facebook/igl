/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/d3d12/Common.h>
#include <igl/d3d12/ShaderModule.h>
#include <vector>
#include <algorithm>

namespace igl::d3d12 {

/**
 * @brief Key structure for root signature cache lookup based on shader resource usage
 *
 * This structure captures the essential shader resource requirements needed to construct
 * a compatible root signature. It enables Vulkan-style dynamic root signature selection
 * where the root signature is chosen based on actual shader resource usage rather than
 * being globally fixed.
 *
 * The key includes:
 * - Push constant configuration (slot and size)
 * - Resource slot usage (CBV/SRV/UAV/Sampler ranges)
 * - Flags for shader visibility and optimization
 *
 * Root signatures with the same key are compatible and can be reused across pipelines.
 */
struct D3D12RootSignatureKey {
  // Push constants configuration
  bool hasPushConstants = false;
  UINT pushConstantSlot = UINT_MAX;  // Which b# register
  UINT pushConstantSize = 0;         // Size in 32-bit values

  // Resource slot ranges (sorted for consistent hashing)
  std::vector<UINT> usedCBVSlots;
  std::vector<UINT> usedSRVSlots;
  std::vector<UINT> usedUAVSlots;
  std::vector<UINT> usedSamplerSlots;

  // Maximum slot indices (for determining descriptor table sizes)
  UINT maxCBVSlot = 0;
  UINT maxSRVSlot = 0;
  UINT maxUAVSlot = 0;
  UINT maxSamplerSlot = 0;

  // Root signature flags
  D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  /**
   * @brief Construct key from vertex + fragment shader reflection
   *
   * Merges resource usage from both shaders to create a unified key.
   * Handles push constant slot conflicts (prefers vertex shader if both use different slots).
   */
  static D3D12RootSignatureKey fromShaderReflection(
      const ShaderModule::ShaderReflectionInfo* vsReflection,
      const ShaderModule::ShaderReflectionInfo* psReflection);

  /**
   * @brief Construct key from compute shader reflection
   */
  static D3D12RootSignatureKey fromShaderReflection(
      const ShaderModule::ShaderReflectionInfo* csReflection);

  bool operator==(const D3D12RootSignatureKey& other) const {
    return hasPushConstants == other.hasPushConstants &&
           pushConstantSlot == other.pushConstantSlot &&
           pushConstantSize == other.pushConstantSize &&
           usedCBVSlots == other.usedCBVSlots &&
           usedSRVSlots == other.usedSRVSlots &&
           usedUAVSlots == other.usedUAVSlots &&
           usedSamplerSlots == other.usedSamplerSlots &&
           maxCBVSlot == other.maxCBVSlot &&
           maxSRVSlot == other.maxSRVSlot &&
           maxUAVSlot == other.maxUAVSlot &&
           maxSamplerSlot == other.maxSamplerSlot &&
           flags == other.flags;
  }

  struct HashFunction {
    size_t operator()(const D3D12RootSignatureKey& key) const {
      size_t hash = 0;

      // Hash push constants
      hashCombine(hash, key.hasPushConstants ? 1 : 0);
      hashCombine(hash, static_cast<size_t>(key.pushConstantSlot));
      hashCombine(hash, static_cast<size_t>(key.pushConstantSize));

      // Hash resource slots
      for (UINT slot : key.usedCBVSlots) {
        hashCombine(hash, static_cast<size_t>(slot));
      }
      for (UINT slot : key.usedSRVSlots) {
        hashCombine(hash, static_cast<size_t>(slot));
      }
      for (UINT slot : key.usedUAVSlots) {
        hashCombine(hash, static_cast<size_t>(slot));
      }
      for (UINT slot : key.usedSamplerSlots) {
        hashCombine(hash, static_cast<size_t>(slot));
      }

      // Hash max slots
      hashCombine(hash, static_cast<size_t>(key.maxCBVSlot));
      hashCombine(hash, static_cast<size_t>(key.maxSRVSlot));
      hashCombine(hash, static_cast<size_t>(key.maxUAVSlot));
      hashCombine(hash, static_cast<size_t>(key.maxSamplerSlot));

      // Hash flags
      hashCombine(hash, static_cast<size_t>(key.flags));

      return hash;
    }
  };

private:
  // Helper to merge two slot vectors and sort
  static std::vector<UINT> mergeAndSort(const std::vector<UINT>& a, const std::vector<UINT>& b) {
    std::vector<UINT> result = a;
    result.insert(result.end(), b.begin(), b.end());
    std::sort(result.begin(), result.end());
    // Remove duplicates
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
  }
};

// Implementation of fromShaderReflection for graphics pipeline
inline D3D12RootSignatureKey D3D12RootSignatureKey::fromShaderReflection(
    const ShaderModule::ShaderReflectionInfo* vsReflection,
    const ShaderModule::ShaderReflectionInfo* psReflection) {
  D3D12RootSignatureKey key;

  // Merge push constants (prefer vertex shader if conflict)
  if (vsReflection && vsReflection->hasPushConstants) {
    key.hasPushConstants = true;
    key.pushConstantSlot = vsReflection->pushConstantSlot;
    key.pushConstantSize = vsReflection->pushConstantSize;
  } else if (psReflection && psReflection->hasPushConstants) {
    key.hasPushConstants = true;
    key.pushConstantSlot = psReflection->pushConstantSlot;
    key.pushConstantSize = psReflection->pushConstantSize;
  }

  // Merge resource slots
  if (vsReflection && psReflection) {
    key.usedCBVSlots = mergeAndSort(vsReflection->usedCBVSlots, psReflection->usedCBVSlots);
    key.usedSRVSlots = mergeAndSort(vsReflection->usedSRVSlots, psReflection->usedSRVSlots);
    key.usedUAVSlots = mergeAndSort(vsReflection->usedUAVSlots, psReflection->usedUAVSlots);
    key.usedSamplerSlots = mergeAndSort(vsReflection->usedSamplerSlots, psReflection->usedSamplerSlots);

    key.maxCBVSlot = std::max(vsReflection->maxCBVSlot, psReflection->maxCBVSlot);
    key.maxSRVSlot = std::max(vsReflection->maxSRVSlot, psReflection->maxSRVSlot);
    key.maxUAVSlot = std::max(vsReflection->maxUAVSlot, psReflection->maxUAVSlot);
    key.maxSamplerSlot = std::max(vsReflection->maxSamplerSlot, psReflection->maxSamplerSlot);
  } else if (vsReflection) {
    key.usedCBVSlots = vsReflection->usedCBVSlots;
    key.usedSRVSlots = vsReflection->usedSRVSlots;
    key.usedUAVSlots = vsReflection->usedUAVSlots;
    key.usedSamplerSlots = vsReflection->usedSamplerSlots;
    key.maxCBVSlot = vsReflection->maxCBVSlot;
    key.maxSRVSlot = vsReflection->maxSRVSlot;
    key.maxUAVSlot = vsReflection->maxUAVSlot;
    key.maxSamplerSlot = vsReflection->maxSamplerSlot;
  } else if (psReflection) {
    key.usedCBVSlots = psReflection->usedCBVSlots;
    key.usedSRVSlots = psReflection->usedSRVSlots;
    key.usedUAVSlots = psReflection->usedUAVSlots;
    key.usedSamplerSlots = psReflection->usedSamplerSlots;
    key.maxCBVSlot = psReflection->maxCBVSlot;
    key.maxSRVSlot = psReflection->maxSRVSlot;
    key.maxUAVSlot = psReflection->maxUAVSlot;
    key.maxSamplerSlot = psReflection->maxSamplerSlot;
  }

  return key;
}

// Implementation of fromShaderReflection for compute pipeline
inline D3D12RootSignatureKey D3D12RootSignatureKey::fromShaderReflection(
    const ShaderModule::ShaderReflectionInfo* csReflection) {
  D3D12RootSignatureKey key;

  if (!csReflection) {
    return key;
  }

  // Copy push constants
  key.hasPushConstants = csReflection->hasPushConstants;
  key.pushConstantSlot = csReflection->pushConstantSlot;
  key.pushConstantSize = csReflection->pushConstantSize;

  // Copy resource slots
  key.usedCBVSlots = csReflection->usedCBVSlots;
  key.usedSRVSlots = csReflection->usedSRVSlots;
  key.usedUAVSlots = csReflection->usedUAVSlots;
  key.usedSamplerSlots = csReflection->usedSamplerSlots;

  key.maxCBVSlot = csReflection->maxCBVSlot;
  key.maxSRVSlot = csReflection->maxSRVSlot;
  key.maxUAVSlot = csReflection->maxUAVSlot;
  key.maxSamplerSlot = csReflection->maxSamplerSlot;

  // Compute shaders don't need input assembler
  key.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

  return key;
}

} // namespace igl::d3d12
