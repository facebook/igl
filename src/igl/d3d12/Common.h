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

// Frame buffering count (2-3 for double/triple buffering)
constexpr uint32_t kMaxFramesInFlight = 3;

// Maximum number of descriptor sets (matching IGL's Vulkan backend)
constexpr uint32_t kMaxDescriptorSets = 4;

// Maximum number of samplers (increased from 16 to accommodate ImGui with multiple texture views)
constexpr uint32_t kMaxSamplers = 32;

// Maximum number of vertex attributes
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
TextureFormat dxgiFormatToTextureFormat(DXGI_FORMAT format);

} // namespace igl::d3d12

#endif // IGL_D3D12_COMMON_H
