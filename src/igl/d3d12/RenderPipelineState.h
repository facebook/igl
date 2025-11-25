/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/RenderPipelineState.h>
#include <igl/d3d12/Common.h>
#include <unordered_map>
#include <array>

namespace igl::d3d12 {

class Device;  // Forward declaration

/**
 * @brief Encapsulates dynamic render state that affects PSO selection
 *
 * Following Vulkan's RenderPipelineDynamicState pattern, this structure serves as a hash key
 * for PSO variant caching. D3D12 PSOs are immutable and must match the exact render target
 * formats at draw time.
 *
 * Key differences from Vulkan:
 * - Vulkan: renderPassIndex_ encodes all render pass compatibility (formats + load/store ops)
 * - D3D12: We only need render target formats (no render pass object exists)
 *
 * The structure is designed for efficient hashing and comparison:
 * - Packed into fixed-size array for fast memcmp
 * - Zero-initialized padding for consistent hashing
 */
struct D3D12RenderPipelineDynamicState {
  // Render target formats (up to 8 MRT targets)
  std::array<DXGI_FORMAT, D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvFormats;
  // Depth-stencil format
  DXGI_FORMAT dsvFormat;

  D3D12RenderPipelineDynamicState() {
    rtvFormats.fill(DXGI_FORMAT_UNKNOWN);
    dsvFormat = DXGI_FORMAT_UNKNOWN;
  }

  bool operator==(const D3D12RenderPipelineDynamicState& other) const {
    return rtvFormats == other.rtvFormats && dsvFormat == other.dsvFormat;
  }

  struct HashFunction {
    size_t operator()(const D3D12RenderPipelineDynamicState& s) const {
      size_t hash = 0;
      for (const auto& fmt : s.rtvFormats) {
        hash ^= std::hash<DXGI_FORMAT>{}(fmt) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
      }
      hash ^= std::hash<DXGI_FORMAT>{}(s.dsvFormat) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
      return hash;
    }
  };
};

class RenderPipelineState final : public IRenderPipelineState {
 public:
  RenderPipelineState(const RenderPipelineDesc& desc,
                      igl::d3d12::ComPtr<ID3D12PipelineState> pipelineState,
                      igl::d3d12::ComPtr<ID3D12RootSignature> rootSignature);
  ~RenderPipelineState() override = default;

  std::shared_ptr<IRenderPipelineReflection> renderPipelineReflection() override;
  void setRenderPipelineReflection(
      const IRenderPipelineReflection& renderPipelineReflection) override;
  int getIndexByName(const igl::NameHandle& name, ShaderStage stage) const override;
  int getIndexByName(const std::string& name, ShaderStage stage) const override;

  // D3D12-specific accessors
  ID3D12PipelineState* getPipelineState() const { return pipelineState_.Get(); }

  /**
   * @brief Get PSO variant for specific render target formats (Vulkan-style dynamic PSO selection)
   *
   * This method follows Vulkan's getVkPipeline(dynamicState) pattern to create PSO variants
   * on-demand based on actual framebuffer formats. D3D12 PSOs are immutable and must exactly
   * match render target formats at creation time.
   *
   * @param dynamicState Contains actual framebuffer RTVformats and DSV format at draw time
   * @param device IGL D3D12 device for PSO creation
   * @return PSO variant matching the requested formats, or nullptr on error
   */
  ID3D12PipelineState* getPipelineState(const D3D12RenderPipelineDynamicState& dynamicState,
                                         Device& device) const;

  ID3D12RootSignature* getRootSignature() const { return rootSignature_.Get(); }
  uint32_t getVertexStride() const { return vertexStride_; }
  uint32_t getVertexStride(size_t slot) const { return (slot < IGL_BUFFER_BINDINGS_MAX) ? vertexStrides_[slot] : 0; }
  D3D_PRIMITIVE_TOPOLOGY getPrimitiveTopology() const { return primitiveTopology_; }

 private:
  friend class Device;  // Device needs access to create PSO variants

  // Base PSO created from RenderPipelineDesc (may not match actual framebuffer formats)
  igl::d3d12::ComPtr<ID3D12PipelineState> pipelineState_;
  igl::d3d12::ComPtr<ID3D12RootSignature> rootSignature_;
  std::shared_ptr<IRenderPipelineReflection> reflection_;
  uint32_t vertexStride_ = 0;
  uint32_t vertexStrides_[IGL_BUFFER_BINDINGS_MAX] = {};
  D3D_PRIMITIVE_TOPOLOGY primitiveTopology_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

  // PSO variant cache following Vulkan's pattern
  // Maps framebuffer formats → PSO variant
  mutable std::unordered_map<D3D12RenderPipelineDynamicState,
                              igl::d3d12::ComPtr<ID3D12PipelineState>,
                              D3D12RenderPipelineDynamicState::HashFunction>
      psoVariants_;
};

} // namespace igl::d3d12
