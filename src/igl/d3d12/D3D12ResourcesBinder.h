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
  size_t elementStrides[IGL_BUFFER_BINDINGS_MAX] = {}; // Byte stride per element for structured
                                                       // buffers
  D3D12_GPU_DESCRIPTOR_HANDLE handles[IGL_BUFFER_BINDINGS_MAX] = {};
  uint32_t count = 0;
};

/**
 * @brief Centralized resource binding management for D3D12 command encoders
 *
 * D3D12ResourcesBinder is the single entry point for shader-visible descriptor binding
 * (CBV/SRV/UAV/Sampler) used by command encoders. It consolidates descriptor allocation
 * and resource binding logic that was previously fragmented across RenderCommandEncoder
 * and ComputeCommandEncoder.
 *
 * Note: RTV/DSV descriptors are managed separately by DescriptorHeapManager and bound
 * directly by encoders during render pass setup.
 *
 * ============================================================================
 * ARCHITECTURE: D3D12 Descriptor Management Overview
 * ============================================================================
 *
 * The D3D12 backend uses THREE distinct descriptor management strategies:
 *
 * 1. **Transient Descriptor Allocator** (Per-Frame Heaps)
 *    - Location: D3D12Context::FrameContext, CommandBuffer allocation methods
 *    - Purpose: Shader-visible descriptors (CBV/SRV/UAV/Samplers) for rendering
 *    - Lifecycle: Allocated during command encoding, reset at frame boundary
 *    - Strategy: Linear allocation with dynamic multi-page growth
 *    - Used for: SRVs (textures), UAVs (storage buffers), CBVs, Samplers
 *    - Access: ONLY through D3D12ResourcesBinder (internal detail)
 *
 * 2. **Persistent Descriptor Allocator** (DescriptorHeapManager)
 *    - Location: DescriptorHeapManager class
 *    - Purpose: CPU-visible descriptors (RTV/DSV) with explicit lifecycle
 *    - Lifecycle: Allocated at resource creation, freed at resource destruction
 *    - Strategy: Free-list allocation with double-free protection
 *    - Used for: Render target views, depth-stencil views
 *    - Access: Directly by Texture and Framebuffer classes
 *
 * 3. **Root Descriptor Optimization** (Inline Binding)
 *    - Location: D3D12ResourcesBinder::updateBufferBindings()
 *    - Purpose: Bypass descriptor heaps for frequently-updated constant buffers
 *    - Lifecycle: No descriptor created - binds GPU virtual address directly
 *    - Strategy: D3D12 root CBVs (graphics b0-b1 only)
 *    - Used for: Hot-path constant buffers in graphics pipeline
 *    - Access: ONLY through D3D12ResourcesBinder (internal optimization)
 *
 * **Design Rationale**:
 * - Strategies 1 and 2 handle DIFFERENT descriptor types (shader-visible vs CPU-visible)
 *   and lifecycles (transient vs persistent), so they cannot be merged
 * - Strategy 3 is a D3D12-specific optimization, not a separate "system"
 * - D3D12ResourcesBinder abstracts these details, providing a unified binding interface
 *
 * ============================================================================
 * Key Responsibilities of D3D12ResourcesBinder
 * ============================================================================
 *
 * - Cache resource bindings locally until updateBindings() is called
 * - Allocate descriptors from per-frame shader-visible heaps on-demand (Strategy 1)
 * - Create SRV/UAV/CBV/Sampler descriptors in GPU-visible heaps
 * - Decide when to use root CBVs vs descriptor tables (Strategy 3)
 * - Track dirty state to minimize descriptor creation and root parameter updates
 * - Support both graphics and compute pipeline bind points
 * - Transition texture resources to appropriate shader-resource states (buffers must
 *   be created in the correct state and are not transitioned here)
 *
 * Design principles:
 * - **Lazy update**: Bindings are cached locally and only applied to GPU on updateBindings()
 * - **Dirty tracking**: Only update descriptor sets when resources change
 * - **Pipeline awareness**: Different root signature layouts for graphics vs compute
 * - **Per-frame isolation**: Uses per-frame descriptor heaps to prevent race conditions
 * - **Implementation hiding**: External code should never directly access CommandBuffer
 *   descriptor allocation methods - always go through ResourcesBinder
 *
 * Thread-safety: This class is NOT thread-safe. Each encoder should own its own binder.
 *
 * Dependencies:
 * - T01: Correct descriptor binding patterns
 * - T06: Shared helper utilities for descriptor creation
 * - T16: Unified logging controls
 * - T20: Consolidated descriptor management architecture
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
   * - **COMPUTE SHADERS**: CBV bindings MUST be dense starting from index 0 with no gaps.
   *   For example, binding slots 0, 1, 2 is valid; binding 0, 2 (skipping 1) will fail.
   *   This constraint is enforced because descriptor tables bind contiguously from b0.
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
   * @param renderPipeline For graphics pipelines: current pipeline to query reflection-based root
   * parameter indices. For compute pipelines: pass nullptr (uses hardcoded layout).
   * @param outResult Optional result for error reporting (e.g., descriptor heap overflow).
   *                  If nullptr, caller receives only success/fail boolean. If non-null,
   *                  all failure paths populate both error code and diagnostic message.
   * @return true if bindings applied successfully, false on error
   */
  [[nodiscard]] bool updateBindings(const class RenderPipelineState* renderPipeline = nullptr,
                                    Result* outResult = nullptr);

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
   * @param renderPipeline Pipeline to query reflection-based root parameter indices (graphics only)
   * @param outResult Optional result for error reporting
   * @return true on success, false on error
   */
  [[nodiscard]] bool updateTextureBindings(ID3D12GraphicsCommandList* cmdList,
                                           ID3D12Device* device,
                                           const class RenderPipelineState* renderPipeline,
                                           Result* outResult);

  /**
   * @brief Update sampler bindings (sampler descriptor table)
   *
   * Creates sampler descriptors for all bound samplers in the per-frame heap
   * and sets the root descriptor table parameter.
   *
   * @param cmdList Command list to update
   * @param device D3D12 device for descriptor creation
   * @param renderPipeline Pipeline to query reflection-based root parameter indices (graphics only)
   * @param outResult Optional result for error reporting
   * @return true on success, false on error
   */
  [[nodiscard]] bool updateSamplerBindings(ID3D12GraphicsCommandList* cmdList,
                                           ID3D12Device* device,
                                           const class RenderPipelineState* renderPipeline,
                                           Result* outResult);

  /**
   * @brief Update buffer bindings (CBV descriptor table)
   *
   * For graphics pipelines:
   * - Creates CBV descriptor table for all bound CBVs
   * - Queries pipeline for reflection-based root parameter index
   *
   * For compute pipelines:
   * - Creates CBV descriptor table for all bindings (hardcoded root parameter)
   *
   * @param cmdList Command list to update
   * @param device D3D12 device for descriptor creation
   * @param renderPipeline Pipeline to query reflection-based root parameter indices (graphics only)
   * @param outResult Optional result for error reporting
   * @return true on success, false on error
   */
  [[nodiscard]] bool updateBufferBindings(ID3D12GraphicsCommandList* cmdList,
                                          ID3D12Device* device,
                                          const class RenderPipelineState* renderPipeline,
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
  uint32_t dirtyFlags_ =
      DirtyFlagBits_Textures | DirtyFlagBits_Samplers | DirtyFlagBits_Buffers | DirtyFlagBits_UAVs;
};

} // namespace igl::d3d12
