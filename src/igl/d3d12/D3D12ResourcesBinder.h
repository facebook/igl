/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/Common.h>
#include <igl/SamplerState.h>
#include <igl/Texture.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12Headers.h>

namespace igl::d3d12 {

class CommandBuffer;
class PipelineState;

/**
 * @brief Binding state for textures and their associated GPU descriptor handles
 *
 * Stores up to IGL_TEXTURE_SAMPLERS_MAX texture bindings (t0-t15 in HLSL).
 * Each binding stores the texture pointer (for descriptor creation) and the
 * resulting GPU descriptor handle (for root parameter binding).
 */
struct BindingsTextures {
  ITexture* textures[IGL_TEXTURE_SAMPLERS_MAX] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE handles[IGL_TEXTURE_SAMPLERS_MAX] = {};
  uint32_t count = 0;
};

/**
 * @brief Binding state for samplers and their associated GPU descriptor handles
 *
 * Stores up to IGL_TEXTURE_SAMPLERS_MAX sampler bindings (s0-s15 in HLSL).
 * Each binding stores the sampler state pointer (for descriptor creation) and the
 * resulting GPU descriptor handle (for root parameter binding).
 */
struct BindingsSamplers {
  ISamplerState* samplers[IGL_TEXTURE_SAMPLERS_MAX] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE handles[IGL_TEXTURE_SAMPLERS_MAX] = {};
  uint32_t count = 0;
};

/**
 * @brief Binding state for uniform buffers (constant buffers in D3D12)
 *
 * Stores up to IGL_BUFFER_BINDINGS_MAX buffer bindings (b0-b30 in HLSL).
 * D3D12 has two binding methods:
 * - Root CBV (direct GPU virtual address) - used for b0-b1 (legacy/frequent)
 * - CBV descriptor table - used for b2+ (less frequent)
 *
 * This struct stores buffer pointers and GPU virtual addresses/sizes for all bindings.
 * The actual binding method is determined by the pipeline root signature.
 */
struct BindingsBuffers {
  IBuffer* buffers[IGL_BUFFER_BINDINGS_MAX] = {};
  D3D12_GPU_VIRTUAL_ADDRESS addresses[IGL_BUFFER_BINDINGS_MAX] = {};
  size_t offsets[IGL_BUFFER_BINDINGS_MAX] = {};
  size_t sizes[IGL_BUFFER_BINDINGS_MAX] = {};
  uint32_t count = 0;
};

/**
 * @brief Binding state for unordered access views (UAVs)
 *
 * Stores up to IGL_BUFFER_BINDINGS_MAX UAV bindings (u0-u30 in HLSL).
 * Used for storage buffers in compute shaders and writable resources.
 * Each binding stores the buffer pointer, offset, element stride (for descriptor creation),
 * and the resulting GPU descriptor handle (for root parameter binding).
 */
struct BindingsUAVs {
  IBuffer* buffers[IGL_BUFFER_BINDINGS_MAX] = {};
  size_t offsets[IGL_BUFFER_BINDINGS_MAX] = {};
  size_t elementStrides[IGL_BUFFER_BINDINGS_MAX] = {};  // Byte stride per element for structured buffers
  D3D12_GPU_DESCRIPTOR_HANDLE handles[IGL_BUFFER_BINDINGS_MAX] = {};
  uint32_t count = 0;
};

/**
 * @brief Centralized resource binding management for D3D12 command encoders
 *
 * This class consolidates descriptor allocation and resource binding logic that was
 * previously fragmented across RenderCommandEncoder and ComputeCommandEncoder.
 * It follows the Vulkan ResourcesBinder pattern to provide a consistent, centralized
 * interface for binding resources to the GPU pipeline.
 *
 * Key responsibilities:
 * - Cache resource bindings locally until updateBindings() is called
 * - Allocate descriptors from per-frame shader-visible heaps on-demand
 * - Create SRV/UAV/CBV/Sampler descriptors in GPU-visible heaps
 * - Track dirty state to minimize descriptor creation and root parameter updates
 * - Support both graphics and compute pipeline bind points
 *
 * Design principles:
 * - Lazy update: Bindings are cached locally and only applied to GPU on updateBindings()
 * - Dirty tracking: Only update descriptor sets when resources change
 * - Pipeline awareness: Different root signature layouts for graphics vs compute
 * - Per-frame isolation: Uses per-frame descriptor heaps to prevent race conditions
 *
 * Thread-safety: This class is NOT thread-safe. Each encoder should own its own binder.
 *
 * Dependencies:
 * - T01: Correct descriptor binding patterns
 * - T06: Shared helper utilities for descriptor creation
 *
 * Related to Vulkan ResourcesBinder pattern (src/igl/vulkan/ResourcesBinder.h)
 */
class D3D12ResourcesBinder final {
 public:
  /**
   * @brief Initialize the resource binder for a command buffer
   *
   * @param commandBuffer Command buffer to bind resources to (provides context/device access)
   * @param isCompute True for compute pipelines, false for graphics pipelines
   */
  D3D12ResourcesBinder(CommandBuffer& commandBuffer, bool isCompute);

  /**
   * @brief Bind a texture (shader resource view) to a specific slot
   *
   * Creates or updates an SRV descriptor in the per-frame CBV/SRV/UAV heap
   * and caches the GPU handle. The binding is not applied to the command list
   * until updateBindings() is called.
   *
   * @param index Texture slot (t0-t15 in HLSL, 0-based index)
   * @param texture Texture to bind (nullptr to unbind)
   */
  void bindTexture(uint32_t index, ITexture* texture);

