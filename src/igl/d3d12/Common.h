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

// Set to 1 to enable verbose logging (hot-path logs, detailed state tracking, etc.)
// This is disabled by default to reduce log volume.
#define IGL_D3D12_DEBUG_VERBOSE 0

namespace igl::d3d12 {

// Configuration structure for D3D12 backend.
// Centralizes all size-related configuration with documented rationale.
struct D3D12ContextConfig {
  // === Frame Buffering ===
  // Rationale: Triple buffering (3 frames) provides optimal GPU/CPU parallelism on modern hardware
  // while maintaining reasonable memory overhead. Reducing to 2 can save memory on constrained
  // devices but may reduce throughput. Increasing beyond 3 provides minimal benefit.
  // D3D12 spec: Minimum 2, recommended 2-3 for flip model swapchains
  //
  // LIMITATION: Currently fixed at 3 due to fixed-size arrays (frameContexts_, renderTargets_).
  // Attempting to change this value will be clamped by validate(). To enable true configurability,
  // D3D12Context must be refactored to use std::vector instead of fixed-size arrays.
  uint32_t maxFramesInFlight = 3;

  // === Descriptor Heap Sizes (Per-Frame Shader-Visible) ===
  // Rationale: Following Microsoft MiniEngine pattern for dynamic per-frame allocation
  // Samplers: D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE (2048) is the hardware limit.
  // D3D12 spec limits: CBV/SRV/UAV up to 1,000,000, Samplers max 2048
  uint32_t samplerHeapSize = 2048;       // Total sampler descriptors per frame (D3D12 spec limit)

  // === CBV/SRV/UAV Dynamic Heap Growth ===
  // Rationale: Prevents unbounded memory growth while supporting complex scenes
  // Starts with one page, can grow up to maxHeapPages as needed
  // 16 pages × 1024 descriptors = 16,384 max descriptors per frame
  // This supports ~500-1000 draw calls per frame with typical descriptor usage patterns
  uint32_t descriptorsPerPage = 1024;    // CBV/SRV/UAV descriptors per heap page
  uint32_t maxHeapPages = 16;            // Maximum pages per frame (total capacity = pages × descriptorsPerPage)

  // Pre-allocation policy for descriptor pages.
  // Rationale: Following Vulkan fail-fast pattern to prevent mid-frame descriptor invalidation.
  // When true: All maxHeapPages are pre-allocated at init (recommended).
  // When false: Only 1 page pre-allocated at init (minimal memory footprint).
  // Both modes fail-fast when pages are exhausted - no dynamic growth to prevent descriptor invalidation.
  // Default: true for safety (matches Vulkan behavior and supports complex scenes).
  bool preAllocateDescriptorPages = true;

  // DEPRECATED: Use descriptorsPerPage instead
  // This field is kept for backward compatibility but has the same value as descriptorsPerPage
  uint32_t cbvSrvUavHeapSize = 1024;     // Alias for descriptorsPerPage (deprecated)

  // === CPU-Visible Descriptor Heaps (Static) ===
  // Rationale: RTVs/DSVs are created once per texture and persist across frames
  // 256 RTVs: Supports ~128 textures with mips/array layers (typical for games)
  // 128 DSVs: Sufficient for depth buffers, shadow maps, and multi-pass rendering
  // These values should be tuned based on application texture usage patterns
  uint32_t rtvHeapSize = 256;
  uint32_t dsvHeapSize = 128;

  // === Upload Ring Buffer ===
  // Rationale: 128MB provides good balance for streaming resources (textures, constant buffers)
  // Smaller values (64MB) reduce memory footprint but increase allocation failures
  // Larger values (256MB) reduce failures but waste memory on simple scenes
  // Microsoft MiniEngine uses similar sizes (64-256MB range)
  uint64_t uploadRingBufferSize = 128 * 1024 * 1024;  // 128 MB

