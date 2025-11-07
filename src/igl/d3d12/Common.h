/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifndef IGL_D3D12_COMMON_H
#define IGL_D3D12_COMMON_H

#include <cassert>
#include <utility>

#include <igl/Common.h>
#include <igl/Macros.h>
#include <igl/TextureFormat.h>
#include <igl/d3d12/D3D12Headers.h>

// Set to 1 to see verbose debug console logs with D3D12 commands
#define IGL_D3D12_PRINT_COMMANDS 0

namespace igl::d3d12 {

// ===== D3D12 Backend Configuration Limits =====
//
// These constants define the IGL D3D12 backend's operational limits.
// They are validated against actual device capabilities at runtime (see Device::validateDeviceLimits).
//
// Design Principles:
// - Conservative defaults that work on all D3D12 Feature Level 11.0+ hardware
// - Balance between compatibility and performance
// - Aligned with IGL's cross-platform design (matching Vulkan backend where applicable)
//
// Device Capability Validation (P2_DX12-018):
// - At Device initialization, actual hardware capabilities are queried via CheckFeatureSupport
// - Limits are validated against D3D12 specifications and logged for diagnostics
// - No runtime failures on capability mismatches; warnings are logged instead

// Frame buffering count for CPU/GPU parallelism (2-3 for double/triple buffering)
// Design Choice: 3 frames allows optimal pipeline parallelism on most hardware
// No direct D3D12 limit; validated against available system memory at runtime
constexpr uint32_t kMaxFramesInFlight = 3;

// Maximum number of descriptor sets (matching IGL's Vulkan backend)
// Design Choice: Matches Vulkan's typical descriptor set count for cross-backend consistency
// Validated against: Resource Binding Tier (Tier 1/2/3 affect descriptor table limits)
constexpr uint32_t kMaxDescriptorSets = 4;

// Maximum number of samplers (increased from 16 to accommodate ImGui with multiple texture views)
// D3D12 Limit: D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE = 2048
// Current Value: 32 (conservative, well within spec)
// Validated at runtime to ensure kMaxSamplers <= 2048
constexpr uint32_t kMaxSamplers = 32;

// Maximum number of vertex attributes
// D3D12 Limit: D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT = 32
// Current Value: 16 (conservative, matches common usage patterns)
// Validated at runtime to ensure kMaxVertexAttributes <= 32
constexpr uint32_t kMaxVertexAttributes = 16;

// Macro to check HRESULT and convert to IGL Result
#define D3D12_CHECK(func)                                                    \
  {                                                                          \
    const HRESULT d3d12_check_result = func;                                 \
    if (FAILED(d3d12_check_result)) {                                        \
      IGL_DEBUG_ABORT("D3D12 API call failed: %s\n  HRESULT: 0x%08X\n",      \
                      #func,                                                 \
                      static_cast<unsigned int>(d3d12_check_result));        \
    }                                                                        \
  }

// Macro to check HRESULT and return IGL Result on failure
#define D3D12_CHECK_RETURN(func)                                             \
  {                                                                          \
    const HRESULT d3d12_check_result = func;                                 \
    if (FAILED(d3d12_check_result)) {                                        \
      IGL_DEBUG_ABORT("D3D12 API call failed: %s\n  HRESULT: 0x%08X\n",      \
                      #func,                                                 \
                      static_cast<unsigned int>(d3d12_check_result));        \
      return getResultFromHRESULT(d3d12_check_result);                       \
    }                                                                        \
  }

// Convert HRESULT to IGL Result
inline Result getResultFromHRESULT(HRESULT hr) {
  if (SUCCEEDED(hr)) {
    return Result();
  }

  // Map common HRESULT codes to IGL Result codes
  switch (hr) {
  case E_OUTOFMEMORY:
    return Result(Result::Code::RuntimeError, "Out of memory");
  case E_INVALIDARG:
    return Result(Result::Code::ArgumentInvalid, "Invalid argument");
  case E_NOTIMPL:
    return Result(Result::Code::Unimplemented, "Not implemented");
  case DXGI_ERROR_DEVICE_REMOVED:
    return Result(Result::Code::RuntimeError, "Device removed");
  case DXGI_ERROR_DEVICE_RESET:
    return Result(Result::Code::RuntimeError, "Device reset");
  default:
    return Result(Result::Code::RuntimeError, "D3D12 error");
  }
}

// Helper for COM resource release
template<typename T>
void SafeRelease(T*& ptr) {
  if (ptr) {
    ptr->Release();
    ptr = nullptr;
  }
}

// Texture format conversion
DXGI_FORMAT textureFormatToDXGIFormat(TextureFormat format);
DXGI_FORMAT textureFormatToDXGIResourceFormat(TextureFormat format, bool sampledUsage);
DXGI_FORMAT textureFormatToDXGIShaderResourceViewFormat(TextureFormat format);
TextureFormat dxgiFormatToTextureFormat(DXGI_FORMAT format);
bool formatNeedsRgbPadding(TextureFormat format);

} // namespace igl::d3d12

#endif // IGL_D3D12_COMMON_H