  /**
   * @brief Bind a sampler state to a specific slot
   *
   * Creates or updates a sampler descriptor in the per-frame sampler heap
   * and caches the GPU handle. The binding is not applied to the command list
   * until updateBindings() is called.
   *
   * @param index Sampler slot (s0-s15 in HLSL, 0-based index)
   * @param samplerState Sampler state to bind (nullptr to unbind)
   */
  void bindSamplerState(uint32_t index, ISamplerState* samplerState);

  /**
   * @brief Bind a buffer (constant buffer or storage buffer) to a specific slot
   *
   * For uniform buffers (constant buffers):
   * - Stores GPU virtual address for root CBV binding (b0-b1)
   * - Or creates CBV descriptor for descriptor table binding (b2+)
   *
   * For storage buffers:
   * - Creates UAV descriptor in the per-frame CBV/SRV/UAV heap
   * - Requires elementStride for structured buffer descriptor creation
   *
   * The binding is not applied to the command list until updateBindings() is called.
   *
   * @param index Buffer slot (b0-b30 for CBVs, u0-u30 for UAVs in HLSL)
   * @param buffer Buffer to bind (nullptr to unbind)
   * @param offset Offset in bytes into the buffer
   * @param size Size in bytes to bind
   * @param isUAV True to bind as UAV (storage buffer), false for CBV (uniform buffer)
   * @param elementStride For UAVs: byte stride per element for structured buffers (required)
   */
  void bindBuffer(uint32_t index,
                  IBuffer* buffer,
                  size_t offset,
                  size_t size,
                  bool isUAV = false,
                  size_t elementStride = 0);

  /**
   * @brief Apply all pending bindings to the command list
   *
   * This method performs the actual GPU binding work:
   * 1. Creates descriptors for any dirty bindings (textures/samplers/buffers/UAVs)
   * 2. Sets root descriptor tables (SetGraphicsRootDescriptorTable/SetComputeRootDescriptorTable)
   * 3. Sets root constants/root CBVs if applicable
   * 4. Clears dirty flags
   *
   * This should be called before draw/dispatch commands to ensure all bindings are active.
   *
   * @param outResult Optional result for error reporting (e.g., descriptor heap overflow)
   * @return true if bindings applied successfully, false on error
   */
  [[nodiscard]] bool updateBindings(Result* outResult = nullptr);

  /**
   * @brief Reset all bindings and dirty flags
   *
   * Called at the start of a new frame or when switching pipelines to ensure
   * clean binding state. Does not affect the underlying descriptor heaps.
   */
  void reset();

 private:
  /**
   * @brief Bitwise flags for dirty resource types
   *
   * Used to track which resource types have been modified since the last
   * updateBindings() call, allowing us to skip descriptor creation and
   * root parameter updates for unchanged resources.
   */
  enum DirtyFlagBits : uint8_t {
    DirtyFlagBits_Textures = 1 << 0,
    DirtyFlagBits_Samplers = 1 << 1,
    DirtyFlagBits_Buffers = 1 << 2,
    DirtyFlagBits_UAVs = 1 << 3,
  };

  /**
   * @brief Update texture bindings (SRV descriptor table)
   *
   * Creates SRV descriptors for all bound textures in the per-frame heap
   * and sets the root descriptor table parameter.
   *
   * @param cmdList Command list to update
   * @param device D3D12 device for descriptor creation
   * @param outResult Optional result for error reporting
   * @return true on success, false on error
   */
  [[nodiscard]] bool updateTextureBindings(ID3D12GraphicsCommandList* cmdList,
                                           ID3D12Device* device,
                                           Result* outResult);

  /**
   * @brief Update sampler bindings (sampler descriptor table)
   *
   * Creates sampler descriptors for all bound samplers in the per-frame heap
   * and sets the root descriptor table parameter.
   *
   * @param cmdList Command list to update
   * @param device D3D12 device for descriptor creation
   * @param outResult Optional result for error reporting
   * @return true on success, false on error
   */
  [[nodiscard]] bool updateSamplerBindings(ID3D12GraphicsCommandList* cmdList,
                                           ID3D12Device* device,
                                           Result* outResult);

  /**
   * @brief Update buffer bindings (root CBVs or CBV descriptor table)
   *
   * For graphics pipelines:
   * - Sets root CBVs for b0-b1 (legacy/frequent bindings)
   * - Creates CBV descriptor table for b2+ (less frequent bindings)
   *
   * For compute pipelines:
   * - Creates CBV descriptor table for all bindings
   *
   * @param cmdList Command list to update
   * @param device D3D12 device for descriptor creation
   * @param outResult Optional result for error reporting
   * @return true on success, false on error
   */
  [[nodiscard]] bool updateBufferBindings(ID3D12GraphicsCommandList* cmdList,
                                          ID3D12Device* device,
                                          Result* outResult);

  /**
   * @brief Update UAV bindings (UAV descriptor table for compute shaders)
   *
   * Creates UAV descriptors for all bound storage buffers in the per-frame heap
   * and sets the root descriptor table parameter. Only used for compute pipelines.
   *
   * @param cmdList Command list to update
   * @param device D3D12 device for descriptor creation
   * @param outResult Optional result for error reporting
   * @return true on success, false on error
   */
  [[nodiscard]] bool updateUAVBindings(ID3D12GraphicsCommandList* cmdList,
                                       ID3D12Device* device,
                                       Result* outResult);

  CommandBuffer& commandBuffer_;
  bool isCompute_ = false;

  // Cached binding state
  BindingsTextures bindingsTextures_;
  BindingsSamplers bindingsSamplers_;
  BindingsBuffers bindingsBuffers_;
  BindingsUAVs bindingsUAVs_;

  // Dirty tracking flags
  uint32_t dirtyFlags_ = DirtyFlagBits_Textures | DirtyFlagBits_Samplers |
                         DirtyFlagBits_Buffers | DirtyFlagBits_UAVs;
};

} // namespace igl::d3d12