  // === Validation Helpers ===
  // Clamp values to D3D12 spec limits and provide warnings for unusual configurations
  void validate() {
    // Frame buffering: Allow 2-4 buffers (double/triple/quad buffering)
    // T43: Now that renderTargets_ and frameContexts_ are std::vector, we can support runtime counts.
    // Practical range: 2 (double-buffer, higher latency), 3 (triple-buffer, balanced), 4 (lower latency, more memory)
    // Note: DXGI may adjust the requested count; actual runtime count comes from GetDesc1().
    constexpr uint32_t kMinFrames = 2;
    constexpr uint32_t kMaxFrames = 4;
    if (maxFramesInFlight < kMinFrames || maxFramesInFlight > kMaxFrames) {
      IGL_LOG_ERROR("D3D12ContextConfig: maxFramesInFlight=%u out of range [%u, %u], clamping to %u\n",
                    maxFramesInFlight, kMinFrames, kMaxFrames,
                    (maxFramesInFlight < kMinFrames) ? kMinFrames : kMaxFrames);
      maxFramesInFlight = (maxFramesInFlight < kMinFrames) ? kMinFrames : kMaxFrames;
    }

    // Sampler heap: Use D3D12 constant instead of magic number
    if (samplerHeapSize > D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE) {
      IGL_LOG_INFO("D3D12ContextConfig: samplerHeapSize=%u exceeds D3D12 limit (%u), clamping\n",
                   samplerHeapSize, D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE);
      samplerHeapSize = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
    }

    // Descriptor page limits: Prevent absurd/invalid values
    if (descriptorsPerPage == 0) {
      IGL_LOG_ERROR("D3D12ContextConfig: descriptorsPerPage=0 is invalid, setting to 1024\n");
      descriptorsPerPage = 1024;
    }
    if (maxHeapPages == 0) {
      IGL_LOG_ERROR("D3D12ContextConfig: maxHeapPages=0 is invalid, setting to 16\n");
      maxHeapPages = 16;
    }

    // CBV/SRV/UAV heap: D3D12 spec limit (generic, tier-independent upper bound)
    // Note: Actual device limits may be lower depending on resource binding tier;
    // use CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS) for precise caps
    constexpr uint32_t kMaxCbvSrvUavDescriptors = 1000000;
    if (descriptorsPerPage > kMaxCbvSrvUavDescriptors) {
      IGL_LOG_INFO("D3D12ContextConfig: descriptorsPerPage=%u exceeds D3D12 limit (%u), clamping\n",
                   descriptorsPerPage, kMaxCbvSrvUavDescriptors);
      descriptorsPerPage = kMaxCbvSrvUavDescriptors;
    }

    // Keep deprecated cbvSrvUavHeapSize in sync with descriptorsPerPage
    cbvSrvUavHeapSize = descriptorsPerPage;

    // Upload ring buffer: Warn if too small (may cause allocation failures)
    constexpr uint64_t kMinRecommendedSize = 32 * 1024 * 1024;  // 32 MB
    if (uploadRingBufferSize < kMinRecommendedSize) {
      IGL_LOG_INFO("D3D12ContextConfig: uploadRingBufferSize=%llu MB is small, "
                   "may cause allocation failures (recommended minimum: %llu MB)\n",
                   uploadRingBufferSize / (1024 * 1024), kMinRecommendedSize / (1024 * 1024));
    }
  }

  // === Preset Configurations ===
  // Factory methods for common use cases

  // Default configuration (balanced for most applications)
  static D3D12ContextConfig defaultConfig() {
    return D3D12ContextConfig{};  // Uses default member initializers
  }

  // Low memory configuration (mobile, integrated GPUs, constrained devices)
  static D3D12ContextConfig lowMemoryConfig() {
    D3D12ContextConfig config;
    config.maxFramesInFlight = 2;        // Double-buffering to reduce memory (T43)
    config.descriptorsPerPage = 512;     // Smaller pages
    config.cbvSrvUavHeapSize = 512;      // Keep in sync (deprecated field)
    config.maxHeapPages = 8;             // Fewer pages (total: 512 × 8 = 4K descriptors)
    config.rtvHeapSize = 128;            // Fewer RTVs
    config.dsvHeapSize = 64;             // Fewer DSVs
    config.uploadRingBufferSize = 64 * 1024 * 1024;  // 64 MB
    config.validate();
    return config;
  }

