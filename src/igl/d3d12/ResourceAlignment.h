/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/d3d12/D3D12Headers.h>

// B-007: D3D12 resource alignment utilities
// These utilities provide alignment validation and helper functions for D3D12 resource creation.
// While the current implementation uses CreateCommittedResource (which handles alignment
// automatically), these utilities prepare for future placed resource/memory pooling support
// and provide diagnostic capabilities for debugging alignment issues.

namespace igl::d3d12 {
namespace ResourceAlignment {

// D3D12 alignment constants
// Note: We use different names than the D3D12 SDK constants to avoid redefinition errors
const UINT64 kDefaultResourcePlacementAlignment = 65536;  // 64KB for textures and MSAA resources
const UINT64 kSmallResourcePlacementAlignment = 4096;     // 4KB for non-MSAA buffers
const UINT64 kTextureDataPlacementAlignment = 512;        // 512 bytes for texture data (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT)
const UINT64 kTextureDataPitchAlignment = 256;            // 256 bytes for texture row pitch (D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)

// Query actual allocation info for resource
// This wraps ID3D12Device::GetResourceAllocationInfo and provides the actual size and
// alignment requirements that the driver will use for a given resource description.
inline D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo(
    ID3D12Device* device,
    const D3D12_RESOURCE_DESC& desc) {
  return device->GetResourceAllocationInfo(0, 1, &desc);
}

// Align value to specified alignment
// Template allows use with different integer types (UINT64, size_t, etc.)
template<typename T>
inline T AlignUp(T value, T alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

template<typename T>
inline bool IsAligned(T value, T alignment) {
  return (value & (alignment - 1)) == 0;
}

// Validate that offset meets alignment requirement for resource type
// Returns true if offset is properly aligned, false otherwise.
// If outRequiredAlignment is provided, it will be set to the alignment requirement.
inline bool ValidateResourcePlacementAlignment(
    UINT64 offset,
    const D3D12_RESOURCE_DESC& desc,
    UINT64* outRequiredAlignment = nullptr) {

  UINT64 requiredAlignment = kDefaultResourcePlacementAlignment;

  // Determine alignment based on resource type
  if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
    // Buffers can use 4KB alignment if not MSAA
    if (desc.SampleDesc.Count == 1) {
      requiredAlignment = kSmallResourcePlacementAlignment;
    }
  } else {
    // Textures always need 64KB alignment
    requiredAlignment = kDefaultResourcePlacementAlignment;
  }

  if (outRequiredAlignment) {
    *outRequiredAlignment = requiredAlignment;
  }

  return IsAligned(offset, requiredAlignment);
}

// Log allocation info for diagnostics
// Provides visibility into actual allocation sizes which may differ from requested sizes
inline void LogResourceAllocationInfo(
    const char* resourceType,
    const D3D12_RESOURCE_ALLOCATION_INFO& allocInfo) {
  IGL_D3D12_LOG_VERBOSE("D3D12: %s allocation: size=%llu bytes (%.2f MB), alignment=%llu bytes\n",
               resourceType,
               allocInfo.SizeInBytes,
               allocInfo.SizeInBytes / (1024.0 * 1024.0),
               allocInfo.Alignment);
}

// B-007: Placed Resource Alignment Guidelines
//
// When implementing placed resource allocation (memory pooling), ensure:
//
// 1. Query allocation info BEFORE creating heap:
//    auto allocInfo = device->GetResourceAllocationInfo(0, 1, &desc);
//
// 2. Align heap offset to required alignment:
//    UINT64 alignedOffset = AlignUp(currentOffset, allocInfo.Alignment);
//
// 3. Validate alignment before CreatePlacedResource:
//    if (!IsAligned(alignedOffset, allocInfo.Alignment)) {
//      return error;
//    }
//
// 4. Track allocated ranges to prevent overlap:
//    struct PlacedResourceEntry {
//      UINT64 offset;
//      UINT64 size;
//      ID3D12Resource* resource;
//    };
//
// 5. Standard alignments:
//    - Textures: 64KB (65536 bytes)
//    - MSAA resources: 64KB
//    - Small buffers: 4KB (4096 bytes) if not MSAA
//    - Texture data: 512 bytes for CopyTextureRegion
//    - Texture row pitch: 256 bytes

} // namespace ResourceAlignment
} // namespace igl::d3d12
