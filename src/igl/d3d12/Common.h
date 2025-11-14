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
#include <igl/Shader.h>
#include <igl/TextureFormat.h>
#include <igl/d3d12/D3D12Headers.h>

// Set to 1 to see verbose debug console logs with D3D12 commands
#define IGL_D3D12_PRINT_COMMANDS 0

namespace igl::d3d12 {

// Frame buffering count (2-3 for double/triple buffering)
constexpr uint32_t kMaxFramesInFlight = 3;

// Maximum number of descriptor sets (matching IGL's Vulkan backend)
constexpr uint32_t kMaxDescriptorSets = 4;

// Maximum number of samplers (TASK_P2_DX12-FIND-08: Increased to D3D12 spec limit to support complex scenes)
// D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE is defined as 2048 in d3d12.h
constexpr uint32_t kMaxSamplers = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;

// Descriptor heap sizes (per-frame shader-visible heaps)
// Following Microsoft MiniEngine pattern for dynamic per-frame allocation
constexpr uint32_t kCbvSrvUavHeapSize = 1024;  // CBV/SRV/UAV descriptors per page
constexpr uint32_t kSamplerHeapSize = kMaxSamplers;  // Sampler descriptors per frame

// C-001: Dynamic heap growth limits (prevent unbounded memory usage)
constexpr uint32_t kDescriptorsPerPage = kCbvSrvUavHeapSize;  // 1024 descriptors per page
constexpr uint32_t kMaxHeapPages = 16;  // Maximum 16 pages = 16K descriptors per frame
constexpr uint32_t kMaxDescriptorsPerFrame = kMaxHeapPages * kDescriptorsPerPage;  // 16384 total

// Maximum number of vertex attributes (D3D12 spec limit)
// H-015: Use D3D12 spec constant instead of hard-coded value
constexpr uint32_t kMaxVertexAttributes = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;  // 32

// Macro to check HRESULT and convert to IGL Result
#define D3D12_CHECK(func)                                                    \
  {                                                                          \
    const HRESULT d3d12_check_result = func;                                 \
    if (FAILED(d3d12_check_result)) {                                        \
      IGL_DEBUG_ASSERT(false, "D3D12 API call failed: %s, HRESULT: 0x%08X",  \
                       #func,                                                \
                       static_cast<unsigned int>(d3d12_check_result));       \
      IGL_LOG_ERROR("D3D12 API call failed: %s\n  HRESULT: 0x%08X\n",        \
                    #func,                                                   \
                    static_cast<unsigned int>(d3d12_check_result));          \
    }                                                                        \
  }

// Macro to check HRESULT and return IGL Result on failure
#define D3D12_CHECK_RETURN(func)                                             \
  {                                                                          \
    const HRESULT d3d12_check_result = func;                                 \
    if (FAILED(d3d12_check_result)) {                                        \
      IGL_DEBUG_ASSERT(false, "D3D12 API call failed: %s, HRESULT: 0x%08X",  \
                       #func,                                                \
                       static_cast<unsigned int>(d3d12_check_result));       \
      IGL_LOG_ERROR("D3D12 API call failed: %s\n  HRESULT: 0x%08X\n",        \
                    #func,                                                   \
                    static_cast<unsigned int>(d3d12_check_result));          \
      return getResultFromHRESULT(d3d12_check_result);                       \
    }                                                                        \
  }

// C-006: Validate D3D12 descriptor handles before use
#if IGL_DEBUG
  #define IGL_D3D12_VALIDATE_CPU_HANDLE(handle, name) \
    do { \
      if ((handle).ptr == 0) { \
        IGL_LOG_ERROR("D3D12: Invalid CPU descriptor handle (%s) - ptr is null\n", name); \
        IGL_DEBUG_ASSERT(false, "Invalid CPU descriptor handle"); \
      } \
    } while (0)

  #define IGL_D3D12_VALIDATE_GPU_HANDLE(handle, name) \
    do { \
      if ((handle).ptr == 0) { \
        IGL_LOG_ERROR("D3D12: Invalid GPU descriptor handle (%s) - ptr is null\n", name); \
        IGL_DEBUG_ASSERT(false, "Invalid GPU descriptor handle"); \
      } \
    } while (0)
#else
  // No-op in release builds (performance-critical paths)
  #define IGL_D3D12_VALIDATE_CPU_HANDLE(handle, name) ((void)0)
  #define IGL_D3D12_VALIDATE_GPU_HANDLE(handle, name) ((void)0)
#endif

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

// Shader target helper (H-009)
// Convert D3D_SHADER_MODEL enum to shader target string (e.g., "vs_6_6", "ps_5_1")
inline std::string getShaderTarget(D3D_SHADER_MODEL shaderModel, ShaderStage stage) {
  // Extract major and minor version from D3D_SHADER_MODEL enum
  // Format: 0xMm where M = major, m = minor (e.g., 0x66 = SM 6.6, 0x51 = SM 5.1)
  int major = (shaderModel >> 4) & 0xF;
  int minor = shaderModel & 0xF;

  // Get stage prefix
  const char* stagePrefix = nullptr;
  switch (stage) {
  case ShaderStage::Vertex:
    stagePrefix = "vs";
    break;
  case ShaderStage::Fragment:
    stagePrefix = "ps";  // DirectX uses "ps" for pixel/fragment shaders
    break;
  case ShaderStage::Compute:
    stagePrefix = "cs";
    break;
  default:
    return "";
  }

  // Build target string (e.g., "vs_6_6", "ps_5_1", "cs_6_0")
  char target[16];
  snprintf(target, sizeof(target), "%s_%d_%d", stagePrefix, major, minor);
  return std::string(target);
}

} // namespace igl::d3d12

#endif // IGL_D3D12_COMMON_H