  // High performance configuration (discrete GPUs, desktop, complex scenes)
  static D3D12ContextConfig highPerformanceConfig() {
    D3D12ContextConfig config;
    config.maxFramesInFlight = 3;        // Triple-buffering (balanced, default) (T43)
    config.descriptorsPerPage = 2048;    // Larger pages
    config.cbvSrvUavHeapSize = 2048;     // Keep in sync (deprecated field)
    config.maxHeapPages = 32;            // More pages (total: 2048 × 32 = 64K descriptors)
    config.rtvHeapSize = 512;            // More RTVs for render targets
    config.dsvHeapSize = 256;            // More DSVs for shadow maps
    config.uploadRingBufferSize = 256 * 1024 * 1024;  // 256 MB
    config.validate();
    return config;
  }
};

// Default frame buffering count (triple buffering).
// T43: D3D12Context now uses runtime swapchainBufferCount_ queried from the swapchain.
// This constant serves as the default value for D3D12ContextConfig::maxFramesInFlight
// and is used by headless contexts (which have no swapchain to query).
// Applications can configure 2-4 buffers via D3D12ContextConfig::maxFramesInFlight.
constexpr uint32_t kMaxFramesInFlight = 3;

// Maximum number of descriptor sets (matching IGL's Vulkan backend)
constexpr uint32_t kMaxDescriptorSets = 4;

// Maximum number of samplers; increased to D3D12 spec limit to support complex scenes.
// D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE is defined as 2048 in d3d12.h.
constexpr uint32_t kMaxSamplers = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;

// Descriptor heap sizes (per-frame shader-visible heaps)
// Following Microsoft MiniEngine pattern for dynamic per-frame allocation
constexpr uint32_t kCbvSrvUavHeapSize = 1024;  // CBV/SRV/UAV descriptors per page
constexpr uint32_t kSamplerHeapSize = kMaxSamplers;  // Sampler descriptors per frame

// Dynamic heap growth limits (prevent unbounded memory usage).
constexpr uint32_t kDescriptorsPerPage = kCbvSrvUavHeapSize;  // 1024 descriptors per page
constexpr uint32_t kMaxHeapPages = 16;  // Maximum 16 pages = 16K descriptors per frame
constexpr uint32_t kMaxDescriptorsPerFrame = kMaxHeapPages * kDescriptorsPerPage;  // 16384 total

// Maximum number of vertex attributes (D3D12 spec limit).
// Uses D3D12 spec constant instead of a hard-coded value.
constexpr uint32_t kMaxVertexAttributes = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;  // 32

// Normalized error macros - single log per error (no double logging).
// Debug builds: IGL_DEBUG_ASSERT logs via _IGLDebugAbort
// Release builds: IGL_LOG_ERROR provides visibility
#if IGL_DEBUG_ABORT_ENABLED
  #define D3D12_CHECK(func)                                                  \
    do {                                                                     \
      const HRESULT d3d12_check_result = (func);                             \
      if (FAILED(d3d12_check_result)) {                                      \
        IGL_DEBUG_ASSERT(false, "D3D12 API call failed: %s, HRESULT: 0x%08X", \
                         #func,                                              \
                         static_cast<unsigned int>(d3d12_check_result));     \
      }                                                                      \
    } while (0)

  #define D3D12_CHECK_RETURN(func)                                           \
    do {                                                                     \
      const HRESULT d3d12_check_result = (func);                             \
      if (FAILED(d3d12_check_result)) {                                      \
        IGL_DEBUG_ASSERT(false, "D3D12 API call failed: %s, HRESULT: 0x%08X", \
                         #func,                                              \
                         static_cast<unsigned int>(d3d12_check_result));     \
        return getResultFromHRESULT(d3d12_check_result);                     \
      }                                                                      \
    } while (0)
#else
  #define D3D12_CHECK(func)                                                  \
    do {                                                                     \
      const HRESULT d3d12_check_result = (func);                             \
      if (FAILED(d3d12_check_result)) {                                      \
        IGL_LOG_ERROR("D3D12 API call failed: %s, HRESULT: 0x%08X\n",        \
                      #func,                                                 \
                      static_cast<unsigned int>(d3d12_check_result));        \
      }                                                                      \
    } while (0)

  #define D3D12_CHECK_RETURN(func)                                           \
    do {                                                                     \
      const HRESULT d3d12_check_result = (func);                             \
      if (FAILED(d3d12_check_result)) {                                      \
        IGL_LOG_ERROR("D3D12 API call failed: %s, HRESULT: 0x%08X\n",        \
                      #func,                                                 \
                      static_cast<unsigned int>(d3d12_check_result));        \
        return getResultFromHRESULT(d3d12_check_result);                     \
      }                                                                      \
    } while (0)
#endif

// Verbose logging macro (hot-path logs, detailed state tracking).
// Only logs when IGL_D3D12_DEBUG_VERBOSE is enabled (disabled by default)
#if IGL_D3D12_DEBUG_VERBOSE
  #define IGL_D3D12_LOG_VERBOSE(format, ...) IGL_LOG_INFO(format, ##__VA_ARGS__)
#else
  #define IGL_D3D12_LOG_VERBOSE(format, ...) ((void)0)
#endif

// Command logging macro (D3D12 API command traces).
// Only logs when IGL_D3D12_PRINT_COMMANDS is enabled (disabled by default)
// Use for command recording, state transitions, and D3D12 API call traces
// Note: Treated as INFO-level severity but controlled separately from DEBUG_VERBOSE
// to allow independent toggling of command traces vs general verbose output
#if IGL_D3D12_PRINT_COMMANDS
  #define IGL_D3D12_LOG_CMD(format, ...) IGL_LOG_INFO(format, ##__VA_ARGS__)
#else
  #define IGL_D3D12_LOG_CMD(format, ...) ((void)0)
#endif

// Convert HRESULT to IGL Result
inline Result getResultFromHRESULT(HRESULT hr) {
  if (SUCCEEDED(hr)) {
    return Result(Result::Code::Ok);
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
  default: {
    // Include HRESULT code for better debugging of unexpected errors.
    char buf[64];
    snprintf(buf, sizeof(buf), "D3D12 error (hr=0x%08X)", static_cast<unsigned>(hr));
    return Result(Result::Code::RuntimeError, buf);
  }
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

// Align value to specified alignment (must be power-of-two)
// Template allows use with different integer types (UINT64, size_t, etc.)
// IMPORTANT: alignment must be a power of 2 (e.g., 256, 4096, 65536)
template<typename T>
inline T AlignUp(T value, T alignment) {
  IGL_DEBUG_ASSERT((alignment & (alignment - 1)) == 0, "AlignUp: alignment must be power-of-two");
  return (value + alignment - 1) & ~(alignment - 1);
}

// Hash combining utility (boost::hash_combine pattern)
// Used for hashing complex structures like root signatures and pipeline descriptors
template<typename T>
inline void hashCombine(size_t& seed, const T& value) {
  seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// Feature level to string conversion
inline const char* featureLevelToString(D3D_FEATURE_LEVEL level) {
  switch (level) {
    case D3D_FEATURE_LEVEL_12_2: return "12.2";
    case D3D_FEATURE_LEVEL_12_1: return "12.1";
    case D3D_FEATURE_LEVEL_12_0: return "12.0";
    case D3D_FEATURE_LEVEL_11_1: return "11.1";
    case D3D_FEATURE_LEVEL_11_0: return "11.0";
    default: return "Unknown";
  }
}

// Shader target helper.
// Convert D3D_SHADER_MODEL enum to shader target string (e.g., "vs_6_6", "ps_5_1").
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
